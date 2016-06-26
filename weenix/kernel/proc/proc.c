#include "kernel.h"
#include "config.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/proc.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/mm.h"
#include "mm/mman.h"

#include "vm/vmmap.h"

#include "fs/vfs.h"
#include "fs/vfs_syscall.h"
#include "fs/vnode.h"
#include "fs/file.h"


proc_t *curproc = NULL; /* global */
static slab_allocator_t *proc_allocator = NULL;

static list_t _proc_list;
static proc_t *proc_initproc = NULL; /* Pointer to the init process (PID 1) */

void
proc_init()
{
        list_init(&_proc_list);
        proc_allocator = slab_allocator_create("proc", sizeof(proc_t));
        KASSERT(proc_allocator != NULL);
}

static pid_t next_pid = 0;

/**
 * Returns the next available PID.
 *
 * Note: Where n is the number of running processes, this algorithm is
 * worst case O(n^2). As long as PIDs never wrap around it is O(n).
 *
 * @return the next available PID
 */
static int
_proc_getid()
{
        proc_t *p;
        pid_t pid = next_pid;
        while (1) {
failed:
                list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                        if (p->p_pid == pid) {
                                if ((pid = (pid + 1) % PROC_MAX_COUNT) == next_pid) {
                                        return -1;
                                } else {
                                        goto failed;
                                }
                        }
                } list_iterate_end();
                next_pid = (pid + 1) % PROC_MAX_COUNT;
                return pid;
        }
}

/*
 * The new process, although it isn't really running since it has no
 * threads, should be in the PROC_RUNNING state.
 *
 * Don't forget to set proc_initproc when you create the init
 * process. You will need to be able to reference the init process
 * when reparenting processes to the init process.
 */
proc_t *
proc_create(char *name)
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_create");*/
	
	proc_t *parentProc = NULL;
	proc_t *proc = NULL;
	if(curproc != NULL){
		parentProc = curproc;
	}
	proc =(proc_t *)slab_obj_alloc(proc_allocator);
	list_init(&(proc->p_threads));
	list_init(&(proc->p_children));
	list_link_init(&proc->p_list_link);
	list_link_init(&proc->p_child_link);
	sched_queue_init(&(proc->p_wait));
	proc->p_pid = _proc_getid();
	pid_t pid = proc->p_pid;
	proc->p_cwd = NULL;
	vmmap_t *map = vmmap_create();
	map->vmm_proc = proc;
	proc->p_vmmap = map;
	
	if(proc->p_pid == PID_INIT){
                proc_initproc = proc;
        }

	int i;
	for(i=0;i<NFILES;i++)
        {
                proc->p_files[i]=NULL;
        }
        /*FIXME: struct initialization */
	KASSERT(PID_IDLE != pid || list_empty(&_proc_list)); /* pid can only be PID_IDLE if this is the first process */
	dbg_print("GRADING1 2.a PASSED: pid can only be PID_IDLE if this is the first process.\n");

	KASSERT(PID_INIT != pid || PID_IDLE == parentProc->p_pid); /* pid can only be PID_INIT when creating from idle process */
	dbg_print("GRADING2 2.a PASSED: pid can only be PID_INIT when creating from idle process.\n");

	proc->p_pagedir=pt_create_pagedir();
	proc->p_state = PROC_RUNNING;
	proc->p_status = 0;
	/*list_init(&(proc->p_wait.tq_list));*/

	strncpy(proc->p_comm, name, PROC_NAME_LEN); /*null character added?TODO*/
	proc->p_pproc = parentProc;
	if(parentProc != NULL){
		list_insert_tail(&(parentProc->p_children), &(proc->p_child_link));
	}
	list_insert_tail(&(_proc_list), &(proc->p_list_link));
        
	return proc;
}

/**
 * Cleans up as much as the process as can be done from within the
 * process. This involves:
 *    - Closing all open files (VFS)
 *    - Cleaning up VM mappings (VM)
 *    - Waking up its parent if it is waiting
 *    - Reparenting any children to the init process
 *    - Setting its status and state appropriately
 *
 * The parent will finish destroying the process within do_waitpid (make
 * sure you understand why it cannot be done here). Until the parent
 * finishes destroying it, the process is informally called a 'zombie'
 * process.
 *
 * This is also where any children of the current process should be
 * reparented to the init process (unless, of course, the current
 * process is the init process. However, the init process should not
 * have any children at the time it exits).
 *
 * Note: You do _NOT_ have to special case the idle process. It should
 * never exit this way.
 *
 * @param status the status to exit the process with
 */
