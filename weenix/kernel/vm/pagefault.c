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

#include "mm/tlb.h"

static int has_valid_permissions(vmarea_t *vma, uint32_t cause){
    if (vma->vma_prot & PROT_NONE){
        return 0;
    }

    if (!((cause & FAULT_WRITE) || (cause & FAULT_EXEC))
            && !(vma->vma_prot & PROT_READ)){
        return 0;
    }

    if ((cause & FAULT_WRITE) && !(vma->vma_prot & PROT_WRITE)){
        return 0;
    }

    if ((cause & FAULT_EXEC) && !(vma->vma_prot & PROT_EXEC)){
        return 0;
    }

    return 1;
}

size_t
pt_mapping_info(const void *pt, char *buf, size_t osize);
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
    KASSERT(cause & FAULT_USER);

    vmarea_t *vma = vmmap_lookup(curproc->p_vmmap, ADDR_TO_PN(vaddr));

    if (vma == NULL){
        dbg(DBG_VMMAP, "\n\n\nhere!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");
        dbginfo(DBG_VMMAP, vmmap_mapping_info, curproc->p_vmmap);
        dbg(DBG_VMMAP, "\n");
        do_exit(EFAULT);
        panic("returned from do_exit");
    }

    if(!has_valid_permissions(vma, cause)){
        do_exit(EFAULT);
        panic("returned from do_exit");
    }

    pframe_t *p;
    int forwrite = (cause & FAULT_WRITE) ? 1 : 0;
    int lookup_res = pframe_lookup(vma->vma_obj, ADDR_TO_PN(vaddr) -
            vma->vma_start + vma->vma_off, forwrite, &p);   

    if (lookup_res < 0){
        do_exit(EFAULT);
        panic("returned from do_exit");
    }

    if (cause & FAULT_WRITE){
        pframe_pin(p);
        int dirty_res = pframe_dirty(p);
        pframe_unpin(p);

        if (dirty_res < 0){
            do_exit(EFAULT);
            panic("returned from do_exit");
        }
    }

    int pdflags = PD_PRESENT | PD_USER;

    int ptflags = PT_PRESENT | PT_USER;

    if (cause & FAULT_WRITE){
        pdflags |= PD_WRITE;
        ptflags |= PT_WRITE;
    }

    pt_map(curproc->p_pagedir,
           (uintptr_t) PAGE_ALIGN_DOWN(vaddr),
           pt_virt_to_phys((uintptr_t) p->pf_addr), pdflags, ptflags);

    tlb_flush_all();
    /* TODO flush TLB (?) */
}
