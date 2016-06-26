#include "config.h"
#include "globals.h"

#include "errno.h"

#include "util/init.h"
#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"

#include "mm/slab.h"
#include "mm/page.h"

kthread_t *curthr; /* global */
static slab_allocator_t *kthread_allocator = NULL;

#ifdef __MTP__
/* Stuff for the reaper daemon, which cleans up dead detached threads */
static proc_t *reapd = NULL;
static kthread_t *reapd_thr = NULL;
static ktqueue_t reapd_waitq;
static list_t kthread_reapd_deadlist; /* Threads to be cleaned */

static void *kthread_reapd_run(int arg1, void *arg2);
#endif

void
kthread_init()
{
        kthread_allocator = slab_allocator_create("kthread", sizeof(kthread_t));
        KASSERT(NULL != kthread_allocator);
}

/**
 * Allocates a new kernel stack.
 *
 * @return a newly allocated stack, or NULL if there is not enough
 * memory available
 */
static char *
alloc_stack(void)
{
        /* extra page for "magic" data */
        char *kstack;
        int npages = 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT);
        kstack = (char *)page_alloc_n(npages);

        return kstack;
}

/**
 * Frees a stack allocated with alloc_stack.
 *
 * @param stack the stack to free
 */
static void
free_stack(char *stack)
{
        page_free_n(stack, 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT));
}

/*
 * Allocate a new stack with the alloc_stack function. The size of the
 * stack is DEFAULT_STACK_SIZE.
 *
 * Don't forget to initialize the thread context with the
 * context_setup function. The context should have the same pagetable
 * pointer as the process.
 */
kthread_t *
kthread_create(struct proc *p, kthread_func_t func, long arg1, void *arg2)
{
        KASSERT(NULL != p);
        dbg(DBG_INIT,"(GRADING1 3.a) : Process is not NULL \n");

        /*KASSERT(p && curproc && (p->p_pid == curproc->p_pid));*/

	kthread_t *new_thread=(kthread_t*)slab_obj_alloc(kthread_allocator);
        /* Create stack */
	new_thread->kt_kstack=alloc_stack();

	new_thread->kt_proc=p;
	new_thread->kt_cancelled=0;
	new_thread->kt_wchan=NULL;
	new_thread->kt_state=KT_NO_STATE;
	list_insert_head(&((new_thread->kt_proc)->p_threads),&(new_thread->kt_plink));
        dbg(DBG_INIT, "Context setup for  %d process's thread\n", p->p_pid);
        context_setup(&new_thread->kt_ctx,func,arg1,arg2,new_thread->kt_kstack,DEFAULT_STACK_SIZE,new_thread->kt_proc->p_pagedir);
        dbg(DBG_INIT, "Finished creating %d process's thread\n", p->p_pid);

       return new_thread;

}

void
kthread_destroy(kthread_t *t)
{
        KASSERT(t && t->kt_kstack);
        free_stack(t->kt_kstack);
        if (list_link_is_linked(&t->kt_plink)){
                list_remove(&t->kt_plink);
        }
        slab_obj_free(kthread_allocator, t);
return;
}

/*
 * If the thread to be cancelled is the current thread, this is
 * equivalent to calling kthread_exit. Otherwise, the thread is
 * sleeping and we need to set the cancelled and retval fields of the
 * thread.
 *
 * If the thread's sleep is cancellable, cancelling the thread should
 * wake it up from sleep.
 *
 * If the thread's sleep is not cancellable, we do nothing else here.
 */
void
kthread_cancel(kthread_t *kthr, void *retval)
{
        KASSERT(NULL != kthr);
        dbg(DBG_INIT,"(GRADING1 3.b) : Thread to be cancelled is not NULL \n");

        dbg(DBG_INIT, "Thread for %dth process is going to be cancelled\n", kthr->kt_proc->p_pid);
        if(curthr==kthr){
		kthread_exit((retval));
        }
	else{
		sched_cancel(kthr);
		kthr->kt_retval=retval;
        }
return;
}

/*
 * You need to set the thread's retval field, set its state to
 * KT_EXITED, and alert the current process that a thread is exiting
 * via proc_thread_exited.
 *
 * It may seem unneccessary to push the work of cleaning up the thread
 * over to the process. However, if you implement MTP, a thread
 * exiting does not necessarily mean that the process needs to be
 * cleaned up.
 */
void
kthread_exit(void *retval)
{
        KASSERT(!curthr->kt_wchan);
        dbg(DBG_INIT,"(GRADING1 3.c) : Current thread's wchan is not NULL \n");
        KASSERT(!curthr->kt_qlink.l_next && !curthr->kt_qlink.l_prev);
        dbg(DBG_INIT,"(GRADING1 3.c) : Current thread's kt_qlink is empty \n");
        KASSERT(curthr->kt_proc == curproc);
        dbg(DBG_INIT,"(GRADING1 3.c) : Current thread's process is the current process \n");
	curthr->kt_retval=retval;
        /* Set state */
	curthr->kt_state=KT_EXITED;
	/*Call exited*/
	proc_thread_exited(retval);
return;
}

/*
 * The new thread will need its own context and stack. Think carefully
 * about which fields should be copied and which fields should be
 * freshly initialized.
 *
 * You do not need to worry about this until VM.
 */
kthread_t *
kthread_clone(kthread_t *thr)
{
        NOT_YET_IMPLEMENTED("VM: kthread_clone");
        return NULL;
}

/*
 * The following functions will be useful if you choose to implement
 * multiple kernel threads per process. This is strongly discouraged
 * unless your weenix is perfect.
 */
#ifdef __MTP__
int
kthread_detach(kthread_t *kthr)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_detach");
        return 0;
}

int
kthread_join(kthread_t *kthr, void **retval)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_join");
        return 0;
}

/* ------------------------------------------------------------------ */
/* -------------------------- REAPER DAEMON ------------------------- */
/* ------------------------------------------------------------------ */
static __attribute__((unused)) void
kthread_reapd_init()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_init");
}
init_func(kthread_reapd_init);
init_depends(sched_init);

void
kthread_reapd_shutdown()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_shutdown");
}

static void *
kthread_reapd_run(int arg1, void *arg2)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_run");
        return (void *) 0;
}
#endif
