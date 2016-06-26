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
proc_t *new_process;

/*	if(strcmp(name,"Idle_Process"))
	{	new_process=(proc_t*)proc_allocator;}
	else
	{}*/
/*	dbg(DBG_INIT, "\nPROC_CREATE: creating process\n");*/
	new_process=(proc_t*)slab_obj_alloc(proc_allocator);
	KASSERT(new_process!= NULL);
	strncpy(new_process->p_comm,name,PROC_NAME_LEN);

	list_init(&(new_process->p_threads));

	list_init(&(new_process->p_children));

	new_process->p_status=0;
	new_process->p_pid=_proc_getid();

	KASSERT(PID_IDLE != new_process->p_pid || list_empty(&_proc_list)); /* pid can only be PID_IDLE if this is the first process */
	dbg(DBG_INIT,"(GRADING1 2.a)  PID is only idle when this is the first process\n");
        KASSERT(PID_INIT != new_process->p_pid || PID_IDLE == curproc->p_pid); /* pid can only be PID_INIT when creating from idle process */	
	dbg(DBG_INIT,"(GRADING1 2.a)  PID is PID_INIT when creating from idle process\n");
	dbg(DBG_INIT, "\nPROC_CREATE: Created Process ID :%d\n",new_process->p_pid);
	new_process->p_state=PROC_RUNNING;

	if(new_process->p_pid==PID_INIT)
		{proc_initproc=new_process;}	
	
	sched_queue_init(&new_process->p_wait);
	
	list_insert_head(&_proc_list,&new_process->p_list_link);

	new_process->p_pproc=curproc;

	if(new_process->p_pid!=0)

	list_insert_head(&new_process->p_pproc->p_children,&new_process->p_child_link);

	
	new_process->p_pagedir=pt_create_pagedir();

	KASSERT(new_process->p_pagedir!=NULL);

	int fd;	
	for (fd = 0; fd < NFILES; fd++) {
              new_process->p_files[fd]=NULL;
        }
        new_process->p_cwd=vfs_root_vn;
        if (new_process->p_cwd)
        {
                vref(new_process->p_cwd);
        }


        
        return new_process;
}
/**
 * Cleans up as much as the process as can be done from within the
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
	proc_t *p;
	KASSERT(NULL != proc_initproc); /* should have an "init" process */
        dbg(DBG_INIT,"(GRADING1 2.b)  There exists an init process when cleanup is running\n");
	KASSERT(1 <= curproc->p_pid); /* this process should not be idle process */
	dbg(DBG_INIT,"(GRADING1 2.b)  This process is not idle process\n");
        KASSERT(NULL != curproc->p_pproc); /* this process should have parent process */
	dbg(DBG_INIT,"(GRADING1 2.b)  Process getting cleaned up has a parent process(precondition)\n");
	if((curproc->p_pproc->p_wait).tq_size!=0)
	{
		sched_wakeup_on(&(curproc->p_pproc->p_wait));
	}
	if(curproc!=proc_initproc)
	{ 
	 if(!list_empty(&curproc->p_children))
         {    
           list_iterate_begin(&curproc->p_children, p, proc_t, p_child_link) {
	       list_insert_head(&(proc_initproc->p_children),&p->p_child_link);
           	p->p_pproc=proc_initproc;
		} list_iterate_end();
         }}	  
	 curproc->p_state=PROC_DEAD;
 	 curproc->p_status=status;
	KASSERT(NULL != curproc->p_pproc); /* this process should have parent process */
	dbg(DBG_INIT,"(GRADING1 2.b)  This process getting cleaned up still has a parent process(postcondition)\n");
/*	sched_switch();*/
/*	dbg(DBG_INIT,"\n********************************process getting cleaned up %s*********************************\n",curthr->kt_proc->p_comm);*/
return;
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
KASSERT(p!=NULL);
KASSERT(p->p_state!=PROC_DEAD);
/*dbg(DBG_INIT,"\n\n\nProcess to be killed is %d..............current process is %d\n\n\n",p->p_pid,curproc->p_pid);*/
kthread_t *child_thread;
proc_t *child;
	if(p==curproc)

{ 	dbg(DBG_INIT,"\n\n\n killing current process\n\n\n");	
	do_exit(status);
		
		
}
	else 
		 {     		
			/*	if((p->p_pproc->p_wait).tq_size!=0)
			       { sched_wakeup_on(&p->p_pproc->p_wait);}*/
				/* if(curproc==proc_initproc)
			         {
                			 list_iterate_begin(&curproc->p_children, child, proc_t,p_child_link) {
                			 do_waitpid(child->p_pid,0,&status);
                		 	 } list_iterate_end();
         			 }
				*/
          		/*	if(!list_empty(&p->p_children))
         			{	
           			
                                        list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
	                                    list_insert_head(&(proc_initproc->p_children),&child->p_child_link);
         			            child->p_pproc=proc_initproc;
                                         } list_iterate_end();
    			        }*/
       		        	  p->p_state=PROC_DEAD;
       				  p->p_status=status;
				 dbg(DBG_INIT,"\nKilling process with pid %d\n",p->p_pid);

                                list_iterate_begin(&p->p_threads,child_thread, kthread_t, kt_plink) {
                                kthread_cancel(child_thread,(void*)1);

                                
                                } list_iterate_end();
				
				
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
	proc_t *p;
	list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
        if (p->p_pid != PID_IDLE) {
                        proc_kill(p,1);
	                }
         } list_iterate_end();
