/*
 * Nagios Event Radio Dispatcher
 *
 * This is a subscriber service which initiates a unix domain socket,
 * listens to it and lets other programs connect to it and subscribe
 * to various channels using a simple text-based syntax.
 *
 * This code uses the eventbroker api to get its data, which means
 * we're finally eating our own dogfood in that respect.
 *
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#include "include/config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "lib/libnagios.h"
#include "include/common.h"
#include "include/objects.h"
#include "include/broker.h"
#include "include/nebmods.h"
#include "include/nebmodules.h"
#include "include/nebstructs.h"

static unsigned int max_subscribers = 32; /* make this configurable */
static unsigned int num_subscribers;
static nebmodule *nerd_mod; /* fake module to get our callbacks accepted */
static int nerd_sock; /* teehee. nerd-socks :D */

struct subscriber {
	int sd;
	objectlist *subscriptions; /* subscriptions taken out */
};

struct nerd_channel {
	const char *name; /* name of this channel */
	unsigned int id; /* channel id (might vary between invocations) */
	unsigned int num_subscribers; /* subscriber refcount */
	unsigned int required_options; /* event_broker_options required for this channel */
	unsigned int num_callbacks;
	unsigned int callbacks[NEBCALLBACK_NUMITEMS];
	int (*handler)(int , void *); /* callback handler for this channel */
	objectlist *subscriptions; /* subscriber list */
};

struct subscription {
	struct subscriber *sub;
	struct nerd_channel *chan;
	char *format; /* requested format (macro string) for this subscription */
};

static unsigned int chan_host_checks_id, chan_service_checks_id;

static struct nerd_channel **channels;
static unsigned int num_channels, alloc_channels;


static struct nerd_channel *find_channel(const char *name)
{
	int i;

	for(i = 0; i < num_channels; i++) {
		struct nerd_channel *chan = channels[i];
		if(!strcmp(name, chan->name)) {
			return chan;
		}
	}

	return NULL;
}

static int nerd_register_channel_callbacks(struct nerd_channel *chan)
{
	int i;

	for(i = 0; i < chan->num_callbacks; i++) {
		int result = neb_register_callback(chan->callbacks[i], nerd_mod, 0, chan->handler);
		if(result != 0) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to register callback %d for channel '%s': %d\n",
				  chan->callbacks[i], chan->name, result);
			return -1;
		}
	}
	return 0;
}

static int nerd_deregister_channel_callbacks(struct nerd_channel *chan)
{
	int i;

	for(i = 0; i < chan->num_callbacks; i++) {
		neb_deregister_callback(chan->callbacks[i], chan->handler);
	}
	return 0;
}
	
static int subscribe(int sd, struct nerd_channel *chan, struct subscriber *sub, char *fmt)
{
	struct subscription *subscr;

	subscr = malloc(sizeof(*subscr));
	if(!subscr)
		return -1;

	subscr->chan = chan;
	subscr->sub = sub;
	subscr->format = fmt ? strdup(fmt) : NULL;

	prepend_object_to_objectlist(&sub->subscriptions, subscr);
	if(!chan->subscriptions) {
		nerd_register_channel_callbacks(chan);
		prepend_object_to_objectlist(&chan->subscriptions, subscr);
		return 0;
	}

	prepend_object_to_objectlist(&chan->subscriptions, subscr);
	return 0;
}

static int cancel_channel_subscription(struct subscription *subscr)
{
	objectlist *list, *next, *prev = NULL;
	struct nerd_channel *chan;

	if(!subscr)
		return -1;

	chan = subscr->chan;
	logit(NSLOG_INFO_MESSAGE, TRUE, "Cancelling channel subscription '%s' for subscriber %d\n",
		  chan->name, subscr->sub->sd);
	for(list = chan->subscriptions; list; list = next) {
		next = list->next;
		if(list->object_ptr != subscr)
			continue;
		free(list);
		if(prev) {
			prev->next = next;
			return 0;
		}
		chan->subscriptions = next;
		break;
	}

	if(chan->subscriptions == NULL)
		nerd_deregister_channel_callbacks(chan);

	return 0;
}

