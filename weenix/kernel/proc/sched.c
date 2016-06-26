#include "globals.h"
#include "errno.h"

#include "main/interrupt.h"

#include "proc/sched.h"
#include "proc/kthread.h"

#include "util/init.h"
#include "util/debug.h"

static ktqueue_t kt_runq;
uint8_t priority,priority1,prior1;
kthread_t *newthread,*prevthread;
static __attribute__((unused)) void
sched_init(void)
{
    sched_queue_init(&kt_runq);
}
init_func(sched_init);

/*** PRIVATE KTQUEUE MANIPULATION FUNCTIONS ***/
/**
 * Enqueues a thread onto a queue.
 *
 * @param q the queue to enqueue the thread onto
 * @param thr the thread to enqueue onto the queue
 */
static void
ktqueue_enqueue(ktqueue_t *q, kthread_t *thr)
{
    KASSERT(!thr->kt_wchan);
    list_insert_head(&q->tq_list, &thr->kt_qlink);
    thr->kt_wchan = q;
    q->tq_size++;
}

/**
 * Dequeues a thread from the queue.
 *
 * @param q the queue to dequeue a thread from
 * @return the thread dequeued from the queue
 */
static kthread_t *
ktqueue_dequeue(ktqueue_t *q)
{
    kthread_t *thr;
    list_link_t *link;

    if (list_empty(&q->tq_list))
        return NULL;

    link = q->tq_list.l_prev;
    thr = list_item(link, kthread_t, kt_qlink);
    list_remove(link);
    thr->kt_wchan = NULL;

    q->tq_size--;

    return thr;
}

/**:wq

 * Removes a given thread from a queue.
 *
 * @param q the queue to remove the thread from
 * @param thr the thread to remove from the queue
 */
static void
ktqueue_remove(ktqueue_t *q, kthread_t *thr)
{
    KASSERT(thr->kt_qlink.l_next && thr->kt_qlink.l_prev);
    list_remove(&thr->kt_qlink);
    thr->kt_wchan = NULL;
    q->tq_size--;
}

/*** PUBLIC KTQUEUE MANIPULATION FUNCTIONS ***/
void
sched_queue_init(ktqueue_t *q)
{
    list_init(&q->tq_list);
    q->tq_size = 0;
}

int
sched_queue_empty(ktqueue_t *q)
{
    return list_empty(&q->tq_list);
}

/*
 * Updates the thread's state and enqueues it on the given
 * queue. Returns when the thread has been woken up with wakeup_on or
 * broadcast_on.
 *
 * Use the private queue manipulation functions above.
 */
void
sched_sleep_on(ktqueue_t *q)
{
    /*Assign state to sleep*/
    curthr->kt_state=2;
    KASSERT(q!=NULL);

    /*dbg(DBG_INIT,"Sleeping %d process's thread\n", curthr->kt_proc->p_pid);*/
    /* Insert into the queue */
    ktqueue_enqueue(q, curthr);
    /* Switch threads for other threads to run */
    sched_switch();
}


/*
 * Similar to sleep on, but the sleep can be cancelled.
 *
 * Don't forget to check the kt_cancelled flag at the correct times.
 *
 * Use the private queue manipulation functions above.
 */
int
sched_cancellable_sleep_on(ktqueue_t *q)
{
 if(curthr->kt_cancelled == 1)
    {
        /*dbg(DBG_INIT,"Returning interrupt as %d process's thread is cancelled\n", curthr->kt_proc->p_pid);*/
	return -EINTR;
    }
    /* Assign state as SLEEP_CANCELLABLE */
    curthr->kt_state=3;
   
    /*dbg(DBG_INIT,"Sleeping %d process's thread\n", curthr->kt_proc->p_pid);*/
    /* Sleep on the given queue q */
    ktqueue_enqueue(q, curthr);
    /* Switch threads for other threads to run */
    sched_switch();

  if(curthr->kt_cancelled == 1)
    {
        /* If the thread woken up is cancelled  then return interuppt */
        /*dbg(DBG_INIT,"Returning interrupt as %d process's thread is cancelled which was woken up\n", curthr->kt_proc->p_pid);*/
	return -EINTR;
    }
    return 0;
}

kthread_t *
sched_wakeup_on(ktqueue_t *q)
{
    /*Remove the thread for the given queue*/
    newthread=ktqueue_dequeue(q);
    if(newthread!=NULL)
    {
        /*dbg(DBG_INIT,"Woked up %d process's thread from the queue\n", newthread->kt_proc->p_pid);*/
        KASSERT((newthread->kt_state == KT_SLEEP) || (newthread->kt_state == KT_SLEEP_CANCELLABLE));
        dbg(DBG_INIT,"(GRADING1 4.a) : Current thread to be woken up is in SLEEP or CANCELLABLE SLEEP state.\n");

        /* Setting up interrupt so that the code of execution is uninterrupted */
        priority1=intr_getipl();
        intr_setipl(IPL_HIGH);
        /* Assign the new thread to run state */
        newthread->kt_state=1;
        /* Set the new thread to run queue */
        ktqueue_enqueue(&kt_runq, newthread);
        intr_setipl(priority);
    }
    return newthread;
}

