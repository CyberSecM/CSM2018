#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char get_proc_ppid(int pid) {
    char path[60];
    char* res = NULL;
    char status;
    FILE* fpid;
    snprintf(path, 60, "/proc/%d/status", pid);
    fpid = fopen(path, "r");
    fscanf(fpid, "%*d %*c%*m[^\\)]%*c %c", &status);
    return status;
}

int main(void)
{
    int pid, ppid;
    char* comm;
    FILE* p;
    char status;
    p = fopen("/proc/771/stat", "r");
    fscanf(p, "%d %*c%m[^\\)]%*c %c %d", &pid, &comm, &status, &ppid);
    printf("%d\n", pid);
    printf("%s\n", comm);
    printf("%c\n", status);
    printf("%d\n", ppid);
    free(comm);
    return 0;
}
