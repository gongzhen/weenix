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

/*kmutex_t lookup_mutex;*/

#define KMUTEX_STATIC_INITIALIZER(name) {{{&name.km_waitq.tq_list,\
    &name.km_waitq.tq_list}, 0}, NULL}

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
    if (dir->vn_ops->lookup == NULL){
        return -ENOTDIR;
    }

    KASSERT(name != NULL);

    int lookup_result = dir->vn_ops->lookup(dir, name, len, result);
    
    return lookup_result;
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
    if (*pathname == '\0'){
        return -EINVAL;
    }

    vnode_t *parent = NULL;
    vnode_t *curr;

    if (*pathname == '/'){
        curr = vfs_root_vn;

        while (*pathname == '/'){    
            pathname++;
        }

    } else if (base == NULL){
        curr = curproc->p_cwd;
    } else {
        curr = base;
    }

    vref(curr);

    if (*pathname == '\0'){
        *namelen = 1;
        *name = ".";
        *res_vnode = curr;
        return 0;
    }

    int dir_name_start = 0;
    int next_name = 0;
    int lookup_result = 1;
    int cur_name_len;
    int errcode = 0;

    while (lookup_result >= 0 && pathname[next_name] != '\0'){
        if (parent != NULL){
            vput(parent);
        }

        parent = curr;

        dir_name_start = next_name;

        /* first, find the end of the current dir name */
        while (pathname[next_name] != '/' && pathname[next_name] != '\0'){
            next_name++;
        }

        /* save the length of the current dir name in case we need it
         * outside the loop (if the current dir is actually the base) */
        cur_name_len = next_name - dir_name_start;

        if (next_name - dir_name_start > NAME_LEN){
            errcode = -ENAMETOOLONG;
            break;
        }

        /* then, look up the node */
        lookup_result = lookup(parent, (pathname + dir_name_start),
                next_name - dir_name_start, &curr);

        if (lookup_result == -ENOTDIR){
            errcode = -ENOTDIR;
            break;
        }

        /* we've reserved this as a special value, so let's make sure it never
         * is returned for real */
        KASSERT(!(lookup_result > 0));

        /* read away any trailing slashes */
        while (pathname[next_name] == '/'){
            next_name++;
        }
    }

    /* see if we exited in error -- these are only true if we've entered the loop */
    if (lookup_result < 0 && lookup_result != -ENOENT){
        dbg(DBG_VFS, "lookup failed with error code %d\n", lookup_result);
        vput(parent);

        return lookup_result;
    } else if (errcode != 0){
        dbg(DBG_VFS, "lookup failed with error code %d\n", errcode);
        vput(parent);

        return errcode;
    } else if (pathname[next_name] != '\0'){
        KASSERT(lookup_result == -ENOENT);
        dbg(DBG_VFS, "lookup failed with error code %d\n", -ENOENT);

        vput(parent);
        return -ENOENT;
    }
    
    if (lookup_result == 0){
        vput(curr);
    }

    *namelen = cur_name_len;
    *name = (pathname + dir_name_start);

    *res_vnode = parent;

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
    static kmutex_t lookup_mutex = KMUTEX_STATIC_INITIALIZER(lookup_mutex);

    size_t namelen;
    const char *name;

    vnode_t *dir;

    int namev_result = dir_namev(pathname, &namelen, &name, base, &dir);

    if (namev_result < 0){
        dbg(DBG_VFS, "couldn't find the file\n");
        return namev_result;
    }
  
    kmutex_lock(&lookup_mutex);
    int lookup_res = lookup(dir, name, namelen, res_vnode);

    int ret_val = lookup_res;

    if (lookup_res == -ENOENT){
       if (flag & O_CREAT){
           ret_val = dir->vn_ops->create(dir, name, namelen, res_vnode);
       } else {
           ret_val = -ENOENT;
       }
    } else if (lookup_res < 0){
        ret_val = lookup_res;
    } else if ((*res_vnode)->vn_ops->mkdir != NULL &&
            ((flag & O_WRONLY) || (flag & O_RDWR))){
        
        ret_val = -EISDIR;
        vput(*res_vnode);
        *res_vnode = NULL;
    }

    kmutex_unlock(&lookup_mutex);
    vput(dir);

    return ret_val;
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
