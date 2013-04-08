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
	struct fanout_entry **entries, *next;

	if (!t || !t->entries || !t->alloc)
		return;

	/* protect against destructors calling fanout_remove() */
	entries = t->entries;
	t->entries = NULL;

	for (i = 0; i < t->alloc; i++) {
		struct fanout_entry *entry;

		for (entry = entries[i]; entry; entry = next) {
			void *data = entry->data;
			next = entry->next;
			free(entry);
			if (destructor) {
				destructor(data);
			}
		}
	}
	free(entries);
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

void *fanout_get(fanout_table *t, unsigned long key)
{
	struct fanout_entry *entry;
	unsigned long slot;

	if (!t || !t->entries || !t->alloc)
		return NULL;

	slot = key % t->alloc;
	for (entry = t->entries[slot]; entry; entry = entry->next) {
		if (entry->key == key)
			return entry->data;
	}

	return NULL;
}

void *fanout_remove(fanout_table *t, unsigned long key)
{
	struct fanout_entry *entry, *next, *prev = NULL;
	unsigned long slot;

	if (!t || !t->entries || !t->alloc)
		return NULL;

	slot = key % t->alloc;
	for (entry = t->entries[slot]; entry; prev = entry, entry = next) {
		next = entry->next;
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
