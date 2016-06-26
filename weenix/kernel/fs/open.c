/*
 *  FILE: open.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Mon Apr  6 19:27:49 1998
 */

#include "globals.h"
#include "errno.h"
#include "fs/fcntl.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/stat.h"
#include "util/debug.h"

/* find empty index in p->p_files[] */
int
get_empty_fd(proc_t *p)
{
        int fd;

        for (fd = 0; fd < NFILES; fd++) {
                if (!p->p_files[fd])
                        return fd;
        }

   dbg(DBG_ERROR | DBG_VFS, "ERROR: get_empty_fd: out of file descriptors for pid %d\n", curproc->p_pid);
        return -EMFILE;
}

/*
 * There a number of steps to opening a file:
 *      1. Get the next empty file descriptor.
 *      2. Call fget to get a fresh file_t.
 *      3. Save the file_t in curproc's file descriptor table.
 *      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
 *         oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
 *         O_APPEND.
 *      5. Use open_namev() to get the vnode for the file_t.
 *      6. Fill in the fields of the file_t.
 *      7. Return new fd.
 *
 * If anything goes wrong at any point (specifically if the call to open_namev
 * fails), be sure to remove the fd from curproc, fput the file_t and return an
 * error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        oflags is not valid.
 *      o EMFILE
 *        The process already has the maximum number of files open.
 *      o ENOMEM
 *        Insufficient kernel memory was available.
 *      o ENAMETOOLONG
 *        A component of filename was too long.
 *      o ENOENT
 *        O_CREAT is not set and the named file does not exist.  Or, a
 *        directory component in pathname does not exist.
 *      o EISDIR
 *        pathname refers to a directory and the access requested involved
 *        writing (that is, O_WRONLY or O_RDWR is set).
 *      o ENXIO
 *        pathname refers to a device special file and no corresponding device
 *        exists.
 */

int
do_open(const char *filename, int oflags)
{
        /*NOT_YET_IMPLEMENTED("VFS: do_open");*/
        if(strlen(filename) > NAME_LEN){
            return -ENAMETOOLONG;
        }
        /*Get free descriptor of current process*/
        int fd = get_empty_fd(curproc);
        if(fd == -EMFILE){
            /*No empty descriptor available*/
            /*Maximum number of files are opened*/
            dbg(DBG_PRINT,"(GRADING2C) Maximum number of files are open\n");
            return -EMFILE;
        }
        file_t *f = fget(-1);

        if(f==NULL){
            /*The kalloc operation will return NULL*/
            dbg(DBG_PRINT,"(GRADING2C) Out of memory\n");
            return -ENOMEM;
        }
        
        int perm = oflags&3;
        int extra = oflags&2044;
        int final_mode=0;
        int seek;
        /*perm can be
         O_RDONLY        0
         O_WRONLY        1
         O_RDWR          2

         O_CREAT         0x100   256 Create file if non-existent. 
         O_TRUNC         0x200   512 Truncate to zero length. 
         O_APPEND        0x400   1024 Append to file.

         FMODE_READ    1
         FMODE_WRITE   2
         FMODE_APPEND  4

         2^11-1
        */
        
        if(perm == (O_WRONLY | O_RDWR)){
            /*Error case for flags*/
            fput(f);
            return -EINVAL; 
        }

        if(perm == 0){
            final_mode = FMODE_READ;
            seek = 0;
        }
        else if(perm == 1){
            final_mode = FMODE_WRITE;
            seek = 0;
        }
        else if(perm == 2 || perm == 3){
            final_mode = FMODE_READ | FMODE_WRITE; 
            seek = 0;
        }

        if((extra&256) != 0){
           /*O_CREAT*/  
           oflags = O_CREAT;
        }

        if((extra&512) != 0 ){
          /*0_TRUNC*/
        }
        
	if((extra&1024) != 0){
            /*O_APPEND*/
           /*Take default seek*/ 
           final_mode=final_mode|FMODE_APPEND;
        }

        /*Call open_namev to get vnode of the file*/
        /*Result vnode come here*/
        vnode_t *res_vnode;
        /*parent vnode*/
        vnode_t *base;

        int status = open_namev(filename, oflags, &res_vnode, NULL);

        if(status == -ENOENT && oflags != O_CREAT){
            curproc->p_files[fd]=NULL;
            fput(f);
            return -ENOENT;
        }
        else if (status < 0){
            curproc->p_files[fd]=NULL;
            fput(f);
            return status;
        }
	
	 /*If ISDIR and oflag permissions are */
         if(_S_TYPE(res_vnode->vn_mode)==S_IFDIR && 
              ( perm != O_RDONLY) ){
           curproc->p_files[fd]=NULL;
           fput(f);
           return -EISDIR; 
        }
        if((extra&1024) != 0 && perm!=2){
            /*O_APPEND*/
           /*Take default seek*/ 
           dbg(DBG_PRINT,"(GRADING2C) Case where O_APPEND (or) RDWR \n");
           seek = res_vnode->vn_len;
        }

        /*Set file descriptor of current process*/
        /*fd of the current process will not be null anymore*/

        /*Fill in the file_t*/
        f->f_pos = seek;
        f->f_refcount = res_vnode->vn_refcount;
        f->f_vnode = res_vnode;
        /*Set the mode of the file*/
        f->f_mode = final_mode;
	 curproc->p_files[fd]=f;
        /*Return new fd*/
        return fd;
}
