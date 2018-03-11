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
void list_test_display(struct p_data* list);
struct p_data* new_int(int n);
struct p_data* new_str(unsigned char* str, int len);
struct p_data* new_float(float n);
void pickle_int(struct p_data* data, struct dynstr* res);
void pickle_str(struct p_data* data, struct dynstr* res);
void pickle_list(struct p_data* data, struct dynstr* res);
void _pickle(struct p_data* data, struct dynstr* res);
struct dynstr* create_pickle(struct p_data* data);
void str_append(struct dynstr* str, unsigned char* new, size_t len_new);
void hexdump(unsigned char data[], int len);
struct dynstr* build_pickle(struct p_data* data);
int p_data_free(struct p_data* data);

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