void
sched_broadcast_on(ktqueue_t *q)
{
    /* Remove thread from the given queue */
    newthread=ktqueue_dequeue(q);
    while(newthread!=NULL)
    {
        priority1=intr_getipl();
        intr_setipl(IPL_HIGH);
        newthread->kt_state=1;
        /* Insert it into runQ */
        ktqueue_enqueue(&kt_runq, newthread);

        intr_setipl(priority);
        /* Dequeue again as all the threads has to be removed */
        newthread=ktqueue_dequeue(q);
    }
}

/*
 * If the thread's sleep is cancellable, we set the kt_cancelled
 * flag and remove it from the queue. Otherwise, we just set the
 * kt_cancelled flag and leave the thread on the queue.
 *
 * Remember, unless the thread is in the KT_NO_STATE or KT_EXITED
 * state, it should be on some queue. Otherwise, it will never be run
 * again.
 */
void
sched_cancel(struct kthread *kthr)
{
    KASSERT(kthr!=NULL);
    if(kthr->kt_state == KT_SLEEP_CANCELLABLE)
    {
        kthr->kt_cancelled = 1;
        ktqueue_remove(kthr->kt_wchan,kthr);
        priority1=intr_getipl();
        intr_setipl(IPL_HIGH);
        kthr->kt_state=1;
        ktqueue_enqueue(&kt_runq, kthr);

        intr_setipl(priority1);
    }
    else
    {
        /* Assign the cancelled state */
        kthr->kt_cancelled = 1;
    }

}

/*
 * In this function, you will be modifying the run queue, which can
 * also be modified from an interrupt context. In order for thread
 * contexts and interrupt contexts to play nicely, you need to mask
 * all interrupts before reading or modifying the run queue and
 * re-enable interrupts when you are done. This is analagous to
 * locking a mutex before modifying a data structure shared between
 * threads. Masking interrupts is accomplished by setting the IPL to
 * high.
 *
 * Once you have masked interrupts, you need to remove a thread from
 * the run queue and switch into its context from the currently
 * executing context.
 *
 * If there are no threads on the run queue (assuming you do not have
 * any bugs), then all kernel threads are waiting for an interrupt
 * (for example, when reading from a block device, a kernel thread
 * will wait while the block device seeks). You will need to re-enable
 * interrupts and wait for one to occur in the hopes that a thread
 * gets put on the run queue from the interrupt context.
 *
 * The proper way to do this is with the intr_wait call. See
 * interrupt.h for more details on intr_wait.
 *
 * Note: When waiting for an interrupt, don't forget to modify the
 * IPL. If the IPL of the currently executing thread masks the
 * interrupt you are waiting for, the interrupt will never happen, and
 * your run queue will remain empty. This is very subtle, but
 * _EXTREMELY_ important.
 *
 * Note: Don't forget to set curproc and curthr. When sched_switch
 * returns, a different thread should be executing than the thread
 * which was executing when sched_switch was called.
 *
 * Note: The IPL is process specific.
 */
void
sched_switch(void)
{
    prior1=intr_getipl();
    intr_setipl(IPL_HIGH);
    while(1)
    {
        /* Dequeue the thread to be executed next */
        newthread=ktqueue_dequeue(&kt_runq);
        if(newthread==NULL)
        {
            /* In case of no thread in runQ, wait */
            intr_setipl(IPL_LOW);
            intr_wait();
            intr_setipl(IPL_HIGH);
            continue;
        }
        else
        {
            /* Break out of the loop and context switch */
            break;
        }
    }

    intr_setipl(prior1);
    prevthread=curthr;
    /* Assign the global variable. */
    curthr=newthread;

    curproc = newthread->kt_proc;
    /* Context switch. */
    context_switch(&prevthread->kt_ctx,&newthread->kt_ctx);

}

/*
 * Since we are modifying the run queue, we _MUST_ set the IPL to high
 * so that no interrupts happen at an inopportune moment.

 * Remember to restore the original IPL before you return from this
 * function. Otherwise, we will not get any interrupts after returning
 * from this function.
 *
 * Using intr_disable/intr_enable would be equally as effective as
 * modifying the IPL in this case. However, in some cases, we may want
 * more fine grained control, making modifying the IPL more
 * suitable. We modify the IPL here for consistency.
 */
void
sched_make_runnable(kthread_t *thr)
{
    KASSERT(&kt_runq != thr->kt_wchan);
    dbg(DBG_INIT,"(GRADING1 4.b) : Given thread is not in the kt_runq \n");
    /* Set up the interrupt to prevent the switching to be interuupted. */
    priority= intr_getipl();
    intr_setipl(IPL_HIGH);
    thr->kt_state=1;
    /* Setting the given thread to runq */
    ktqueue_enqueue(&kt_runq, thr);
    /* Assign wchan of the given thread to runQ. */
    thr->kt_wchan=&kt_runq;
    intr_setipl(priority);
    /* Set interrupt to normal level. */
}

