#ifndef JSON_H
#define JSON_H

#include "utils.h"


typedef enum {
    JSON_OBJ,
    JSON_STR,
    JSON_ARR,
    JSON_NUM,
} json_type_t;

typedef struct json {
    json_type_t typ;
    union {
        struct {
            const char *key;
            const struct json *val;
        } *obj;
        const char *str;
        const da_t *arr;
        double num;
    } val;
} json_t;

// Parse string to json
const json_t *json_parse(const char* str);

// Write printable json to stream
void json_print(const json_t *node, FILE *s);

// Free json object
void json_free(const json_t *node);

#endif // JSON_H
