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

	KASSERT(NULL != dir);
        dbg(DBG_PRINT, "GRADING2 A.2.a #PASSED : dir is not null\n");
        KASSERT(NULL != name);
        dbg(DBG_PRINT, "GRADING2 A.2.a #PASSED : name is not null\n");
        KASSERT(NULL != result);        
        dbg(DBG_PRINT, "GRADING2 A.2.a #PASSED : result is not null\n");
        /*TODO: handle . and .. special cases*/

	if(!S_ISDIR(dir->vn_mode)){
		return -ENOTDIR;
	}
	if(dir->vn_ops->lookup==NULL){
		return -ENOTDIR;
	}
	if(len > STR_MAX){
		return -ENAMETOOLONG;
	}
	/*else{
		int k=strcmp(name,".");
		if(len==0 || k==0){
			vref(dir);
			*result=dir;
			return 0;
		}
	}*/
	int result_dir=dir->vn_ops->lookup(dir,name,len,result);
	if(result_dir <0){
        	return result_dir;
	}else{
		return 0;
	}
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
          vnode_t *base, vnode_t **res_vnode)
{
        /*NOT_YET_IMPLEMENTED("VFS: dir_namev");*/

        KASSERT(NULL != pathname);
        dbg(DBG_PRINT, "GRADING2 A.2.b #PASSED : pathname is not null\n");
        KASSERT(NULL != namelen);
        dbg(DBG_PRINT, "GRADING2 A.2.b #PASSED : namelen is not null\n");
        KASSERT(NULL != name);
        dbg(DBG_PRINT, "GRADING2 A.2.b #PASSED : name is not null\n");
        KASSERT(NULL != res_vnode);
        dbg(DBG_PRINT, "GRADING2 A.2.b #PASSED : res_vnode is not null\n");

        if(pathname[0]=='\0'){
                return -EINVAL;
        }
        if(strlen(pathname) > MAXPATHLEN){
                return -ENAMETOOLONG;
        }
        vnode_t *cur_dir;
	char *n = (char *)pathname;
        
        if(base==NULL){
                cur_dir=curproc->p_cwd;
                vref(cur_dir);
        }else{
                cur_dir=base;
                vref(cur_dir);
        }
        if(pathname[0]=='/'){
                vput(cur_dir);
                cur_dir=vfs_root_vn;
		if(strlen(pathname) == 1){
			vref(cur_dir);
			*namelen = 1;
			*name = ".";
			*res_vnode = cur_dir;
			return 0;
		}
                vref(cur_dir);
                pathname++;   
		n++;     
        }
        char *temppathname=(char *)pathname;
        char *slash_ptr=(char *)pathname;
        int pathlength=strlen(pathname);
        char *pathend=(char *)pathname+pathlength;
	while(n[pathlength-1] == '/'){
		n[pathlength-1] = '\0';
		pathlength--;
		pathend = n + pathlength;
		temppathname = n;
	}
       	int slash_count = 0;
 
        slash_ptr=strchr(temppathname,'/'); /*TODO this is increasing the count for vfs_root as well*/
	if(slash_ptr != NULL && slash_ptr != pathend){
		while(*(slash_ptr+1) =='/'){
			slash_count++;
			slash_ptr++;
			if(slash_ptr==pathend)
				break;
		}
	}
        while(slash_ptr != pathend){
                /*check whethre it is directory or not */
                if(!S_ISDIR(cur_dir->vn_mode)){
                        vput(cur_dir);
                        return -ENOTDIR;
                }
                if(slash_ptr==NULL){
                        break;
                }else{
                        int ispathexist = lookup(cur_dir,temppathname,slash_ptr-temppathname-slash_count,res_vnode);
                        if(ispathexist >= 0){
                                vput(cur_dir);
                                cur_dir=*res_vnode;
				if(slash_ptr == pathend){
					break;
				}
                                slash_ptr++;
                                temppathname=slash_ptr;
                        }else{
                                vput(cur_dir);
                                return ispathexist;
                        }
                }
		slash_count = 0;
                slash_ptr=strchr(temppathname,'/'); /*TODO this is increasing the count for vfs_root as well*/
		if(slash_ptr != NULL && slash_ptr != pathend){
			while(*(slash_ptr+1) =='/'){
				slash_ptr++;
				slash_count++;
				if(slash_ptr==pathend)
					break;
			}
		}
        }
        *namelen = pathend-temppathname;
        *name=temppathname;	
	KASSERT(NULL != cur_dir );
	dbg(DBG_PRINT, "GRADING 2 A.2.b \n");
        *res_vnode=cur_dir;
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
        /*NOT_YET_IMPLEMENTED("VFS: open_namev");*/
	const char *retname=NULL;
	size_t length;
	vnode_t *temp_res_vnode;
	int result_dir=dir_namev(pathname,&length,&retname,base,&temp_res_vnode);
	if(result_dir<0){
		return result_dir;
	}
	if(!S_ISDIR(temp_res_vnode->vn_mode)){
		vput(temp_res_vnode);
		return -ENOTDIR;
	}
	int nodelookup=lookup(temp_res_vnode,retname,length,res_vnode);
	if(nodelookup == -ENOENT && (flag &  O_CREAT) != NULL){
		int returnvalue=temp_res_vnode->vn_ops->create(temp_res_vnode,retname,length,res_vnode);
		KASSERT(NULL !=  temp_res_vnode->vn_ops->create);
        	dbg(DBG_PRINT, "GRADING2 A.2.c #PASSED : vn_ops->create is not null\n");
		vput(temp_res_vnode);
		return returnvalue;
	}
	vput(temp_res_vnode);
	return nodelookup;
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
