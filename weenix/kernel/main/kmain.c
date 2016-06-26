#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "errno.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadowd.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/disk/ata.h"
#include "drivers/tty/virtterm.h"
#include "drivers/pci.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/namev.c"

#include "test/proctest.h"
#include "test/ttytest.h"
#include "test/atatest.h"
#include "test/memdevtest.h"
#include "test/s5fstest.h"
#include "test/vmmtest.h"
#include "test/kshell/customcommands.h"

#include "test/kshell/kshell.h"
#include "../test/kshell/priv.h"
#include "../test/kshell/command.h"

#include "test/kshell/io.h"
GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);

static context_t bootstrap_context;

int vfstest_main(int argc, char **argv);

/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);

        dbg_init();
        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
	      pci_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
bootstrap(int arg1, void *arg2)
{
    /* necessary to finalize page table information */
    pt_template_init();

    char *name = "idle process";

    proc_t *idle_proc = proc_create(name);

    if (idle_proc == NULL){
        panic("idle proc is NULL :( \n");
    }

    KASSERT(idle_proc->p_pid == 0);

    kthread_t *idle_thread = kthread_create(idle_proc, idleproc_run, NULL, NULL);

    if (idle_thread == NULL){
        panic("idle thread is NULL :( \n");
    }

    curproc = idle_proc;
    curthr = idle_thread;

    context_make_active(&idle_thread->kt_ctx);   

    panic("weenix returned to bootstrap()!!! BAD!!!\n");
    return NULL;
}

static void destroy_kshell_commands(){
    list_t *commands = &kshell_commands_list;
    list_link_t *link = commands->l_next;

    while (link != commands){
        kshell_command_t *cmd = list_item(link, kshell_command_t, kc_commands_link);

        
        
        link = link->l_next;

         if (cmd != NULL){
            kshell_command_destroy(cmd);
        }
    }
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
    int status;
    pid_t child;

    /* create init proc */
    kthread_t *initthr = initproc_create();

    if (initthr == NULL){
        panic("couldn't create init proc\n");
    }

    init_call_all();
    GDB_CALL_HOOK(initialized);

    /* Create other kernel threads (in order) */

#ifdef __VFS__
    /* Once you have VFS remember to set the current working directory
     * of the idle and init processes */
    curproc->p_cwd = vfs_root_vn;
    vref(vfs_root_vn); 

    initthr->kt_proc->p_cwd = vfs_root_vn;
    vref(vfs_root_vn);

    /* Here you need to make the null, zero, and tty devices using mknod 
     You can't do this until you have VFS, check the include/drivers/dev.h
     file for macros with the device ID's you will need to pass to mknod */
    int mkdir_res = do_mkdir("/dev");

    if (mkdir_res == 0){
        if (do_mknod("/dev/tty0", S_IFCHR, MKDEVID(2, 0)) < 0){
            panic("unable to create tty0\n");
        }

        if (do_mknod("/dev/tty1", S_IFCHR, MKDEVID(2, 1)) < 0){
            panic("unable to create tty1\n");
        }

        if (do_mknod("/dev/tty2", S_IFCHR, MKDEVID(2, 2)) < 0){
            panic("unable to create tty2\n");
        }

        /*if (do_mknod("/dev/null", S_IFBLK, MEM_NULL_DEVID) < 0){*/
        if (do_mknod("/dev/null", S_IFCHR, MEM_NULL_DEVID) < 0){
            panic("unable to create /dev/null");
        } 

        if (do_mknod("/dev/zero", S_IFCHR, MEM_ZERO_DEVID) < 0){
            panic("unable to create /dev/zero");
        }
    } else {
        KASSERT(mkdir_res == -EEXIST && "wrong type of error when making /dev");
    }

    int mktmp_res = do_mkdir("/tmp");

    KASSERT((mkdir_res == 0 || mkdir_res == -EEXIST) && "wront type of error \
            making /tmp");

    /*kmutex_init(&lookup_mutex);*/

#endif

    /* Finally, enable interrupts (we want to make sure interrupts
     * are enabled AFTER all drivers are initialized) */
    intr_enable();

    /* Run initproc */
    sched_make_runnable(initthr);

    /* Now wait for it */
    child = do_waitpid(-1, 0, &status);
    KASSERT(PID_INIT == child);

    destroy_kshell_commands();

#ifdef __MTP__
    kthread_reapd_shutdown();
#endif


#ifdef __SHADOWD__
    /* wait for shadowd to shutdown */
    shadowd_shutdown();
#endif

#ifdef __VFS__
    /* Shutdown the vfs: */
    dbg_print("weenix: vfs shutdown...\n");
    vput(curproc->p_cwd);
    if (vfs_shutdown())
        panic("vfs shutdown FAILED!!\n");

#endif

    /* Shutdown the pframe system */
#ifdef __S5FS__
    pframe_shutdown();
#endif

    dbg_print("\nweenix: halted cleanly!\n");
    GDB_CALL_HOOK(shutdown);
    hard_shutdown();
    return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
    proc_t *initproc = proc_create("init proc");

    if (initproc == NULL){
        return NULL;
    }

    KASSERT(initproc->p_pid == (pid_t) 1 && "initproc pid isn't 1");

    kthread_t *init_thread = kthread_create(initproc, initproc_run, NULL, NULL);

    return init_thread;
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/bin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
initproc_run(int arg1, void *arg2)
{
    static char bullshit[1000];
    /*run_vmm_tests();*/

    /*kshell_add_command("exec", kshell_exec, "executes a given command");*/

    char *empty_args[2] = {"init", NULL};
    char *empty_envp[1] = {NULL};
    /*kernel_execve("/usr/bin/hello", empty_args, empty_envp);*/
    kernel_execve("/sbin/init", empty_args, empty_envp);
    panic("oh shit");
    
    /*run_proc_tests();*/
    /*run_tty_tests();*/
    /*run_memdev_tests();*/
    /*[>run_ata_tests();<]*/

/*    int i;*/
    /*for (i = 0; i < 4; i++){*/
        /*run_s5fs_tests();*/
    /*}*/
    
    /*vfstest_main(1, NULL);   */
    
    return NULL;
}

/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}
