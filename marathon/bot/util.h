#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int is_strdigit(const char* s);
int dynamic_mem_fgets(FILE*, char**);
void seek_until(FILE* fp, const unsigned char* b, int len);
void strip_endline(char* s);
void str_fixnull(char* s, size_t len);
