#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h> // fot htonl
#include <list.h>

#define MARK            '('   //# push special markobject on stack
#define STOP            '.'   //# every pickle ends with STOP
#define POP             '0'   //# discard topmost stack item
#define POP_MARK        '1'   //# discard stack top through topmost markobject
#define DUP             '2'   //# duplicate top stack item
#define FLOAT           'F'   //# push float object; decimal string argument
#define INT             'I'   //# push integer or bool; decimal string argument
#define BININT          'J'   //# push four-byte signed int
#define BININT1         'K'   //# push 1-byte unsigned int
#define LONG            'L'   //# push long; decimal string argument
#define BININT2         'M'   //# push 2-byte unsigned int
#define NONE            'N'   //# push None
#define PERSID          'P'   //# push persistent object; id is taken from string arg
#define BINPERSID       'Q'   //#  "       "         "  ;  "  "   "     "  stack
#define REDUCE          'R'   //# apply callable to argtuple, both on stack
#define STRING          'S'   //# push string; NL-terminated string argument
#define BINSTRING       'T'   //# push string; counted binary string argument
#define SHORT_BINSTRING 'U'   //#  "     "   ;    "      "       "      " < 256 bytes
#define UNICODE         'V'   //# push Unicode string; raw-unicode-escaped'd argument
#define BINUNICODE      'X'   //#   "     "       "  ; counted UTF-8 string argument
#define APPEND          'a'   //# append stack top to list below it
#define BUILD           'b'   //# call __setstate__ or __dict__.update()
#define GLOBAL          'c'   //# push self.find_class(modname, name); 2 string args
#define DICT            'd'   //# build a dict from stack items
#define EMPTY_DICT      '}'   //# push empty dict
#define APPENDS         'e'   //# extend list on stack by topmost stack slice
#define GET             'g'   //# push item from memo on stack; index is string arg
#define BINGET          'h'   //#   "    "    "    "   "   "  ;   "    " 1-byte arg
#define INST            'i'   //# build & push class instance
#define LONG_BINGET     'j'   //# push item from memo on stack; index is 4-byte arg
#define LIST            'l'   //# build list from topmost stack items
#define EMPTY_LIST      ']'   //# push empty list
#define OBJ             'o'   //# build & push class instance
#define PUT             'p'   //# store stack top in memo; index is string arg
#define BINPUT          'q'   //#   "     "    "   "   " ;   "    " 1-byte arg
#define LONG_BINPUT     'r'   //#   "     "    "   "   " ;   "    " 4-byte arg
#define SETITEM         's'   //# add key+value pair to dict
#define TUPLE           't'   //# build tuple from topmost stack items
#define EMPTY_TUPLE     ')'   //# push empty tuple
#define SETITEMS        'u'   //# modify dict by adding topmost key+value pairs
#define BINFLOAT        'G'   //# push float; arg is 8-byte float encoding

#define TYPE_INT 1
#define TYPE_FLOAT 2
#define TYPE_STR 3
#define TYPE_LIST 4
#define TYPE_DICT 5

struct dynstr {
    unsigned char* data;
    size_t len;
};

union u_data {
    int d_int;
    float d_float;
    unsigned char* d_str;
    struct llist* d_list;
    struct llist* d_dict;
};

struct p_data {
    int type;
    size_t size;
    union u_data data;
    unsigned hash;
};
struct p_dict {
    struct p_data* key;
    struct p_data* value;
};

