#include "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int 
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        /*NOT_YET_IMPLEMENTED("VFS: lookup");*/
	dbg(DBG_PRINT,"(GRADING2C) looking up\n");
	
        KASSERT(NULL != dir);
        dbg(DBG_PRINT,"(GRADING2A 2.a)  Directory is not null\n");
        KASSERT(NULL != name);
        dbg(DBG_PRINT,"(GRADING2A 2.a)  name is not null\n");
        KASSERT(NULL != result);
        dbg(DBG_PRINT,"(GRADING2A 2.a)  result pointer is not null\n");

        /*Check input*/
        if(len==0){
        *result=vget(dir->vn_fs,dir->vn_vno);
        return 0;
        }
        
        if(len > NAME_LEN){
            return -ENAMETOOLONG;
        }

        if(dir->vn_ops->lookup == NULL)
        {
            /* The file system has no lookup implementation defined*/
            dbg(DBG_PRINT,"(GRADING2C) Not a directory\n");
            return -ENOTDIR;
        }
        /*returns with the vnode refcount on *result incremented*/
        
        int status = dir->vn_ops->lookup(dir, name, len, result);
        return status;
}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */

int 
dir_namev(const char *pathname, size_t *namelen, const char **name, 
          vnode_t *base,vnode_t **res_vnode)
{
        /*NOT_YET_IMPLEMENTED("VFS: dir_namev");*/
	KASSERT(NULL != pathname);
	dbg(DBG_PRINT,"(GRADING2A 2.b)  pathname is not null\n");
        KASSERT(NULL != namelen);
        dbg(DBG_PRINT,"(GRADING2A 2.b)  namelen is not null\n");
        KASSERT(NULL != name);
        dbg(DBG_PRINT,"(GRADING2A 2.b)  name is not null\n");
        KASSERT(NULL != res_vnode);
        dbg(DBG_PRINT,"(GRADING2A 2.b) res_vnode not null\n");

      int i=0,j=0,c=0;
      char new_path[5000];
      vnode_t *current_dir;
      int ret_val;

  if(pathname[0]=='/')
  {
     current_dir=vfs_root_vn;
  }
  else if(base==NULL)
  {
     dbg(DBG_PRINT,"(GRADING2C) base=NULL\n");
     current_dir=curproc->p_cwd;
  }
  else
   {
     dbg(DBG_PRINT,"(GRADING2C) CURRENT_DIRECTORY=base ");
     current_dir=base;
   }
  
  for(i=0;i<(int)strlen(pathname);i++){
    if(pathname[i]=='/'){
	if(i==0)		
	  continue;
	else {
	   new_path[c]='\0';
	   ret_val=lookup(current_dir, new_path, c, &current_dir);
	   c=0;
	   if(ret_val < 0)
		return ret_val;
	   vput(current_dir);
	   if((i+1) == (int)strlen(pathname))
		break;
	}
     }
     else{
	new_path[c]=pathname[i];
	c++;
     }
  }
  new_path[c]='\0';
  *name=new_path;
  *namelen=c;
  *res_vnode = vget(current_dir->vn_fs, current_dir->vn_vno);

  KASSERT(NULL != *res_vnode);
  dbg(DBG_PRINT,"(GRADING2A 2.b)  res_vnode pointer is not null\n");
  return 0;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
        /* NOT_YET_IMPLEMENTED("VFS: open_namev"); */
        size_t namelen;
        const char *name;
        /* Status will return if the file is already created or not */
        int status = dir_namev(pathname, &namelen, &name, base, res_vnode);
        if(status < 0){
            return status;
        }
        status = lookup(*res_vnode, name, namelen, res_vnode);
        if(status == -ENOENT){
        dbg(DBG_PRINT,"(GRADING2C) status=ENOENT\n");
            if(flag==O_CREAT){
                KASSERT(NULL != (*res_vnode)->vn_ops->create);
                dbg(DBG_PRINT,"(GRADING2A 2.c) The callee of create has implementation\n");
                /*If the file do not exist then create it */
                /*Create vnode from vnode_ops function*/
                /*Create function return status of the operation*/
                status = (*res_vnode)->vn_ops->create(*res_vnode, name, namelen, res_vnode); 
            }
        }
        return status;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