static int unsubscribe(struct nerd_channel *chan, struct subscriber *sub, const char *fmt)
{
	objectlist *list, *next, *prev;

	if(!sub)
		return -1;

	for(list = chan->subscriptions; list; list = next) {
		struct subscription *subscr = (struct subscription *)list->object_ptr;
		next = list->next;
		if(subscr->sub != sub || (fmt && !subscr->format) || (!fmt && subscr->format)) {
			prev = list;
			continue;
		}
		if(fmt && strcmp(fmt, subscr->format))
			continue;

		/* found it, so remove it */
		free(subscr);
		free(list);
		if(!prev) {
			chan->subscriptions = next;
			continue;
		}
		prev->next = next;
	}
	if(chan->subscriptions == NULL) {
		nerd_deregister_channel_callbacks(chan);
	}
	return 0;
}

/* removes a subscriber entirely and closes its socket */
static int cancel_subscriber(struct subscriber *sub)
{
	objectlist *list, *next;

	if(!sub)
		return -1;

	for(list = sub->subscriptions; list; list = next) {
		struct subscription *subscr = (struct subscription *)list->object_ptr;
		next = list->next;
		free(list);
		cancel_channel_subscription(subscr);
	}

	num_subscribers--;
	iobroker_close(nagios_iobs, sub->sd);
	free(sub);

	return 0;
}

static int broadcast(unsigned int chan_id, void *buf, unsigned int len)
{
	struct nerd_channel *chan;
	objectlist *list, *next;

	if(chan_id < 0 || chan_id >= num_channels)
		return -1;

	chan = channels[chan_id];

	if(!chan->subscriptions) {
		nerd_deregister_channel_callbacks(chan);
		return 0;
	}

	for(list = chan->subscriptions; list; list = next) {
		struct subscription *subscr = (struct subscription *)list->object_ptr;
		int result;

		next = list->next;

		result = send(subscr->sub->sd, buf, len, MSG_NOSIGNAL);
		if(result < 0 && errno == EPIPE) {
			cancel_subscriber(subscr->sub);
		}
	}

	return 0;
}


#define prefixcmp(buf, find) strncmp(buf, find, strlen(find))
#define NERD_SUBSCRIBE   0
#define NERD_UNSUBSCRIBE 1
static int nerd_input_handler(int sd, int events, void *sub_)
{
	struct subscriber *sub = (struct subscriber *)sub_;
	struct nerd_channel *chan;
	char request[2048], *chan_name, *fmt;
	int result, action = NERD_SUBSCRIBE;

	result = recv(sd, request, sizeof(request), MSG_NOSIGNAL);
	printf("nerd_input(): recv() returned %d: %m\n", result);
	if(result <= 0) {
		cancel_subscriber(sub);
		return 0;
	}
	request[result--] = 0;
	printf("request: %s\n", request);
	while(request[result] == '\n')
		request[result--] = 0;
	if(!result) /* empty request */
		return 0;

	chan_name = strchr(request, ' ');
	if(result <= 0 || !chan_name) {
		iobroker_close(nagios_iobs, sd);
	}

	*chan_name = 0;
	chan_name++;
	if(strcmp(request, "subscribe ") && strcmp(request, "unsubscribe"))
		action = NERD_SUBSCRIBE;
	else if(!strcmp(request, "unsubscribe "))
		action = NERD_UNSUBSCRIBE;
	else {
		nsock_printf(sd, "Bad request. Do things right or go away\n");
		iobroker_close(nagios_iobs, sd);
	}

	/* might have a format-string */
	fmt = strchr(chan_name, ':');
	if(fmt) {
		*fmt = 0;
		fmt++;
	}

	chan = find_channel(chan_name);
	if(!chan) {
		nsock_printf(sd, "Invalid request");
		return 0;
	}

	if(action == NERD_SUBSCRIBE)
		subscribe(sd, chan, sub, fmt);
	else
		unsubscribe(chan, sub, fmt);

	return 0;
}