void
proc_cleanup(int status)
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_cleanup");*/
	KASSERT(NULL != proc_initproc); /* should have an "init" process */
	dbg_print("GRADING 2.b PASSED: should have an init process.\n");
	KASSERT(1 <= curproc->p_pid); /* this process should not be idle process */
	dbg_print("GRADING 2.b PASSED: this process should not be idle process.\n");
        KASSERT(NULL != curproc->p_pproc); /* this process should have parent process */
	dbg_print("GRADING 2.b PASSED: this process should have parent process.\n");

	proc_t *myChildProc = NULL;
	proc_t *myParentProc = curproc->p_pproc;
	/*TODO wake up myParentProc, if it is waiting*/
	sched_wakeup_on(&(myParentProc->p_wait));
	int fd;
	if(curproc->p_cwd != NULL){
		vput(curproc->p_cwd);
	}		
        for(fd=0;fd<NFILES;fd++)
        {
              if(curproc->p_files[fd]!=NULL)
              {
                 do_close(fd);
              }
        }
	if(curproc->p_vmmap != NULL){
		vmmap_destroy(curproc->p_vmmap);
	}
	list_t *list = &(curproc->p_children);
	list_link_t *link = NULL;
	for( link = list->l_next; link != list; link = list->l_next ){
		myChildProc = list_item(link, proc_t, p_child_link);	
		list_remove(&(myChildProc->p_child_link));
		list_insert_tail(&proc_initproc->p_children, &(myChildProc->p_child_link));
		myChildProc->p_pproc = proc_initproc;
	}
	
	curproc->p_state = PROC_DEAD;
	curproc->p_status = status;

	KASSERT(NULL != curproc->p_pproc); /* this process should have parent process */
	dbg_print("GRADING 2.b PASSED: this process should have parent process.\n");


}

/*
 * This has nothing to do with signals and kill(1).
 *
 * Calling this on the current process is equivalent to calling
 * do_exit().
 *
 * In Weenix, this is only called from proc_kill_all.
 */
void
proc_kill(proc_t *p, int status)
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_kill");*/
	int fd;
        for(fd=0;fd<NFILES;fd++)
        {
              if(curproc->p_files[fd]!=NULL)
              {
                 int fd=do_close(fd);
              }
        }
	if(NULL != curproc->p_vmmap){
		vmmap_destroy(curproc->p_vmmap);
	}
	if(p == curproc){
		do_exit(status);	
	}else{
		list_t *list = &(p->p_threads);
		list_link_t *link = NULL;
		kthread_t *pThread = NULL;
		for(link = list->l_next; link !=list; link= link->l_next){
			pThread = list_item(link, kthread_t, kt_plink);
			kthread_cancel(pThread, (void *)0);	
		}	
	}
}

/*
 * Remember, proc_kill on the current process will _NOT_ return.
 * Don't kill direct children of the idle process.
 *
 * In Weenix, this is only called by sys_halt.
 */
void
proc_kill_all()
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_kill_all");*/
	if( list_empty(&_proc_list) != 1){
		list_link_t *link = NULL;
		list_t *list =& _proc_list;
		proc_t *pProc = NULL;
		for(link = _proc_list.l_next; link != list; link = link->l_next){
			pProc = list_item(link, proc_t, p_list_link);
			if(pProc->p_pid == PID_IDLE || pProc->p_pproc->p_pid == PID_IDLE){
				continue;	
			}
	
			proc_kill(pProc, pProc->p_status);/* status set pending TODO*/	
		} 
	}
}

proc_t *
proc_lookup(int pid)
{
        proc_t *p;
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                if (p->p_pid == pid) {
                        return p;
                }
        } list_iterate_end();
        return NULL;
}

list_t *
proc_list()
{
        return &_proc_list;
}

/*
 * This function is only called from kthread_exit.
 *
 * Unless you are implementing MTP, this just means that the process
 * needs to be cleaned up and a new thread needs to be scheduled to
 * run. If you are implementing MTP, a single thread exiting does not
 * necessarily mean that the process should be exited.
 */
void
proc_thread_exited(void *retval)
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_thread_exited");*/
	proc_cleanup(*(int *)retval);
	sched_switch();
}

