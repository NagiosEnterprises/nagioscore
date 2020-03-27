#include <string.h>
#include "dictionary.h"

/**
 * @function static dictionaryrecord_hash()
 * @param char *key
 *
 * Take a key for a record and turn it into an int based on its characters.
 * Note: Adding a prime to the keyHash modulo is to avoid duplicates.
 *
 * @return unsigned int
 */
static unsigned int dictionaryrecord_hash(char *key) {
    unsigned int keyHash;
    for (keyHash = 0; *key != '\0'; key++) {
        keyHash = *key + 997 * keyHash;
    }

    return keyHash % DICTIONARY_HASHSIZE;
}

/**
 * @function dictionary_find_by_key()
 * @param dictionaryrecord *dictionary[] - global dictionary pointer to store key/values
 * @param char *key
 *
 * Takes a key and attempts to find a corresponding record. If none return NULL.
 *
 * @return dictionaryrecord *record | NULL;
 */
dictionaryrecord *dictionaryrecord_find_by_key(dictionaryrecord *dictionary[], char *key) {
    dictionaryrecord *record = NULL;
    for (record = dictionary[dictionaryrecord_hash(key)]; record != NULL; record = record->next) {
        if (strcmp(key, record->key) == 0) return record; 
    }
    return record; 
}

/**
 * @function dictionary_write()
 * @param dictionaryrecord *dictionary[] - global dictionary pointer 
 *                                         to store key/values
 * @param char *key
 * @param char *value
 *
 * Takes a key and value and writes a record to dictionary. 
 * Overwrites value if key is repeated.
 *
 * @return dictionaryrecord *record | NULL;
 */
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

/**
 * @function dictionaryrecord_erase()
 * @param dictionaryrecord *dictionary[] - global dictionary pointer 
 *                                         to store key/values
 * @param char *key
 * 
 * Remove a specific record by key.
 *
 * return (void)
 */
void dictionaryrecord_erase(dictionaryrecord *dictionary[], char *key) {
    dictionaryrecord *record = dictionaryrecord_find_by_key(dictionary, key);
    if (record) {
        free(record->key);
        record->key = NULL;
        free(record->value);
        record->value = NULL;
        free(record);
        record = NULL;
    }  
}

/**
 * @function dictionaryrecord_free()
 * @param dictionaryrecord *dictionary[] - global dictionary pointer 
 *                                         to store key/values
 * @param char *key
 * 
 * Remove a specific record by keyhash.
 *
 * return (void)
 */
void dictionaryrecord_free(dictionaryrecord *dictionary[], int keyhash) {
    if (dictionary[keyhash]) {
        free(dictionary[keyhash]->key);
        dictionary[keyhash]->key = NULL;
        free(dictionary[keyhash]->value);
        dictionary[keyhash]->value = NULL;
        free(dictionary[keyhash]);
        dictionary[keyhash] = NULL;
    }
}


