#include "globals.h"
#include "errno.h"
#include "util/debug.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/mman.h"

#include "vm/mmap.h"
#include "vm/vmmap.h"

#include "proc/proc.h"

/*
 * This function implements the brk(2) system call.
 *
 * This routine manages the calling process's "break" -- the ending address
 * of the process's "dynamic" region (often also referred to as the "heap").
 * The current value of a process's break is maintained in the 'p_brk' member
 * of the proc_t structure that represents the process in question.
 *
 * The 'p_brk' and 'p_start_brk' members of a proc_t struct are initialized
 * by the loader. 'p_start_brk' is subsequently never modified; it always
 * holds the initial value of the break. Note that the starting break is
 * not necessarily page aligned!
 *
 * 'p_start_brk' is the lower limit of 'p_brk' (that is, setting the break
 * to any value less than 'p_start_brk' should be disallowed).
 *
 * The upper limit of 'p_brk' is defined by the minimum of (1) the
 * starting address of the next occuring mapping or (2) USER_MEM_HIGH.
 * That is, growth of the process break is limited only in that it cannot
 * overlap with/expand into an existing mapping or beyond the region of
 * the address space allocated for use by userland. (note the presence of
 * the 'vmmap_is_range_empty' function).
 *
 * The dynamic region should always be represented by at most ONE vmarea.
 * Note that vmareas only have page granularity, you will need to take this
 * into account when deciding how to set the mappings if p_brk or p_start_brk
 * is not page aligned.
 *
 * You are guaranteed that the process data/bss region is non-empty.
 * That is, if the starting brk is not page-aligned, its page has
 * read/write permissions.
 *
 * If addr is NULL, you should NOT fail as the man page says. Instead,
 * "return" the current break. We use this to implement sbrk(0) without writing
 * a separate syscall. Look in user/libc/syscall.c if you're curious.
 *
 * Also, despite the statement on the manpage, you MUST support combined use
 * of brk and mmap in the same process.
 *
 * Note that this function "returns" the new break through the "ret" argument.
 * Return 0 on success, -errno on failure.
 */
int
do_brk(void *addr, void **ret)
{


	/*
		find the dynamic region using existing brk
		check 
	*/
	
	uintptr_t *old = curproc->p_brk;
	vmmap_t *map = curproc->p_vmmap;
	vmarea_t *old_vma = vmmap_lookup(map, ADDR_TO_PN(old));
	vmarea_t *vma;
	if(addr == NULL){
		*ret = old;
		return 0;
	}
	if( ((uintptr_t)addr < (uintptr_t)curproc->p_start_brk)
		 || ( (uintptr_t)addr > (uintptr_t)USER_MEM_HIGH)){
		return ENOMEM;
	}

	if(old_vma == NULL){
		return -1;
	}

	if(ADDR_TO_PN(addr) < old_vma->vma_end){
		/*XXX addr not page aligned*/	
		*ret = addr;
	}else{
		uintptr_t *taddr=addr;
		if(!PAGE_ALIGNED(addr)){
			taddr = PAGE_ALIGN_UP(addr);
		}
		vmmap_map(map, NULL, old_vma->vma_end, ((uintptr_t) ADDR_TO_PN(taddr) - old_vma->vma_end)+1 ,old_vma->vma_prot, old_vma->vma_flags, old_vma->vma_off, VMMAP_DIR_LOHI, &vma);
		curproc->p_brk=addr;
		*ret = addr;
	}

	/*	
	if( next_map_addr != NULL && (next_map_addr < (uintptr_t)USER_MEM_HIGH) ){
		next_map_addr = ADDR_TO_PN(USER_MEM_HIGH);
	}
	if(next_map_addr != NULL &&
		 ( ((uintptr_t *)addr < start_brk) ||  ((uintptr_t *)addr > next_map_addr)) ){
		return -1;
	}
	
	if(!PAGE_ALIGNED(addr)){
		addr = PAGE_ALIGN_DOWN(addr);	
	}
	*ret = addr;
	*/
        return 0;
}
