#include <stdio.h>
#include <stdlib.h>
#include "dictionary.c"
#include "t-utils.h"

// Helper struct for populating key/value pairs in dictionaryrecord struct
typedef struct keyvalue {
    char *key;
    char *value;
} keyvalue;

// Prepopulate array ( { key, value } )
keyvalue myvalues[] = {
    { "G_MACRO_DICTIONARY_KEY" , "Life_Saving_VALUE" },
    { "A_DIFFERENT_DICTIONARY" , "Day_Saving_VALUE" },
    { "Simple Key" , "Simple Value" },
    { "Batman" , "Bruce Wayne" },
};

static keyvalue *add_vars(const char *argv) {
    keyvalue *kv = malloc(sizeof(keyvalue *));

    char *key = strdup(argv);
    char *value = strchr(key, '=');
    if (value) {
        *value++ = 0;
    } 
    kv->key = strdup(key);
    kv->value = strdup(value);
    free(key);

    return kv;
} 

int main(int argc, char *argv[])
{
    int i = 0;
    // Generally this is set in a global space, but for tesing we will allocate
    // the pointer in the program main scope to make it self contained.
    dictionaryrecord *library[DICTIONARY_HASHSIZE];

    t_set_colors(1);

    t_start("Dictionary Read/Write Tests");

    for (i = 0; i < sizeof(myvalues) / (2 * sizeof(char *)); i++) {
        dictionaryrecord_write(library, myvalues[i].key, myvalues[i].value);
    }    

    for (i = 1; i < argc; i++) {
        keyvalue *kv = add_vars(argv[i]);
        dictionaryrecord_write(library, kv->key, kv->value);
        free(kv->key);
        free(kv->value);
        free(kv);
    }

    dictionaryrecord *record = dictionaryrecord_find_by_key(library, "Batman");
    t_ok(0 == strcmp("Bruce Wayne", record->value), "Values Must Match");
    record = dictionaryrecord_find_by_key(library, "Simple Key");
    t_ok(0 == strcmp("Simple Key", record->key), "Keys Must Match");

    t_diag("The erase record for key '%s' is:\n"
           "\tKey:   '%s'\n"
           "\tValue: '%s'", 
           record->key, 
           record->key, 
           record->value);

    dictionaryrecord_erase(library, "Simple Key");
    t_ok(NULL == record->key, "This key should now be unset and NULL");
    t_ok(NULL == record->value, "This value should now be unset and NULL");


    // TODO: Place input tests (argc/argv) here if/when necessary.
    //

    t_end();
    return 0;
}

