#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
	if(sizeof(len)==NULL || sizeof(off)==NULL || PAGE_ALIGNED(addr)==0){
		return -EINVAL;
	}
	if(! ( (flags & MAP_SHARED) 
		|| (flags & MAP_PRIVATE) 
			|| (flags & MAP_FIXED)
				 || (flags & MAP_ANON )) ){
		return EINVAL;
	}
	if( (flags & MAP_SHARED)
		 && (flags & MAP_PRIVATE) ){
		
		return EINVAL;
	}
	vnode_t *vn = NULL;
	if( !(flags & MAP_ANON) && (fd < 0 || fd > NFILES)){
			return EBADF;
	}else{
		vn = curproc->p_files[fd]->f_vnode; 
	}
	file_t *f=fget(-1);
	if(f == NULL){
		return ENOMEM;
	}
	if( !(curproc->p_files[fd]->f_mode & FMODE_READ) 
		&& (flags & MAP_PRIVATE) ){
		return EACCES;
	}
	/*	
	if( !( (flags & MAP_SHARED) 
		&& (prot & PROT_WRITE) 
			&& (curproc->p_files[fd]->f_mode & (FMODE_WRITE || FMODE_READ)) ) ){
		return -EACCES;
	}
	if( !( (flags & MAP_SHARED) 
		&& (prot & PROT_WRITE) 
			&& (curproc->p_files[fd]->f_mode & FMODE_APPEND)) ){
		return -EACCES;
	}
	*/
	
	/*TODO*/
	int dir = 0;
	if(len >= PAGE_SIZE){
		len = (size_t)ADDR_TO_PN(len);
		/*
		if(len % PAGE_SIZE){
			len = len / PAGE_SIZE;
		}else{
			len = len / P
		}
		*/
	}
	/*TODO:Handle EPERM and flush the TLB */
	int result=vmmap_map(curproc->p_vmmap, vn, (uintptr_t)ADDR_TO_PN(addr), (uintptr_t)len, prot, flags, off, VMMAP_DIR_LOHI,(vmarea_t **)ret);
        /*NOT_YET_IMPLEMENTED("VM: do_mmap");*/
	tlb_flush_range((uintptr_t)ADDR_TO_PN(addr), (uintptr_t)(len) );
	KASSERT(NULL != curproc->p_pagedir);
	dbg(DBG_PRINT, "GRADING3A 2.a\n");
        return result;
}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
	if( PAGE_ALIGNED(addr)==0 || sizeof(len)== 0){
		return -EINVAL;
	}
	if(len >= PAGE_SIZE){
		len = (size_t)ADDR_TO_PN(len);
	}
	int result=vmmap_remove(curproc->p_vmmap, ADDR_TO_PN(addr),(uint32_t) len);
	if(result == 0){
		return -EINVAL;
	}
	
	tlb_flush((uintptr_t)addr);
		
	KASSERT(NULL != curproc->p_pagedir);	
	dbg(DBG_PRINT, "GRADING3A 2.b\n");
	return result;
       /* NOT_YET_IMPLEMENTED("VM: do_munmap");*/
}

