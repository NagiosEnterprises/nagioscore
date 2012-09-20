/*
 * Nagios Event Radio Dispatcher
 *
 * This is a subscriber service which initiates a unix domain socket,
 * listens to it and lets other programs connect to it and subscribe
 * to various channels using a simple text-based syntax.
 *
 * This code uses the eventbroker api to get its data, which means
 * we're finally eating our own dogfood in that respect.
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

struct nerd_channel {
	const char *name; /* name of this channel */
	unsigned int id; /* channel id (might vary between invocations) */
	unsigned int required_options; /* event_broker_options required for this channel */
	unsigned int num_callbacks;
	unsigned int callbacks[NEBCALLBACK_NUMITEMS];
	int (*handler)(int , void *); /* callback handler for this channel */
	objectlist *subscriptions; /* subscriber list */
};

struct subscription {
	int sd;
	struct nerd_channel *chan;
	char *format; /* requested format (macro string) for this subscription */
};


static nebmodule *nerd_mod; /* fake module to get our callbacks accepted */
static int nerd_sock; /* teehee. nerd-socks :D */
static struct nerd_channel **channels;
static unsigned int num_channels, alloc_channels;
static unsigned int chan_host_checks_id, chan_service_checks_id;


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
	
static int subscribe(int sd, struct nerd_channel *chan, char *fmt)
{
	struct subscription *subscr;

	if(!(subscr = calloc(1, sizeof(*subscr))))
		return -1;

	subscr->sd = sd;
	subscr->chan = chan;
	subscr->format = fmt ? strdup(fmt) : NULL;

	if(!chan->subscriptions) {
		nerd_register_channel_callbacks(chan);
	}

	prepend_object_to_objectlist(&chan->subscriptions, subscr);
	return 0;
}

static int cancel_channel_subscription(struct nerd_channel *chan, int sd)
{
	objectlist *list, *next, *prev = NULL;
	int cancelled = 0;

	if(!chan)
		return -1;

	for(list = chan->subscriptions; list; list = next) {
		struct subscription *subscr = (struct subscription *)list->object_ptr;
		next = list->next;

		if(subscr->sd == sd) {
			cancelled++;
			free(list);
			if(prev) {
				prev->next = next;
			} else {
				chan->subscriptions = next;
			}
			continue;
		}
		prev = list;
	}

	if(cancelled) {
		logit(NSLOG_INFO_MESSAGE, TRUE, "Cancelled %d subscription%s to channel '%s' for %d\n",
			  cancelled, cancelled == 1 ? "" : "s", chan->name, sd);
	}

	if(chan->subscriptions == NULL)
		nerd_deregister_channel_callbacks(chan);

	return 0;
}

static int unsubscribe(int sd, struct nerd_channel *chan, const char *fmt)
{
	objectlist *list, *next, *prev = NULL;

	for(list = chan->subscriptions; list; list = next) {
		struct subscription *subscr = (struct subscription *)list->object_ptr;
		next = list->next;
		if(subscr->sd == sd) {
			/* found it, so remove it */
			free(subscr);
			free(list);
			if(!prev) {
				chan->subscriptions = next;
				continue;
			} else {
				prev->next = next;
			}
			continue;
		}
		prev = list;
	}

	if(chan->subscriptions == NULL) {
		nerd_deregister_channel_callbacks(chan);
	}
	return 0;
}

/* removes a subscriber entirely and closes its socket */
static int cancel_subscriber(int sd)
{
	int i;

	for(i = 0; i < num_channels; i++) {
		cancel_channel_subscription(channels[i], sd);
	}

	iobroker_close(nagios_iobs, sd);
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

		result = send(subscr->sd, buf, len, MSG_NOSIGNAL);
		if(result < 0) {
			if (errno == EAGAIN)
				return 0;

			cancel_subscriber(subscr->sd);
			return 500;
		}
	}

	return 0;
}


static int chan_host_checks(int cb, void *data)
{
	nebstruct_host_check_data *ds = (nebstruct_host_check_data *)data;
	check_result *cr = (check_result *)ds->check_result_ptr;
	host *h;
	char *buf;
	int len;

	if(ds->type != NEBTYPE_HOSTCHECK_PROCESSED)
		return 0;

	if(channels[chan_host_checks_id]->subscriptions == NULL)
		return 0;

	h = (host *)ds->object_ptr;
	len = asprintf(&buf, "%s from %d -> %d: %s\n", h->name, h->last_state, h->current_state, cr->output);
	broadcast(chan_host_checks_id, buf, len);
	free(buf);
	return 0;
}

static int chan_service_checks(int cb, void *data)
{
	nebstruct_service_check_data *ds = (nebstruct_service_check_data *)data;
	check_result *cr = (check_result *)ds->check_result_ptr;
	service *s;
	char *buf;
	int len;

	if(ds->type != NEBTYPE_SERVICECHECK_PROCESSED)
		return 0;
	s = (service *)ds->object_ptr;
	len = asprintf(&buf, "%s;%s from %d -> %d: %s\n", s->host_name, s->description, s->last_state, s->current_state, cr->output);
	broadcast(chan_service_checks_id, buf, len);
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
			struct subscription *subscr = (struct subscription *)list->object_ptr;
			next = list->next;
			free(list);
			free(subscr);
		}
		chan->subscriptions = NULL;
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
	}

	channels[num_channels++] = chan;

	logit(NSLOG_INFO_MESSAGE, TRUE, "NERD channel %s registered successfully\n", chan->name);
	return num_channels - 1;
}

#define NERD_SUBSCRIBE   0
#define NERD_UNSUBSCRIBE 1
static int nerd_qh_handler(int sd, char *request, unsigned int len)
{
	char *chan_name, *fmt;
	struct nerd_channel *chan;
	int action;

	printf("Got request '%s'\n", request);
	while(request[len] == 0 || request[len] == '\n')
		request[len--] = 0;
	chan_name = strchr(request, ' ');
	if(!chan_name)
		return 400;

	*chan_name = 0;
	chan_name++;
	if(!strcmp(request, "subscribe"))
		action = NERD_SUBSCRIBE;
	else if(!strcmp(request, "unsubscribe"))
		action = NERD_UNSUBSCRIBE;
	else {
		return 400;
	}

	/* might have a format-string */
	if((fmt = strchr(chan_name, ':')))
		*(fmt++) = 0;

	chan = find_channel(chan_name);
	if(!chan) {
		printf("Failed to find channel %s\n", chan_name);
		return 400;
	}

	if(action == NERD_SUBSCRIBE)
		subscribe(sd, chan, fmt);
	else
		unsubscribe(sd, chan, fmt);

	return 0;
}

/* nebmod_init(), but loaded even if no modules are */
int nerd_init(void)
{
	nerd_mod = calloc(1, sizeof(*nerd_mod));
	nerd_mod->deinit_func = nerd_deinit;

	if(qh_register_handler("nerd", 0, nerd_qh_handler) < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to register 'nerd' with query handler\n");
		return ERROR;
	}

	neb_add_core_module(nerd_mod);

	chan_host_checks_id = nerd_mkchan("hostchecks", chan_host_checks, nebcallback_flag(NEBCALLBACK_HOST_CHECK_DATA));
	chan_service_checks_id = nerd_mkchan("servicechecks", chan_service_checks, nebcallback_flag(NEBCALLBACK_SERVICE_CHECK_DATA));

	logit(NSLOG_INFO_MESSAGE, TRUE, "NERD initialized and ready to rock!\n");
	return 0;
}
