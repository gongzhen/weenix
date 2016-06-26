#include "types.h"
#include "globals.h"
#include "kernel.h"

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
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/cpuid.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/tty/virtterm.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"

#include "fs/open.h"

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);

static context_t bootstrap_context;

static int gdb_wait = GDBWAIT;
extern void *sunghan_test(int, void*);
extern void *sunghan_deadlock_test(int, void*);
extern void *testproc(int, void*);
extern void *vfstest_main(int argc, void *);	

    #ifdef __DRIVERS__

        int faber(kshell_t *kshell, int argc, char **argv)
        {
            KASSERT(kshell != NULL);
            dbg(DBG_INIT, "(GRADING): testproc() is invoked, argc = %d, argv = 0x%08x\n",
                    argc, (unsigned int)argv);
		testproc(argc, *argv);
            return 0;
        }
	int sunghan(kshell_t *kshell, int argc, char **argv)
        {
            KASSERT(kshell != NULL);
            dbg(DBG_INIT, "(GRADING): sunghan_test() and sunghan_deadlock_test are invoked, argc = %d, argv = 0x%08x\n",
                    argc, (unsigned int)argv);
		sunghan_test(argc, *argv);
            return 0;
        }
	int deadlock(kshell_t *kshell, int argc, char **argv)
        {
            KASSERT(kshell != NULL);
            dbg(DBG_INIT, "(GRADING): sunghan_test() and sunghan_deadlock_test are invoked, argc = %d, argv = 0x%08x\n",
                    argc, (unsigned int)argv);
		sunghan_deadlock_test(argc, *argv);
            return 0;
        }

	int vfstest(kshell_t *kshell, int argc, char **argv){
            
		KASSERT(kshell != NULL);
		/*
		dbg(DBG_PRINT, "(GRADING): sunghan_test() and sunghan_deadlock_test are invoked, argc = %d, argv = 0x%08x\n",
                    argc, (unsigned int)argv);
		*/
             	vfstest_main(argc, NULL);	
		return 0;
	}
	int rename(kshell_t *kshell, int argc, char **argv){
		KASSERT(kshell!=NULL);
		const char *newname="tty0RENAMED";
		do_rename("dev/tty0",newname);
		return 0;	
	}
	int hello(kshell_t *kshell, int argc, char **argv){
		KASSERT(kshell!=NULL);
		char *a[] = { NULL };
		char *e[] = { NULL };
		kernel_execve("/usr/bin/vfstest", a, e);
		return 0;	
	}
	
	
    #endif /* __DRIVERS__ */
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
	/* This little loop gives gdb a place to synch up with weenix.  In the
	 * past the weenix command started qemu was started with -S which
	 * allowed gdb to connect and start before the boot loader ran, but
	 * since then a bug has appeared where breakpoints fail if gdb connects
	 * before the boot loader runs.  See
	 *
	 * https://bugs.launchpad.net/qemu/+bug/526653
	 *
	 * This loop (along with an additional command in init.gdb setting
	 * gdb_wait to 0) sticks weenix at a known place so gdb can join a
	 * running weenix, set gdb_wait to zero  and catch the breakpoint in
	 * bootstrap below.  See Config.mk for how to set GDBWAIT correctly.
	 *
	 * DANGER: if GDBWAIT != 0, and gdb isn't run, this loop will never
	 * exit and weenix will not run.  Make SURE the GDBWAIT is set the way
	 * you expect.
	 */
      	while (gdb_wait) ;
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

        /*NOT_YET_IMPLEMENTED("PROCS: bootstrap");*/
	
	proc_t *pProc = proc_create("idle process");
	curproc = pProc; 
	
	KASSERT(NULL != curproc);
	dbg_print("GRADING1 1.a PASSED: idle process has been created successfully.\n");
	
	KASSERT(PID_IDLE == curproc->p_pid);
	dbg_print("GRADING1 1.a PASSED: what has been created is the idle process.\n");
	
	curthr = kthread_create(curproc, idleproc_run, NULL, NULL);
	KASSERT(NULL != curthr);	
	dbg_print("GRADING1 1.a PASSED: the thread for the idle process has been created successfully.\n");

        context_make_active(&(curthr->kt_ctx));

        panic("weenix returned to bootstrap()!!! BAD!!!\n");
        return NULL;
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

        init_call_all();
        GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
        /* Once you have VFS remember to set the current working directory
         * of the idle and init processes */

        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */
        /*NOT_YET_IMPLEMENTED("VFS: idleproc_run");*/

	
        curproc->p_cwd = vfs_root_vn;
        initthr->kt_proc->p_cwd = vfs_root_vn;
	vref(curproc->p_cwd);
	vref(curproc->p_cwd);
	

	do_mkdir("/dev");
	do_mknod("/dev/null", S_IFCHR, MKDEVID(1,0));	
	do_mknod("/dev/zero", S_IFCHR, MKDEVID(1,1));	
	do_mknod("/dev/tty0", S_IFCHR, MKDEVID(2,0));	
	/*do_mknod("/dev/tty2", S_IFCHR, MKDEVID(2,3));tty first*/	
	/*TODO: tty devide 2?*/
#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();

        /* Run initproc */
        sched_make_runnable(initthr);
        /* Now wait for it */
        child = do_waitpid(-1, 0, &status);
        KASSERT(PID_INIT == child);

#ifdef __MTP__
        kthread_reapd_shutdown();
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
        /*NOT_YET_IMPLEMENTED("PROCS: initproc_create");*/
	
	/*create the init process*/
	kthread_t *pThread = NULL;
	proc_t *pProc = proc_create("init process");
	KASSERT(NULL != pProc); 	
	dbg_print("GRADING1 1.b PASSED: init process has been created.\n");
	
	KASSERT(PID_INIT == pProc->p_pid);
	dbg_print("GRADING1 1.b PASSED: what has been created is the init process.\n");
	
	/*create a thread to execute init process*/
	pThread = kthread_create(pProc, initproc_run, NULL, NULL);
	KASSERT(/* pointer to the thread for the "init" process */ pThread != NULL);
	dbg_print("GRADING1 1.b PASSED: thread for the init process has been created.\n");

        return pThread;
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
       /* NOT_YET_IMPLEMENTED("PROCS: initproc_run");*/


    #ifdef __DRIVERS__
	
        kshell_add_command("faber", faber, "faber tests");
        kshell_add_command("sunghan", sunghan, "sunghan tests");
        kshell_add_command("deadlock", deadlock, "sunghan deadlock tests");
        kshell_add_command("renametest",rename,"Renames dev/tty0 to tty0RENAMED");
	kshell_add_command("vfstest", vfstest, "vfs tests");
        /*
	kshell_t *kshell = kshell_create(0);
        if (NULL == kshell) panic("init: Couldn't create kernel shell\n");
	kshell_add_command("hello", hello, "vfs tests");
        while(kshell_execute_next(kshell));
	*/	
	char *argv[] = {  NULL };
    	char *envp[] = {  NULL };
       	
	kernel_execve("/sbin/init", argv, envp); 
	/*do_open("/dev/tty0", O_RDONLY);*/
	
	
        /*kshell_destroy(kshell);*/

    #endif /* __DRIVERS__ */
/*	
	if(curproc->p_cwd != NULL){
		vput(curproc->p_cwd);

	}
*/
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