static int nerd_tuneins(int in_sock, int events, void *discard)
{
	int sd;
	struct sockaddr sa;
	socklen_t slen;
	struct subscriber *sub;

	sd = accept(in_sock, &sa, &slen);
	printf("NERD tune-in discovered from %d (%m)\n", sd);
	printf("num_subscribers: %u; max_subscribers: %u\n",
		   num_subscribers, max_subscribers);

	if(sd < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to accept() NERD tune-in: %s\n",
			  strerror(errno));
		return 0;
	}

	if(num_subscribers >= max_subscribers) {
		nsock_printf(sd, "We're full. Go away\n");
		close(sd);
		iobroker_unregister(nagios_iobs, sd);
	}

	sub = calloc(1, sizeof(*sub));
	num_subscribers++;
	sub->sd = sd;
	iobroker_register(nagios_iobs, sd, sub, nerd_input_handler);

	return 0;
}

static int chan_host_checks(int cb, void *data)
{
	nebstruct_host_check_data *ds = (nebstruct_host_check_data *)data;
	check_result *cr = (check_result *)ds->check_result_ptr;
	host *h;
	char *buf;

	if(ds->type != NEBTYPE_HOSTCHECK_PROCESSED)
		return 0;

	if(channels[chan_host_checks_id]->subscriptions == NULL)
		return 0;

	h = (host *)ds->object_ptr;
	asprintf(&buf, "%s from %d -> %d: %s\n", h->name, h->last_state, h->current_state, cr->output);
	broadcast(chan_host_checks_id, buf, strlen(buf));
	free(buf);
	return 0;
}

static int chan_service_checks(int cb, void *data)
{
	nebstruct_service_check_data *ds = (nebstruct_service_check_data *)data;
	check_result *cr = (check_result *)ds->check_result_ptr;
	service *s;
	char *buf;

	if(ds->type != NEBTYPE_SERVICECHECK_PROCESSED)
		return 0;
	s = (service *)ds->object_ptr;
	asprintf(&buf, "%s;%s from %d -> %d: %s\n", s->host_name, s->description, s->last_state, s->current_state, cr->output);
	broadcast(chan_service_checks_id, buf, strlen(buf));
	free(buf);
	return 0;
}

static int nerd_deinit(void)
{
	unsigned int i;

	iobroker_close(nagios_iobs, nerd_sock);

	for(i = 0; i < num_channels; i++) {
		struct nerd_channel *chan = channels[i];
		objectlist *list, *next;

		for(list = chan->subscriptions; list; list = next) {
			struct subscriber *sub = (struct subscriber *)list->object_ptr;
			next = list->next;
			free(list);
			sub->sd = -1;
		}
	}
	free(nerd_mod);
	return 0;
}

int nerd_mkchan(const char *name, int (*handler)(int, void *), unsigned int callbacks)
{
	struct nerd_channel *chan, **ptr;
	int i;

	if(num_channels + 1 >= alloc_channels) {
		alloc_channels = alloc_nr(alloc_channels);
		ptr = realloc(channels, alloc_channels * sizeof(struct nerd_channel *));
		if(!ptr)
			return -1;
		channels = ptr;
	}

	if(!(chan = calloc(1, sizeof(*chan))))
		return -1;

	chan->name = name;
	chan->handler = handler;
	for(i = 0; callbacks && i < NEBCALLBACK_NUMITEMS; i++) {
		if(!(callbacks & (1 << i)))
			continue;

		chan->callbacks[chan->num_callbacks++] = i;
		printf("Added callback() %d for channel '%s'\n", i, name);
	}

	channels[num_channels++] = chan;

	logit(NSLOG_INFO_MESSAGE, TRUE, "NERD channel %s registered successfully\n", chan->name);
	return num_channels - 1;
}

/* nebmod_init(), but loaded even if no modules are */
int nerd_init(void)
{
	nerd_mod = calloc(1, sizeof(*nerd_mod));
	nerd_mod->deinit_func = nerd_deinit;

	neb_add_core_module(nerd_mod);

	nerd_sock = nsock_unix("/tmp/nerd.sock", 07, NSOCK_TCP | NSOCK_UNLINK);
	printf("nsock_unix() returned %d: %m\n", nerd_sock);
	iobroker_register(nagios_iobs, nerd_sock, NULL, nerd_tuneins);
	chan_host_checks_id = nerd_mkchan("hostchecks", chan_host_checks, nebcallback_flag(NEBCALLBACK_HOST_CHECK_DATA));
	chan_service_checks_id = nerd_mkchan("servicechecks", chan_service_checks, nebcallback_flag(NEBCALLBACK_SERVICE_CHECK_DATA));

	return 0;
}
