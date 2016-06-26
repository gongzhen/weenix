#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"

/*my includes*/
#include "mm/slab.h"
#include "util/string.h"
#include "proc/sched.h"

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
        /*NOT_YET_IMPLEMENTED("PROCS: kmutex_init");*/
	sched_queue_init(&(mtx->km_waitq));
	mtx->km_holder = NULL;	
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
	dbg_print("GRADING1:5.a PASSED: curthr is not null and curthr is not the mutex holder.\n");

        /*NOT_YET_IMPLEMENTED("PROCS: kmutex_lock");*/
        
        if(mtx->km_holder != NULL){
                /* mutex locked */
		sched_sleep_on(&(mtx->km_waitq));
        }else{
                mtx->km_holder = curthr;
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
	 dbg_print("GRADING1 5.b PASSED: curthr is not null and curthr is not the mutex holder.\n");
	 int returnvalue=0;

        /*NOT_YET_IMPLEMENTED("PROCS: kmutex_lock_cancellable"); */
        if(mtx->km_holder == NULL){
		mtx->km_holder = curthr;
		return 0;
        }else{
		returnvalue=sched_cancellable_sleep_on(&(mtx->km_waitq));
		return returnvalue;
	}
        
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
	dbg_print("GRADING1:5.c PASSED: curthr is not null and curthr IS the mutex holder.\n");
	if((mtx->km_waitq).tq_size!=0){
		mtx->km_holder=sched_wakeup_on(&mtx->km_waitq);
	}else{
		mtx->km_holder = NULL;	
	}
	KASSERT(curthr != mtx->km_holder);
	dbg_print("GRADING1:5.c PASSED: curthr is NOT the mutex holder.\n");

}

