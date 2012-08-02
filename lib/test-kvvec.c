#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kvvec.c"

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
		if (!kv_compare(&vec->kv[step], kv))
			printf(" PASS: step %d on walk %d matches\n", step, walks);
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
	int i;
	struct kvvec *kvv, *kvv2;
	struct kvvec_buf *kvvb, *kvvb2;

	kvv = kvvec_create(1);
	add_vars(kvv, test_data, 1239819);
	add_vars(kvv, (const char **)argv + 1, argc - 1);

	kvvec_sort(kvv);
	kvvec_foreach(kvv, NULL, walker);

	/* kvvec2buf -> buf2kvvec -> kvvec2buf -> buf2kvvec conversion */
	kvvb = kvvec2buf(kvv, KVSEP, PAIRSEP, OVERALLOC);
	kvv2 = buf2kvvec(kvvb->buf, kvvb->buflen, KVSEP, PAIRSEP);
	kvvec_foreach(kvv2, kvv, walker);
	kvvb2 = kvvec2buf(kvv2, KVSEP, PAIRSEP, OVERALLOC);

	if (kvv->kv_pairs != kvv2->kv_pairs) {
		printf("Failure: kvvec2buf -> buf2kvvec fails to get pairs teamed up (delta: %d)\n",
			   kvv->kv_pairs - kvv2->kv_pairs);
	}

	for (i = 0; i < kvv->kv_pairs; i++) {
		struct key_value *kv1, *kv2;
		kv1 = &kvv->kv[i];
		if (i >= kvv2->kv_pairs) {
			printf("%d failed: Not present in kvv2\n", i);
			printf("[%s=%s] (%d+%d)\n", kv1->key, kv1->value, kv1->key_len, kv1->value_len);
			continue;
		}
		kv2 = &kvv2->kv[i];
		if (kv_compare(kv1, kv2)) {
			printf("%d failed: [%s=%s] (%d+%d) != [%s=%s (%d+%d)]\n",
				   i,
				   kv1->key, kv1->value, kv1->key_len, kv1->value_len,
				   kv2->key, kv2->value, kv2->key_len, kv2->value_len);
		}
	}
	if (kvvb2->buflen == kvvb->buflen && kvvb2->bufsize == kvvb->bufsize &&
		!memcmp(kvvb2->buf, kvvb->buf, kvvb->bufsize))
	{
		printf("PASS: kvvec -> buf -> kvvec conversion works flawlessly\n");
	} else {
		printf("FAIL: kvvec -> buf -> kvvec conversion failed :'(\n");
		return EXIT_FAILURE;
	}

	free(kvvb->buf);
	free(kvvb);
	kvvec_destroy(kvv, 1);
	return 0;
}
