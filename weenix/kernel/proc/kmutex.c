#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
        
        dbg(DBG_INIT,"Mutex is initialized\n");
        /* Mutex acquired by none */
        mtx->km_holder = NULL;
        /* Mutex queue is initialized */
        sched_queue_init(&mtx->km_waitq);
}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */
void
kmutex_lock(kmutex_t *mtx)
{
        KASSERT(curthr && (curthr != mtx->km_holder));
        dbg(DBG_INIT,"(GRADING1 5.a) : Current thread is not null and current thread is not the holder of mutex \n"); 
        if (mtx->km_holder == NULL ) {
            /* Run in case of mutex available */
            dbg(DBG_INIT,"Mutex is not taken by anything, assigning mutex to  %d process thread\n", 
                            curthr->kt_proc->p_pid);
            mtx->km_holder = curthr;
        }
        else {
            /* Run in case of mutex is not available */
            dbg(DBG_INIT,"Mutex is taken already by %d process thread, Adding the %d process thread in KM_WAITQ \n", 
                            mtx->km_holder->kt_proc->p_pid, curthr->kt_proc->p_pid);
            /* Sleep on the KM_WAITQ */
            sched_sleep_on(&mtx->km_waitq);
            /* Rise up when wakeup_upon is called. */
            dbg(DBG_INIT,"Woked up %d process thread from KM_WAITQ\n", curthr->kt_proc->p_pid);
        }
}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead.
 */
int
kmutex_lock_cancellable(kmutex_t *mtx)
{
        KASSERT(curthr && (curthr != mtx->km_holder));
        dbg(DBG_INIT,"(GRADING1 5.b) : Current thread is not null and current thread is not the holder of mutex.\n"); 
        if (mtx->km_holder == NULL ) {
            /* Run in case of mutex available */
            dbg(DBG_INIT,"Mutex is not taken by anything, assigning mutex to  %d process thread in cancellable lock\n", 
                            curthr->kt_proc->p_pid);
            mtx->km_holder = curthr;
        }
        else {
            /* Run in case of mutex is not available */
            dbg(DBG_INIT,"Mutex is taken already by %d process thread, Adding the %d process thread in KM_WAITQ in cancellable lock\n", 
                            mtx->km_holder->kt_proc->p_pid, curthr->kt_proc->p_pid);
            int status = sched_cancellable_sleep_on(&mtx->km_waitq);
            if (status == -EINTR){
                /* Return in case of thread is cancelled */
                return -EINTR;
            } 
            dbg(DBG_INIT,"Woked up %d process thread from KM_WAITQ\n", curthr->kt_proc->p_pid);
        }
        return 0;
}

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Don't forget to add the new owner of the mutex back to the
 * run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
        KASSERT(curthr && (curthr == mtx->km_holder));
        dbg(DBG_INIT,"(GRADING1 5.c) : Current thread is not null and current thread is the holder of the lock.\n"); 
        dbg(DBG_INIT, "The %d process thread is giving up the lock\n", curthr->kt_proc->p_pid);
        /* Freeing the lock */
        mtx->km_holder = NULL;
        /* Waking one of the thread sleeping upon KM_WAITQ. */
        kthread_t *unlocked_thread = sched_wakeup_on(&mtx->km_waitq);
        if (unlocked_thread!=NULL){
            dbg(DBG_INIT,"Unlocked %d process thread \n", unlocked_thread->kt_proc->p_pid);
        }
        /* Assign the woken up thread to the lock */
        mtx->km_holder = unlocked_thread;
        KASSERT(curthr != mtx->km_holder);
        dbg(DBG_INIT,"(GRADING1 5.c) : Current thread is the not the holder of the lock.\n"); 
}
