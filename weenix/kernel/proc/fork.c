#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{
        /*NOT_YET_IMPLEMENTED("VM: do_fork");*/
	int i = 0;

	KASSERT(regs != NULL);
	dbg(DBG_PRINT, "GRADING3A 7.a\n");
        KASSERT(curproc != NULL);
	dbg(DBG_PRINT, "GRADING3A 7.a\n");
        KASSERT(curproc->p_state == PROC_RUNNING);
	dbg(DBG_PRINT, "GRADING3A 7.a\n");
	
	proc_t *child = proc_create("child");
	if(child == NULL){
		return -1;
	}
	KASSERT(child->p_state == PROC_RUNNING);
	dbg(DBG_PRINT, "GRADING3A 7.a\n");
	KASSERT(child->p_pagedir != NULL);
	dbg(DBG_PRINT, "GRADING3A 7.a\n");

	child->p_vmmap=vmmap_clone(curproc->p_vmmap);
	dbginfo(DBG_VMMAP, vmmap_mapping_info, curproc->p_vmmap);
	list_link_t *pList = &(curproc->p_vmmap->vmm_list);
	list_link_t *cList = &(child->p_vmmap->vmm_list);
	list_link_t *pLink;
	list_link_t *cLink;
	vmarea_t *aParent;
	vmarea_t *aChild;	
	for(pLink = pList->l_next, cLink = cList->l_next;
		pList != pLink; pLink = pLink->l_next, cLink = cLink->l_next){
			
		aParent = list_item(pLink, vmarea_t, vma_plink);
		aChild = list_item(cLink, vmarea_t, vma_plink);
		
		if(aParent->vma_flags == MAP_PRIVATE){
	
			mmobj_t *Child_shadowobj = shadow_create();
			mmobj_t *Parent_shadowobj = shadow_create();
			Child_shadowobj->mmo_shadowed = aParent->vma_obj;
			Parent_shadowobj->mmo_shadowed = aParent->vma_obj;
			aChild->vma_obj = Child_shadowobj;
			aParent->vma_obj = Parent_shadowobj;

			pt_unmap_range(curproc->p_pagedir,(uintptr_t) PN_TO_ADDR(aParent->vma_start), (uintptr_t)PN_TO_ADDR(aChild->vma_end) ); 
			tlb_flush_all();
		}else if(aParent->vma_flags == MAP_SHARED){
			aChild->vma_obj = aParent->vma_obj;
		}
		/*aChild->vma_obj->mmo_ops->ref(aChild->vma_obj);*/
	}
		dbginfo(DBG_VMMAP, vmmap_mapping_info, child->p_vmmap);
	child->p_start_brk = curproc->p_start_brk;
	child->p_brk = curproc->p_brk;
	for(; i < NFILES; i++){
		child->p_files[i] = curproc->p_files[i];
		if(NULL != child->p_files[i]){
			fref(child->p_files[i]);
		}
	}
	child->p_cwd=curproc->p_cwd;
	vref(child->p_cwd);
	kthread_t *childthread=kthread_clone(curthr);

	KASSERT(childthread->kt_kstack != NULL);
	dbg(DBG_PRINT, "GRADING3A 7.a\n");

	childthread->kt_proc=child;
	list_insert_tail(&(child->p_threads), &(childthread->kt_plink));
	(childthread->kt_ctx).c_eip = (uint32_t)userland_entry;
	regs->r_eax = 0;
	(childthread->kt_ctx).c_esp = fork_setup_stack(regs, childthread->kt_kstack); 
	(childthread->kt_ctx).c_pdptr = childthread->kt_proc->p_pagedir;
        (childthread->kt_ctx).c_kstack = (uintptr_t)childthread->kt_kstack;
 	(childthread->kt_ctx).c_kstacksz = DEFAULT_STACK_SIZE;

	sched_make_runnable(childthread);

	regs->r_eax=child->p_pid;
	return child->p_pid;
}