void str_append(struct dynstr* str, unsigned char* new, size_t len_new);
unsigned int p_hash_str(unsigned char *str, size_t len);
unsigned int p_hash_float(float f);
unsigned int p_hash(struct p_data* data);
struct p_data* build_empty_dict();
struct p_dict* p_dict_kv(struct p_data* key, struct p_data* value);
struct p_data* p_dict_add(struct p_data* dict, struct p_dict* data_dict);
int is_dict_key(struct p_dict* dict, struct p_data* key);
struct p_data* build_dict(int nargs, ...);
struct p_dict* get_dict_from_key(struct p_data* dict, struct p_data* key);
struct p_data* get_dict_val(struct p_data* dict, struct p_data* key);
struct p_data* build_empty_list();
struct p_data* p_list_add(struct p_data* list, struct p_data* data);
struct p_data* build_list(int nargs, ...);
struct p_data* list_get_el(struct p_data* list, int n);
struct p_data* new_int(int n);
struct p_data* new_str(unsigned char* str, int len);
struct p_data* new_float(float n);
void pickle_int(struct p_data* data, struct dynstr* res);
void pickle_str(struct p_data* data, struct dynstr* res);
void pickle_list(struct p_data* data, struct dynstr* res);
void _pickle(struct p_data* data, struct dynstr* res);
struct dynstr* create_pickle(struct p_data* data);
void str_append(struct dynstr* str, unsigned char* new, size_t len_new);

#define LIST_NUMARGS(...) (sizeof((struct p_data* []){__VA_ARGS__})/sizeof(struct p_data*))
#define DICT_NUMARGS(...) (sizeof((struct p_dict* []){__VA_ARGS__})/sizeof(struct p_dict*))
#define _LIST(...) (build_list(LIST_NUMARGS(__VA_ARGS__), __VA_ARGS__))
#define _DICT(...) (build_dict(DICT_NUMARGS(__VA_ARGS__), __VA_ARGS__))

#define _STR_1(X) new_str(X, sizeof(X)-1)
#define _STR_2(X, N) new_str(X, N)
#define GET_ARGS(arg1, arg2, arg3, ...) arg3
#define _STR_MACRO_CHOOSER(...) \
    GET_ARGS(__VA_ARGS__, _STR_2, _STR_1, )
#define _STR(...) _STR_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define _INT new_int
#define _FLOAT new_float
#define _KV p_dict_kv
#define in_range(n, min, max) (min >= n && n < max)

void str_append(struct dynstr* str, unsigned char* new, size_t len_new) {
    str->data = realloc(str->data, str->len + len_new);
    memcpy(&str->data[str->len], new, len_new);
    str->len += len_new;
    return;
}

