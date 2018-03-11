#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int is_strdigit(const char* s) {
    while(*s)
        if(!isdigit(*s++))
            return 0;
    return 1;
}

static inline void err(char* s) {
    fprintf(stderr, "[Error] %s\nexiting..", err);
    exit(1);
}

static inline void warn(char* s) {
    fprintf(stderr, "[Warn] %s\n", s);
}

// Dynamic fgets memory allocation
int dynamic_mem_fgets(FILE* fp, char** dst) {
    char buf[256];
    char **res = dst;
    *res = malloc(1);
    int tmp, buf_len = 0, res_len = 0;
    if(!*res) err("malloc");
    **res = '\0';
    memset(buf, '\0', 256);
    while(1) {
        // current file position
        tmp = ftell(fp);
        fgets(buf, 256, fp);
        // substract new file position with old file position to get the len of buf
        buf_len = ftell(fp) - tmp;
        // new allocation for new data
        *res = realloc(*res, res_len + buf_len + 1);
        if(!*res) err("realloc");
        // append new data
        memcpy(&(*res)[res_len], buf, buf_len);
        // get new len data
        res_len += buf_len;
        // check if *res contain newline or fp reach eof then break
        if(memchr(*res, '\n', res_len) || feof(fp))
            break;
    }
    return res_len;
}

void seek_until(FILE* fp, const unsigned char* b, int len) {
    int c, i;
LAGI:
    while((c = fgetc(fp)) != EOF) {
        for(i = 0; i < len; i++) {
            if(c != b[i])
                goto LAGI;
            c = fgetc(fp);
        }
        break;
    }
}

void strip_endline(char* s) {
    int l = strlen(s);
    for(l; l >= 0; l--) {
        if(s[l] != '\n' && s[l] != '\0')
            break;
        s[l] = '\0';
    }
}

void str_replace(char* s, char f, char n, size_t len) {
    int i;
    for(i = 0; i < len; i++) {
        if(s[i] == f)
            s[i] = n;
    }
}

void str_fixnull(char* s, size_t len) {
    str_replace(s, '\0', '.', len);
}
