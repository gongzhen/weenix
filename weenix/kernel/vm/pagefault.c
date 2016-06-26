#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/proc.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/pagetable.h"

#include "vm/pagefault.h"
#include "vm/vmmap.h"

/*
 * This gets called by _pt_fault_handler in mm/pagetable.c The
 * calling function has already done a lot of error checking for
 * us. In particular it has checked that we are not page faulting
 * while in kernel mode. Make sure you understand why an
 * unexpected page fault in kernel mode is bad in Weenix. You
 * should probably read the _pt_fault_handler function to get a
 * sense of what it is doing.
 *
 * Before you can do anything you need to find the vmarea that
 * contains the address that was faulted on. Make sure to check
 * the permissions on the area to see if the process has
 * permission to do [cause]. If either of these checks does not
 * pass kill the offending process, setting its exit status to
 * EFAULT (normally we would send the SIGSEGV signal, however
 * Weenix does not support signals).
 *
 * Now it is time to find the correct page (don't forget
 * about shadow objects, especially copy-on-write magic!). Make
 * sure that if the user writes to the page it will be handled
 * correctly.
 *
 * Finally call pt_map to have the new mapping placed into the
 * appropriate page table.
 *
 * @param vaddr the address that was accessed to cause the fault
 *
 * @param cause this is the type of operation on the memory
 *              address which caused the fault, possible values
 *              can be found in pagefault.h
 */
void
handle_pagefault(uintptr_t vaddr, uint32_t cause)
{
        /*NOT_YET_IMPLEMENTED("VM: handle_pagefault");*/
	vmmap_t *map = curproc->p_vmmap;
	dbginfo(DBG_ERROR, proc_info, curproc);
	dbginfo(DBG_ERROR, proc_list_info, NULL);
	if(vaddr == NULL){
	}	
	vmarea_t *vma =	vmmap_lookup(map, ADDR_TO_PN(vaddr));
	
	/*uintptr_t pagenum = PAGE_OFFSET(vaddr);*/
	if(vma == NULL ||  !(cause & FAULT_USER)){
		/*XXX permission checks*/
		curproc->p_status = EFAULT;
		proc_kill(curproc, EFAULT);
	}
	pframe_t *pf;
	uintptr_t pagenum =  ADDR_TO_PN(vaddr) - vma->vma_start+vma->vma_off;
	/*XXX handle shadow objects*/	
	/*
	int forWrite = 0;
	if(cause & FAULT_WRITE){
		forWrite = 1;
	}
	*/		
	/*
	if(vma->vma_obj->mmo_shadowed != NULL){
		shadow_lookuppage(vma->vma_obj->mmo_shadowed, pagenum, forWrite,&pf);
	}else{
	*/
		pframe_get(vma->vma_obj, pagenum, &pf);
	/*}*/
	uintptr_t paddr = pt_virt_to_phys((uintptr_t)pf->pf_addr);
	uintptr_t pdflags = PD_PRESENT | PD_WRITE | PD_USER;
	uintptr_t ptflags = PT_PRESENT | PT_WRITE | PT_USER;
	/*XXX tlb flush?*/
	pt_map(curproc->p_pagedir,(uintptr_t)PAGE_ALIGN_DOWN(vaddr), paddr, pdflags, ptflags);

}