KASSERT(1);
	 
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
  proc_t *child; 
 KASSERT(curthr->kt_state==KT_EXITED);
	 if(curproc==proc_initproc)
         {
        	/*dbg(DBG_INIT,"inside if...so exiting initproc"); */
/*	        list_iterate_begin(&curproc->p_children, child, proc_t,p_child_link) {
                 do_waitpid(child->p_pid,0,(int*)retval);
		 } list_iterate_end();*/
       	 }
	 proc_cleanup(0);
/*	dbg(DBG_INIT,"\n........in proc thread exited of process %s.......\n",curproc->p_comm);*/
	 sched_switch();
return;
		
   
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
 int flag=0; 
proc_t *child;
kthread_t *child_thread;
        if( list_empty(&curproc->p_children) == 1 ){
           dbg(DBG_INIT,"\nreturning echild\n");
		 return -ECHILD;
        } 
        if(pid!=-1)
	{
/*	dbg(DBG_INIT, "waitpid called by %s process with pid %d", curproc->p_comm,pid);*/
	proc_t* given_p = proc_lookup(pid);

        if(!given_p||given_p->p_pproc->p_pid !=  curproc->p_pid){
           dbg(DBG_INIT,"\nreturning echild\n");
            return -ECHILD;
         
	}
        KASSERT( given_p!=NULL);
	dbg(DBG_INIT,"(GRADING1 2.c)  The given process is not NULL\n");
	KASSERT(-1 == pid || given_p->p_pid == pid);
	dbg(DBG_INIT,"(GRADING1 2.c)  The process's pid is found\n");
	KASSERT(NULL != given_p->p_pagedir);
	dbg(DBG_INIT,"(GRADING1 2.c)  The process's has a page directory \n"); 
	}

        if ( pid == -1 ){
				
         /*  dbg(DBG_INIT,"inside if -1");*/
		list_iterate_begin(&curproc->p_children, child, proc_t, p_child_link) {
			if(child->p_state==PROC_DEAD)
			{
                                   
				list_remove(&child->p_child_link);
 				list_remove(&child->p_list_link);		
				flag=1;
			/*TO DO : destroy data structure of the process*/
			}                
                } list_iterate_end();
         	
		while(flag==0)
			{	
           		/*	dbg(DBG_INIT,"flag=0");*/
				sched_sleep_on(&curproc->p_wait);
/*			dbg(DBG_INIT,"................sleep return curproc is %d........\n ",curproc->p_pid);	*/
			
		  list_iterate_begin(&curproc->p_children, child, proc_t, p_child_link) {
                        if(child->p_state==PROC_DEAD)
                        {
/*				dbg(DBG_INIT,"..............inside if iterating pid %d........",child->p_pid);	*/
                                list_remove(&child->p_child_link);
                                list_remove(&child->p_list_link);
                        	flag=1;
				/*TO DO : destroy data structure of the process*/
                        }
                } list_iterate_end();
/*			dbg(DBG_INIT,"................return pid  is %d.........\n ",child->p_pid);	*/

		} *status=child->p_status;	return child->p_pid;
 		  /* Check if exited children exist of the current process
                // If present then dispose one and return its exit status
            //Othewise
                // Block on its p_wait queue until one exits*/
        	} 
	else if ( pid > 0 ) {
     		list_iterate_begin(&curproc->p_children, child, proc_t, p_child_link) {
                if(child->p_pid==pid)
                { 	
                     while( (child->p_state!=PROC_DEAD) )
		     { sched_sleep_on(&curproc->p_wait);
/*			dbg(DBG_INIT,"\n..caught in while loop.....waiting for %s ...\n ",child->p_comm);*/
			}
  
		     
			{	list_remove(&child->p_child_link);
                        	list_remove(&child->p_list_link);
			 list_iterate_begin(&child->p_threads,child_thread, kthread_t, kt_plink) {
		                KASSERT(KT_EXITED == child_thread->kt_state);
				dbg(DBG_INIT,"(GRADING1 2.c)  child thread is a thread to be destroyed\n");
				kthread_destroy(child_thread);
                       	 		
				/*TO DO : destroy data structure of the process*/
		            } list_iterate_end();
				
			}	
                }
               
                } list_iterate_end();
 
        } 
*status=child->p_status;
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
	 proc_t *child;  
  	 
	 kthread_exit((void*)status);
	
/*	 do_waitpid(curproc->pproc,curproc->p_pid,	
	 //   NOT_YET_IMPLEMENTED("PROCS: do_exit");*/

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