/* If pid is -1 dispose of one of the exited children of the current
 * process and return its exit status in the status argument, or if
 * all children of this process are still running, then this function
 * blocks on its own p_wait queue until one exits.
 *
 * If pid is greater than 0 and the given pid is a child of the
 * current process then wait for the given pid to exit and dispose
 * of it.
 *
 * If the current process has no children, or the given pid is not
 * a child of the current process return -ECHILD.
 *
 * Pids other than -1 and positive numbers are not supported.
 * Options other than 0 are not supported.
 */
pid_t
do_waitpid(pid_t pid, int options, int *status)
{
        /*NOT_YET_IMPLEMENTED("PROCS: do_waitpid");*/
	proc_t *pProc = NULL;

	if(list_empty(&curproc->p_children) == 1){
		return -ECHILD;
	}
	 
	
	KASSERT(NULL != curproc); /* the process should not be NULL */
	dbg_print("GRADING1 2.c PASSED: the process should not be NULL.\n");

        
	if(pid == -1){

		KASSERT(-1 == pid || pProc->p_pid == pid); /* should be able to find the process */
		dbg_print("GRADING1 2.c PASSED: should be able to find the process.\n");

		while(1)
		{
			pProc = curproc;
			proc_t *child = NULL;
			proc_t *deadChild = NULL;
			list_t *list = &(pProc->p_children);
			list_link_t *link=NULL;
			for(link = list->l_next; link != list; link = link->l_next){
				child = list_item(link, proc_t, p_child_link);
				if(child->p_state == PROC_DEAD){
					deadChild = child;
					if(status != NULL){
						*status = deadChild->p_status;
					}
					break;
				}
			}
			if(deadChild != NULL){
		
				KASSERT(NULL != deadChild->p_pagedir); /* this process should have pagedir */
				dbg_print("GRADING1 2.c PASSED: this process should have pagedir.\n");
				kthread_t *pThread = NULL;
				list_iterate_begin(&deadChild->p_threads, pThread, kthread_t, kt_plink){	
						KASSERT(KT_EXITED == pThread->kt_state);/* thr points to a thread to be destroied */ 
						dbg_print("GRADING1 2.c PASSED: thr points to a thread to be destroyed.\n");
						kthread_destroy(pThread);	
					}list_iterate_end();	
				

				pt_destroy_pagedir(deadChild->p_pagedir);
				list_remove(&deadChild->p_child_link);
				list_remove(&deadChild->p_list_link);
				slab_obj_free(proc_allocator,(void *)deadChild);
				
				return (pid_t)deadChild->p_pid;
			}else{
				
				sched_sleep_on(&(curproc->p_wait));
			
			}
		}	
	}
	else 
	if(pid > 0){
		
		pProc = proc_lookup(pid);
		if(pProc == NULL || pProc->p_pproc != curproc){
			return -ECHILD;
		}

		KASSERT(-1 == pid || pProc->p_pid == pid); /* should be able to find the process */
		dbg_print("GRADING1 2.c PASSED: should be able to find the process.\n");


		if(pProc->p_pproc == curproc){
			while(1){
				pProc = curproc;
				proc_t *child = NULL;
				proc_t *deadChild = NULL;
				list_t *list = &(pProc->p_children);
				list_link_t *link=NULL;

				for(link = list->l_next; link != list; link = link->l_next){
					child = list_item(link, proc_t, p_child_link);
					if(child->p_state == PROC_DEAD){
						deadChild = child;
						if(status != NULL){
							*status = deadChild->p_status;
						}
						break;
					}
				}
				if(deadChild!=NULL){
					KASSERT(NULL != deadChild->p_pagedir); /* this process should have pagedir */
					dbg_print("GRADING1 2.c PASSED: this process should have pagedir.\n");
				
					kthread_t *pThread = NULL;
					
					list_iterate_begin(&deadChild->p_threads, pThread, kthread_t, kt_plink){	
						KASSERT(KT_EXITED == pThread->kt_state);/* thr points to a thread to be destroied */ 
						dbg_print("GRADING1 2.c PASSED: thr points to a thread to be destroyed.\n");
						kthread_destroy(pThread);	
					}list_iterate_end();	

					pt_destroy_pagedir(deadChild->p_pagedir);
					list_remove(&deadChild->p_child_link);
					list_remove(&deadChild->p_list_link);
					slab_obj_free(proc_allocator, (void *)deadChild);
				
					return deadChild->p_pid;
				}else{
					sched_sleep_on(&curproc->p_wait);
				}	
			}
		}
		else {
			return -ECHILD;
		}
	}
	
		return pid;
}

