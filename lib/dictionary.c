#include <string.h>
#include "dictionary.h"

#ifndef DICTIONARY_HASHSIZE
#define DICTIONARY_HASHSIZE
#endif

static unsigned int dictionaryrecord_hash(char *key) {
    unsigned int keyHash;
    for (keyHash = 0; *key != '\0'; key++) {
        keyHash = *key + 997 * keyHash;
    }

    return keyHash % DICTIONARY_HASHSIZE;
}

dictionaryrecord *dictionaryrecord_find_by_key(dictionaryrecord *dictionary[], char *key) {
    dictionaryrecord *record = NULL;
    for (record = dictionary[dictionaryrecord_hash(key)]; record != NULL; record = record->next) {
        if (strcmp(key, record->key) == 0) return record; 
    }
    return record; 
}

dictionaryrecord *dictionaryrecord_write(dictionaryrecord *dictionary[], char *key, char *value) {
    dictionaryrecord *record = NULL;
    unsigned keyHash;
    
    record = dictionaryrecord_find_by_key(dictionary, key);

    if (record == NULL) {
        record = (dictionaryrecord *) malloc(sizeof(record));
        if ((record == NULL || (record->key = strdup(key)) == NULL)) {
            return record;
        }
        keyHash = dictionaryrecord_hash(key);
        record->next = dictionary[keyHash];
        dictionary[keyHash] = record;
    }
    
    if ((record->value = strdup(value)) == NULL) {
        return record;
    }

    return record;
}