unsigned int p_hash_str(unsigned char* str, size_t len) {
    unsigned int hash = 5381;
    int c, i;
    for(int i=0; i < len; i++) {
        c = str[i];
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

unsigned int p_hash_float(float f) {
    unsigned int hash = *(unsigned int*)&f;
    return hash;
}

unsigned p_hash(struct p_data* data) {
    if(data->type == TYPE_STR) {
        return p_hash_str(data->data.d_str, data->size);
    }
    else if(data->type == TYPE_INT) {
        return data->data.d_int;
    }
    else if(data->type == TYPE_FLOAT){
        return p_hash_float(data->data.d_float);
    }
    else {
        return 0;
    }
}

struct p_data* new_int(int n) {
    struct p_data* new;
    new = malloc(sizeof(*new));
    memset(new, '\0', sizeof(*new));
    if(new == NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    new->size = 0;
    new->type = TYPE_INT;
    new->data.d_int = n;
    new->hash = p_hash(new);
    return new;
}

struct p_data* new_float(float n) {
    struct p_data* new;
    new = malloc(sizeof(*new));
    memset(new, '\0', sizeof(*new));
    new->size = 0;
    new->type = TYPE_FLOAT;
    new->data.d_int = n;
    new->hash = p_hash(new);
    return new;
}

struct p_data* new_str(unsigned char* str, int len) {
    struct p_data* new;
    unsigned char* data;
    new = malloc(sizeof(*new));
    if(new == NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    data = malloc(len + 1);
    if(data == NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    memset(data, '\0', len);
    new->type = TYPE_STR;
    memcpy(data, str, len);
    data[len] = '\0';
    new->data.d_str = data;
    new->size = len;
    new->hash = p_hash(new);
    //printf("%p\n", new);
    return new;
}

struct p_data* build_empty_list() {
    struct p_data* new = malloc(sizeof(*new));
    struct llist* head = malloc(sizeof(*head));
    new->type = TYPE_LIST;
    new->data.d_list = head;
    new->size = 0;
    head->tail = NULL;
    head->head = NULL;
    return new;
}

struct p_data* build_empty_dict() {
    struct p_data* new = malloc(sizeof(*new));
    struct llist* head = malloc(sizeof(*head));
    new->type = TYPE_DICT;
    new->data.d_dict = head;
    new->size = 0;
    head->tail = NULL;
    head->head = NULL;
    return new;
}

struct p_data* p_list_add(struct p_data* list, struct p_data* data) {
    assert(list->type == TYPE_LIST);
    struct llist* head = list->data.d_list;
    llist_add(head, data);
    list->size += 1;
    return list;
}

struct p_dict* p_dict_kv(struct p_data* key, struct p_data* value) {
    struct p_dict* new;
    new = malloc(sizeof(*new));
    new->key = key;
    new->value = value;
    return new;
}

struct p_data* p_dict_add(struct p_data* dict, struct p_dict* data_dict) {
    assert(dict->type == TYPE_DICT);
    struct llist* head = dict->data.d_dict;
    llist_add(head, data_dict);
    dict->size += 1;
    return dict;
}

struct p_data* build_list(int nargs, ...) {
    va_list ap;
    struct p_data* data;
    struct p_data* list;
    int i;
    list = build_empty_list();
    va_start(ap, nargs);
    for(i = 0; i < nargs; i++) {
        data = va_arg(ap, struct p_data*);
        p_list_add(list, data);
    }
    va_end(ap);
    return list;
}

struct p_data* build_dict(int nargs, ...) {
    va_list ap;
    struct p_dict* data_dict;
    struct p_data* dict;
    int i;
    dict = build_empty_dict();
    va_start(ap, nargs);
    for(i = 0; i < nargs; i++) {
        data_dict = va_arg(ap, struct p_dict*);
        p_dict_add(dict, data_dict);
    }
    va_end(ap);
    return dict;
}

struct p_data* get_list_el(struct p_data* list, int n) {
    struct p_data* data;
    struct llist* head;
    head = list->data.d_list;
    int i = 0;
    if(n >= list->size)
        return NULL;
    llist_foreach(data, head)
        if(i++ == n)
            return data;
    llist_foreach_end(head)
    return NULL;
}

struct p_dict* get_dict_from_key(struct p_data* dict, struct p_data* key) {
    struct p_dict* data_dict;
    struct llist* head;
    head = dict->data.d_dict;
    llist_foreach(data_dict, head)
        if(data_dict->key->hash == key->hash)
            return data_dict;
    llist_foreach_end(head);
    return NULL;
}

struct p_data* get_dict_val(struct p_data* dict, struct p_data* key) {
    struct p_dict* data_dict;
    data_dict = get_dict_from_key(dict, key);
    if(data_dict != NULL)
        return data_dict->value;
    return NULL;
}

int p_data_free(struct p_data* data) {
    struct p_data* key;
    struct p_data* value;
    struct p_dict* data_dict;
    struct llist* head;
    struct p_data* _data;
    if(data->type == TYPE_INT || data->type == TYPE_FLOAT) {
        free(data);
    }
    else if(data->type == TYPE_FLOAT) {
        free(data);
    }
    else if(data->type == TYPE_STR) {
        free(data->data.d_str);
        free(data);
    }
    else if(data->type == TYPE_LIST) {
        head = data->data.d_list;
        llist_foreach(_data, head)
            p_data_free(_data);
        llist_foreach_end(head);
    }
    else if(data->type == TYPE_DICT) {
        head = data->data.d_dict;
        llist_foreach(data_dict, head)
            key = data_dict->key;
            value = data_dict->value;
            //printf("%p\n", key);
            //printf("%p\n", value);
            p_data_free(key);
            p_data_free(value);
        llist_foreach_end(head)
    }
    else {
        return -1;
    }
    return 0;
}
void test_list() {
    struct p_data* list;
    struct p_data* data;
    struct llist* head;
    list = _LIST(_INT(100), _INT(200), _INT(300));
    head = list->data.d_list;
    llist_foreach(data, head)
        printf("%d\n", data->data.d_int);
    llist_foreach_end(head)
    data = get_list_el(list, 2);
    printf("%d\n", data->data.d_int);
}

void test_dict() {
    struct p_data* dict;
    struct p_data *key, *value;
    struct p_dict* data_dict;
    struct llist* head;
    dict = _DICT(_KV(_STR("Nama"), _STR("n0psledbyte")),
                 _KV(_STR("Pemrograman"), _STR("C++")),
                 _KV(_STR("Level"), _STR("100")));
    head = dict->data.d_dict;
    llist_foreach(data_dict, head)
        key = data_dict->key;
        value = data_dict->value;
        printf("%s : %s\n", key->data, value->data);
    llist_foreach_end(head)
    data_dict = get_dict_from_key(dict, _STR("Nama"));
    value = data_dict->value;
    printf("%s\n", value->data);
    value = get_dict_val(dict, _STR("Pemrograman"));
    printf("%s\n", value->data);
}

void test_data() {
    struct p_data* s = _STR("Hello world");
    printf("%s\n", s->data);
    s = _INT(100);
    printf("%d\n", s->data);
    test_list();
    test_dict();
}


void pickle_int(struct p_data* data, struct dynstr* res) {
    unsigned char code;
    int sz;
    int n = data->data.d_int;
    if in_range(n, 0, 256) {
        code = BININT1;
        sz = 1;
    }
    else if in_range(n, 256, 65536) {
        code = BININT2;
        sz = 2;
    }
    else {
        code = BININT;
        sz = 4;
    }
    str_append(res, &code, 1);
    str_append(res, (unsigned char*)&n, sz);
}

void pickle_str(struct p_data* data, struct dynstr* res) {
    int sz = data->size;
    unsigned char code = BINUNICODE;
    str_append(res, &code, 1);
    str_append(res, (unsigned char*)&sz, 4);
    if(data->size > 0) str_append(res, data->data.d_str, sz);
}

void pickle_list(struct p_data* data, struct dynstr* res) {
    struct llist* head = data->data.d_list;
    struct p_data* _data;
    unsigned char code = EMPTY_LIST;
    str_append(res, &code , 1);
    code = MARK;
    str_append(res, &code , 1);
    llist_foreach(_data, head)
        _pickle(_data, res);
    llist_foreach_end(head)
    code = APPENDS;
    str_append(res, &code , 1);
}

void pickle_dict(struct p_data* data, struct dynstr* res) {
    struct llist* head = data->data.d_list;
    struct p_dict* data_dict;
    struct p_data *key, *value;
    unsigned char code = EMPTY_DICT;
    str_append(res, &code, 1);
    code = MARK;
    str_append(res, &code, 1);
    llist_foreach(data_dict, head)
        key = data_dict->key;
        value = data_dict->value;
        _pickle(key, res);
        _pickle(value, res);
    llist_foreach_end(head);
    code = SETITEMS;
    str_append(res, &code, 1);
}

void _pickle(struct p_data* data, struct dynstr* res) {
    if(data->type == TYPE_INT) pickle_int(data, res);
    else if(data->type == TYPE_STR) pickle_str(data, res);
    else if(data->type == TYPE_LIST) pickle_list(data, res);
    else if(data->type == TYPE_DICT) pickle_dict(data, res);
    else printf("Data type not implemented\n");
}

struct dynstr* build_pickle(struct p_data* data) {
    struct dynstr* res;
    res = malloc(sizeof(*res));
    res->data = malloc(1);
    res->len = 0;
    str_append(res, "\x80\x03", 2);
    _pickle(data, res);
    str_append(res, ".", 1);
    return res;
}

void hexdump(unsigned char data[], int len) {
    int i;
    for(i = 0; i < len; i++)
        printf("%02x", data[i]);
    printf("\n");
}

void test_pickle() {
    struct dynstr* res;
    res = build_pickle(_LIST(_INT(100), _INT(200), _STR("Hello world")));
    hexdump(res->data, res->len);
    res = build_pickle(_DICT(_KV(_STR("Nama"), _STR("n0psledbyte")),
                       _KV(_STR("Pemrograman"), _STR("C++")),
                       _KV(_STR("Level"), _INT(100))));
    hexdump(res->data, res->len);
}
