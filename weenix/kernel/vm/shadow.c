#include "globals.h"
#include "errno.h"

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/shadowd.h"

#define SHADOW_SINGLETON_THRESHOLD 5

int shadow_count = 0; /* for debugging/verification purposes */
#ifdef __SHADOWD__
/*
 * number of shadow objects with a single parent, that is another shadow
 * object in the shadow objects tree(singletons)
 */
static int shadow_singleton_count = 0;
#endif

static slab_allocator_t *shadow_allocator;

static void shadow_ref(mmobj_t *o);
static void shadow_put(mmobj_t *o);
static int  shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  shadow_fillpage(mmobj_t *o, pframe_t *pf);
static int  shadow_dirtypage(mmobj_t *o, pframe_t *pf);
static int  shadow_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t shadow_mmobj_ops = {
        .ref = shadow_ref,
        .put = shadow_put,
        .lookuppage = shadow_lookuppage,
        .fillpage  = shadow_fillpage,
        .dirtypage = shadow_dirtypage,
        .cleanpage = shadow_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * shadow page sub system. Currently it only initializes the
 * shadow_allocator object.
 */
void
shadow_init()
{
        /*NOT_YET_IMPLEMENTED("VM: shadow_init");*/
	shadow_allocator = slab_allocator_create("sahdowobject", sizeof(mmobj_t));
	KASSERT(shadow_allocator);
        dbg(DBG_PRINT, "GRADING3A 6.a\n");
}

/*
 * You'll want to use the shadow_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
shadow_create()
{
        /*NOT_YET_IMPLEMENTED("VM: shadow_create");*/
	mmobj_t *shadowobj = (mmobj_t*)slab_obj_alloc(shadow_allocator);
	if(shadowobj){
		mmobj_init(shadowobj,&shadow_mmobj_ops);
		shadowobj->mmo_refcount++;
		shadowobj->mmo_un.mmo_bottom_obj=NULL;
	}
        return shadowobj;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
shadow_ref(mmobj_t *o)
{
        /*NOT_YET_IMPLEMENTED("VM: shadow_ref");*/
	KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
	dbg(DBG_PRINT, "GRADING3A 6.b\n");
	o->mmo_refcount++;
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is a shadow object, it will never
 * be used again. You should unpin and uncache all of the object's
 * pages and then free the object itself.
 */
static void
shadow_put(mmobj_t *o)
{
        /*NOT_YET_IMPLEMENTED("VM: shadow_put");*/
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
        dbg(DBG_PRINT, "GRADING3A 6.c\n");
        o->mmo_refcount--;
        if(o->mmo_refcount == o->mmo_nrespages){
                if(!list_empty(&(o->mmo_respages))){
                        pframe_t *pageframe;
                        list_iterate_begin(&(o->mmo_respages),pageframe,pframe_t,pf_olink){
                                while(pframe_is_pinned(pageframe))
                                        pframe_unpin(pageframe);
                                if(pframe_is_busy(pageframe)){
					pframe_clear_busy(pageframe);
                                        sched_sleep_on(&pageframe->pf_waitq);
				}
                                if(pframe_is_dirty(pageframe))
                                        pframe_clean(pageframe);
                        }list_iterate_end();
                	slab_obj_free(shadow_allocator,o);
                }
        }
}

/* This function looks up the given page in this shadow object. The
 * forwrite argument is true if the page is being looked up for
 * writing, false if it is being looked up for reading. This function
 * must handle all do-not-copy-on-not-write magic (i.e. when forwrite
 * is false find the first shadow object in the chain which has the
 * given page resident). copy-on-write magic (necessary when forwrite
 * is true) is handled in shadow_fillpage, not here. */
static int
shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        /*NOT_YET_IMPLEMENTED("VM: shadow_lookuppage");*/

	mmobj_t *shadow_obj = o;	
	mmobj_t *bottom_obj = o->mmo_un.mmo_bottom_obj;
	if(!forwrite){
		while(shadow_obj != NULL){
			*pf = pframe_get_resident(shadow_obj, pagenum);	
			if(*pf != NULL){
				return 1;
			}
			shadow_obj = shadow_obj->mmo_shadowed;
		}
		/*	
		*pf = pframe_get_resident(bottom_obj, pagenum);
		if(*pf == NULL){
			return -1;
		}else{
			return 1;
		}	
		*/	
	}
		
	return -1;	
}

/* As per the specification in mmobj.h, fill the page frame starting
 * at address pf->pf_addr with the contents of the page identified by
 * pf->pf_obj and pf->pf_pagenum. This function handles all
 * copy-on-write magic (i.e. if there is a shadow object which has
 * data for the pf->pf_pagenum-th page then we should take that data,
 * if no such shadow object exists we need to follow the chain of
 * shadow objects all the way to the bottom object and take the data
 * for the pf->pf_pagenum-th page from the last object in the chain). */
static int
shadow_fillpage(mmobj_t *o, pframe_t *pf)
{
        /*NOT_YET_IMPLEMENTED("VM: shadow_fillpage");*/
	KASSERT(pframe_is_busy(pf));
	dbg(DBG_PRINT, "GRADING3A 6.d\n");
        KASSERT(!pframe_is_pinned(pf));
	dbg(DBG_PRINT, "GRADING3A 6.d\n");

	pframe_t *src=NULL;
	
	mmobj_t *shadow_obj = o->mmo_shadowed;
	mmobj_t *temp=NULL;
	mmobj_t *bottom_obj = o->mmo_un.mmo_bottom_obj;
		while(shadow_obj != NULL){
			src = pframe_get_resident(shadow_obj, pf->pf_pagenum);	
			if(src != NULL){
				break;
			}
			temp=shadow_obj;
			shadow_obj = shadow_obj->mmo_shadowed;
		}
		/*	
		*pf = pframe_get_resident(bottom_obj, pagenum);
		if(*pf == NULL){
			return -1;
		}else{
			return 1;
		}	
		*/	
	if(src==NULL){
		pframe_get(temp,pf->pf_pagenum,&src);
	}
	memcpy(pf->pf_addr, src->pf_addr, PAGE_SIZE);
	
	return 0;
}

/* These next two functions are not difficult. */

static int
shadow_dirtypage(mmobj_t *o, pframe_t *pf)
{
        /*NOT_YET_IMPLEMENTED("VM: shadow_dirtypage");*/
	if(!(pframe_is_dirty(pf))){
		pframe_set_dirty(pf);
		return 0;
	}else{
		return -1;
	}
}

static int
shadow_cleanpage(mmobj_t *o, pframe_t *pf)
{
        /*NOT_YET_IMPLEMENTED("VM: shadow_cleanpage");*/
	pframe_t *pageframe;
	shadow_lookuppage(o,pf->pf_pagenum,0,&pageframe);
	if(pageframe == NULL){
		return -1;
	}
	if(pframe_is_busy(pageframe)){
		pframe_clear_busy(pageframe);
		sched_broadcast_on(&(pageframe->pf_waitq));
	}
	while(pframe_is_pinned(pageframe)) /*XXX*/
		pframe_unpin(pageframe);
	memcpy(pageframe->pf_addr,pf->pf_addr,PAGE_SIZE);
	pframe_free(pf);
        return 0;
}
