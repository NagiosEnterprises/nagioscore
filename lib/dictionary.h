#ifndef LIBNAGIOS_dictionary_h__
#define LIBNAGIOS_dictionary_h__

/**
 * @file dictionary.h
 * @brief Dictionary API
 * 
 * The dictionary API is used to store key=value pairs (see resource.cfg)
 * @{
 */
#ifndef DICTIONARY_HASHSIZE
#define DICTIONARY_HASHSIZE 16384
#endif

struct dictionaryrecord;
typedef struct dictionaryrecord {
    struct dictionaryrecord *next;
    char *key;
    char *value;
} dictionaryrecord;

extern dictionaryrecord *dictionaryrecord_find_by_key(dictionaryrecord *dictionary[], char *key);
extern dictionaryrecord *dictionaryrecord_write(dictionaryrecord *dictionary[], char *key, char *value);


/* @}
 */
#endif /* LIBNAGIOS_dictionary_h__ */
