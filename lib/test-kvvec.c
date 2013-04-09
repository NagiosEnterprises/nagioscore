#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kvvec.c"
#include "t-utils.h"

static int walking_steps, walks;

static int walker(struct key_value *kv, void *discard)
{
	static struct kvvec *vec = (void *)1;
	static int step;

	walking_steps++;

	if (vec != discard) {
		walks++;
		vec = (struct kvvec *)discard;
		step = 0;
	}

	if (discard && vec) {
		t_ok(!kv_compare(&vec->kv[step], kv), "step %d on walk %d",
			 step, walks);
	}

	step++;

	return 0;
}

#define KVSEP '='
#define PAIRSEP '\0'
#define OVERALLOC 2

static const char *test_data[] = {
	"lala=trudeldudel",
	"foo=bar",
	"LOTS AND LOTS OF CAPS WITH SPACES=weird",
	"key=value",
	"something-random=pre-determined luls",
	"string=with\nnewlines\n\n\nand\nlots\nof\nthem\ntoo\n",
	"tabs=	this	and		that			and three in a row",
	NULL,
};

static const char *pair_term_missing[] = {
	"foo=bar;lul=bar;haha=lulu",
	"foo=bar;lul=bar;haha=lulu;",
	"hobbit=palace;gandalf=wizard1",
	"hobbit=palace;gandalf=wizard1;",
	"0=0;1=1;2=2;3=3;4=4",
	"0=0;1=1;2=2;3=3;4=4;",
	NULL,
};

static void add_vars(struct kvvec *kvv, const char **ary, int len)
{
	int i;

	for (i = 0; i < len && ary[i]; i++) {
		char *arg = strdup(test_data[i]);
		char *eq = strchr(arg, '=');
		if (eq) {
			*eq++ = 0;
		}
		kvvec_addkv(kvv, strdup(arg), eq ? strdup(eq) : NULL);
		free(arg);
	}
}

int main(int argc, char **argv)
{
	int i, j;
	struct kvvec *kvv, *kvv2, *kvv3;
	struct kvvec_buf *kvvb, *kvvb2;
	struct kvvec k = KVVEC_INITIALIZER;

	t_set_colors(0);

	t_start("key/value vector tests");
	kvv = kvvec_create(1);
	ok_int(kvvec_capacity(kvv), 1, "capacity of one should be guaranteed");
	kvv2 = kvvec_create(1);
	kvv3 = kvvec_create(1);
	add_vars(kvv, test_data, 1239819);
	add_vars(kvv, (const char **)argv + 1, argc - 1);

	kvvec_sort(kvv);
	kvvec_foreach(kvv, NULL, walker);

	/* kvvec2buf -> buf2kvvec -> kvvec2buf -> buf2kvvec conversion */
	kvvb = kvvec2buf(kvv, KVSEP, PAIRSEP, OVERALLOC);
	kvv3 = buf2kvvec(kvvb->buf, kvvb->buflen, KVSEP, PAIRSEP, KVVEC_COPY);
	kvvb2 = kvvec2buf(kvv3, KVSEP, PAIRSEP, OVERALLOC);

	buf2kvvec_prealloc(kvv2, kvvb->buf, kvvb->buflen, KVSEP, PAIRSEP, KVVEC_ASSIGN);
	kvvec_foreach(kvv2, kvv, walker);

	kvvb = kvvec2buf(kvv, KVSEP, PAIRSEP, OVERALLOC);

	test(kvv->kv_pairs == kvv2->kv_pairs, "pairs should be identical");

	for (i = 0; i < kvv->kv_pairs; i++) {
		struct key_value *kv1, *kv2;
		kv1 = &kvv->kv[i];
		if (i >= kvv2->kv_pairs) {
			t_fail("missing var %d in kvv2", i);
			printf("[%s=%s] (%d+%d)\n", kv1->key, kv1->value, kv1->key_len, kv1->value_len);
			continue;
		}
		kv2 = &kvv2->kv[i];
		if (!test(!kv_compare(kv1, kv2), "kv pair %d must match", i)) {
			printf("%d failed: [%s=%s] (%d+%d) != [%s=%s (%d+%d)]\n",
				   i,
				   kv1->key, kv1->value, kv1->key_len, kv1->value_len,
				   kv2->key, kv2->value, kv2->key_len, kv2->value_len);
		}
	}

	test(kvvb2->buflen == kvvb->buflen, "buflens must match");
	test(kvvb2->bufsize == kvvb->bufsize, "bufsizes must match");

	if (kvvb2->buflen == kvvb->buflen && kvvb2->bufsize == kvvb->bufsize &&
		!memcmp(kvvb2->buf, kvvb->buf, kvvb->bufsize))
	{
		t_pass("kvvec -> buf -> kvvec conversion works flawlessly");
	} else {
		t_fail("kvvec -> buf -> kvvec conversion failed :'(");
	}

	free(kvvb->buf);
	free(kvvb);
	free(kvvb2->buf);
	free(kvvb2);
	kvvec_destroy(kvv, 1);
	kvvec_destroy(kvv3, KVVEC_FREE_ALL);

	for (j = 0; pair_term_missing[j]; j++) {
		buf2kvvec_prealloc(&k, strdup(pair_term_missing[j]), strlen(pair_term_missing[j]), '=', ';', KVVEC_COPY);
		for (i = 0; i < k.kv_pairs; i++) {
			struct key_value *kv = &k.kv[i];
			test(kv->key_len == kv->value_len, "%d.%d; key_len=%d; value_len=%d (%s = %s)",
				 j, i, kv->key_len, kv->value_len, kv->key, kv->value);
			test(kv->value_len == (int)strlen(kv->value),
				 "%d.%d; kv->value_len(%d) == strlen(%s)(%d)",
				 j, i, kv->value_len, kv->value, (int)strlen(kv->value));
		}
	}

	t_end();
	return 0;
}
