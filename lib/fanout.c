#include <unistd.h>
#include "fanout.h"

struct fanout_entry {
	unsigned long key;
	void *data;
	struct fanout_entry *next;
};

struct fanout_table {
	unsigned long alloc;
	struct fanout_entry **entries;
};

fanout_table *fanout_create(unsigned long size)
{
	fanout_table *t = calloc(1, sizeof(*t));
	if (!t)
		return NULL;
	t->entries = calloc(size, sizeof(struct fanout_entry *));
	if (!t->entries) {
		free(t);
		return NULL;
	}
	t->alloc = size;
	return t;
}

void fanout_destroy(fanout_table *t, void (*destructor)(void *))
{
	unsigned long i;

	if (!t || !t->entries || !t->alloc)
		return;

	for (i = 0; i < t->alloc; i++) {
		struct fanout_entry *entry;
		for (entry = t->entries[i]; entry; entry = entry->next) {
			if (destructor) {
				destructor(entry->data);
			}
			free(entry);
		}
		t->entries[i] = NULL;
	}
	free(t->entries);
	free(t);
}

int fanout_add(struct fanout_table *t, unsigned long key, void *data)
{
	struct fanout_entry *entry;
	if (!t || !t->entries || !t->alloc || !data)
		return -1;

	entry = calloc(1, sizeof(*entry));
	if (!entry)
		return -1;

	entry->key = key;
	entry->data = data;
	entry->next = t->entries[key % t->alloc];
	t->entries[key % t->alloc] = entry;

	return 0;
}

void *fanout_remove(fanout_table *t, unsigned long key)
{
	struct fanout_entry *entry, *prev = NULL;
	unsigned long slot;
	if (!t || !t->entries || !t->alloc)
		return NULL;

	slot = key % t->alloc;
	for (entry = t->entries[slot]; entry; prev = entry, entry = entry->next) {
		if (entry->key == key) {
			void *data = entry->data;
			if (prev) {
				prev->next = entry->next;
			} else {
				t->entries[slot] = entry->next;
			}
			free(entry);
			return data;
		}
	}
	return NULL;
}