/*
 * Cancel all threads, join with them, and exit from the current
 * thread.
 *
 * @param status the exit status of the process
 */
void
do_exit(int status)
{
        /*NOT_YET_IMPLEMENTED("PROCS: do_exit");*/	
	int *pStatus = &status;
	list_t *list = &(curproc->p_threads);
	kthread_t *pThread = NULL;
	list_link_t *link=NULL;
	for( link=list->l_next; link != list; link=link->l_next){
		
		pThread = list_item(link, kthread_t, kt_plink);	
		if(pThread != curthr){
			kthread_cancel(pThread, &status);
			/*kthread_join(pThread, (void **) &pStatus);*/
		}
	}		
	kthread_exit(&status);
}

size_t
proc_info(const void *arg, char *buf, size_t osize)
{
        const proc_t *p = (proc_t *) arg;
        size_t size = osize;
        proc_t *child;

        KASSERT(NULL != p);
        KASSERT(NULL != buf);

        iprintf(&buf, &size, "pid:          %i\n", p->p_pid);
        iprintf(&buf, &size, "name:         %s\n", p->p_comm);
        if (NULL != p->p_pproc) {
                iprintf(&buf, &size, "parent:       %i (%s)\n",
                        p->p_pproc->p_pid, p->p_pproc->p_comm);
        } else {
                iprintf(&buf, &size, "parent:       -\n");
        }

#ifdef __MTP__
        int count = 0;
        kthread_t *kthr;
        list_iterate_begin(&p->p_threads, kthr, kthread_t, kt_plink) {
                ++count;
        } list_iterate_end();
        iprintf(&buf, &size, "thread count: %i\n", count);
#endif

        if (list_empty(&p->p_children)) {
                iprintf(&buf, &size, "children:     -\n");
        } else {
                iprintf(&buf, &size, "children:\n");
        }
        list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
                iprintf(&buf, &size, "     %i (%s)\n", child->p_pid, child->p_comm);
        } list_iterate_end();

        iprintf(&buf, &size, "status:       %i\n", p->p_status);
        iprintf(&buf, &size, "state:        %i\n", p->p_state);

#ifdef __VFS__
#ifdef __GETCWD__
        if (NULL != p->p_cwd) {
                char cwd[256];
                lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                iprintf(&buf, &size, "cwd:          %-s\n", cwd);
        } else {
                iprintf(&buf, &size, "cwd:          -\n");
        }
#endif /* __GETCWD__ */
#endif

#ifdef __VM__
        iprintf(&buf, &size, "start brk:    0x%p\n", p->p_start_brk);
        iprintf(&buf, &size, "brk:          0x%p\n", p->p_brk);
#endif

        return size;
}

size_t
proc_list_info(const void *arg, char *buf, size_t osize)
{
        size_t size = osize;
        proc_t *p;

        KASSERT(NULL == arg);
        KASSERT(NULL != buf);

#if defined(__VFS__) && defined(__GETCWD__)
        iprintf(&buf, &size, "%5s %-13s %-18s %-s\n", "PID", "NAME", "PARENT", "CWD");
#else
        iprintf(&buf, &size, "%5s %-13s %-s\n", "PID", "NAME", "PARENT");
#endif

        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                char parent[64];
                if (NULL != p->p_pproc) {
                        snprintf(parent, sizeof(parent),
                                 "%3i (%s)", p->p_pproc->p_pid, p->p_pproc->p_comm);
                } else {
                        snprintf(parent, sizeof(parent), "  -");
                }

#if defined(__VFS__) && defined(__GETCWD__)
                if (NULL != p->p_cwd) {
                        char cwd[256];
                        lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                        iprintf(&buf, &size, " %3i  %-13s %-18s %-s\n",
                                p->p_pid, p->p_comm, parent, cwd);
                } else {
                        iprintf(&buf, &size, " %3i  %-13s %-18s -\n",
                                p->p_pid, p->p_comm, parent);
                }
#else
                iprintf(&buf, &size, " %3i  %-13s %-s\n",
                        p->p_pid, p->p_comm, parent);
#endif
        } list_iterate_end();
        return size;
}
