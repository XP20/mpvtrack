#define _POSIX_C_SOURCE 200809L
#include "json.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>


#define SKIPWS for(; p != e && isspace(*p); p++);
static char *p, *e;

static const char *parsestr() {
    assert(*p == '"');
    char *begin = ++p;
    size_t count = strchr(begin, '"') - begin;
    p += count + 1;
    return strndup(begin, count);
}

static const json_t *parse() {
    json_t *node = malloc(sizeof(json_t));

    SKIPWS;
    switch (*p) {
        case '{':
            node->typ = JSON_OBJ;
            node->val.obj = NULL;
            do {
                p++;
                SKIPWS; const char *key = parsestr();
                SKIPWS; assert(*p == ':');
                p++;
                SKIPWS; const json_t *val = parse();
                SKIPWS; hm_put(node->val.obj, key, val);
                assert(*p == '}' || *p == ',');
            } while (*p == ',');
            assert(*p == '}');
            p++;
            break;
        case '[':
            p++;
            node->typ = JSON_ARR;
            da_t *arr = da_init(sizeof(json_t*));
            do {
                SKIPWS; const json_t *el = parse();
                SKIPWS; *(json_t**)da_add(arr) = (json_t*)el;
            } while (*p == ',');
            assert(*p == ']');
            p++;
            break;
        case '"':
            node->typ = JSON_STR;
            node->val.str = parsestr();
            break;
        default:
            SKIPWS; node->typ = JSON_NUM;
            node->val.num = strtod(p, &p);
            break;
    }

    return node;
}

const json_t *json_parse(const char* str) {
    p = (char*)str;
    e = p + strlen(p);
    return parse();
}

void json_print(const json_t *node, FILE *s) {
    size_t len;
    switch (node->typ) {
        case JSON_OBJ:
            fputs("{", s);
            len = hm_len(node->val.obj);
            for (size_t i = 0; i < len; i++) {
                fputc('"', s);
                fputs(node->val.obj[i].key, s);
                fputs("\":", s);
                json_print(node->val.obj[i].val, s);
                if (i < len-1) fputc(',', s);
            }
            fputc('}', s);
            break;
        case JSON_ARR:
            fputc('[', s);
            len = node->val.arr->len;
            for (size_t i = 0; i < len; i++) {
                json_print(((json_t**)node->val.arr->data)[i], s);
                if (i != len) fputc(',', s);
            }
            fputc(']', s);
            break;
        case JSON_STR:
            fprintf(s, "\"%s\"", node->val.str);
            break;
        case JSON_NUM:
            fprintf(s, "%f", node->val.num);
            break;
    }
}

void json_free(const json_t *node) {
    size_t len;
    switch (node->typ) {
        case JSON_OBJ:
            len = hm_len(node->val.obj);
            for (size_t i = 0; i < len; i++) {
                free((void*)node->val.obj[i].key);
                json_free(node->val.obj[i].val);
            }
            hm_free(node->val.obj);
            break;
        case JSON_ARR:
            len = node->val.arr->len;
            for (size_t i = 0; i < len; i++)
                json_free(((json_t**)node->val.arr->data)[i]);
            da_free((da_t*)node->val.arr);
            break;
        case JSON_STR:
            free((char*)node->val.str);
            break;
        case JSON_NUM:
            break;
    }
    free((json_t*)node);
}
