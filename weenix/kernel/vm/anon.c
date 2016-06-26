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

int anon_count = 0; /* for debugging/verification purposes */

static slab_allocator_t *anon_allocator;

static void anon_ref(mmobj_t *o);
static void anon_put(mmobj_t *o);
static int  anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  anon_fillpage(mmobj_t *o, pframe_t *pf);
static int  anon_dirtypage(mmobj_t *o, pframe_t *pf);
static int  anon_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t anon_mmobj_ops = {
        .ref = anon_ref,
        .put = anon_put,
        .lookuppage = anon_lookuppage,
        .fillpage  = anon_fillpage,
        .dirtypage = anon_dirtypage,
        .cleanpage = anon_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * anonymous page sub system. Currently it only initializes the
 * anon_allocator object.
 */
void
anon_init()
{
	anon_allocator = slab_allocator_create("anonobject",sizeof(mmobj_t));
        KASSERT(anon_allocator);
	dbg(DBG_PRINT, "GRADING3A 4.a\n");
        /*NOT_YET_IMPLEMENTED("VM: anon_init");*/
}

/*
 * You'll want to use the anon_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
anon_create()
{
       /* NOT_YET_IMPLEMENTED("VM: anon_create");*/
	mmobj_t *p_anonobj = slab_obj_alloc(anon_allocator);
	if(p_anonobj!=NULL){
		mmobj_init(p_anonobj,&anon_mmobj_ops);
		p_anonobj->mmo_refcount++;/*TODO? Is this the way to make sure the reference count?*/
		
		return p_anonobj;
	}
        return NULL;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
anon_ref(mmobj_t *o)
{
       /* NOT_YET_IMPLEMENTED("VM: anon_ref");*/
	KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));	
	dbg(DBG_PRINT, "GRADING3A 4.b\n");
	o->mmo_refcount++;
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is an anonymous object, it will
 * never be used again. You should unpin and uncache all of the
 * object's pages and then free the object itself.
 */
static void
anon_put(mmobj_t *o)
{
        /*NOT_YET_IMPLEMENTED("VM: anon_put");*/
	KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops)); 
	dbg(DBG_PRINT, "GRADING 3A 4.c\n");
	if(o->mmo_refcount-o->mmo_nrespages==1){
		if(!list_empty(&(o->mmo_respages))){
			pframe_t *pageframe;
			list_iterate_begin(&(o->mmo_respages),pageframe,pframe_t,pf_olink){
				while(pframe_is_pinned(pageframe))
					pframe_unpin(pageframe);
				if(pframe_is_busy(pageframe))
					sched_sleep_on(&pageframe->pf_waitq);
				if(pframe_is_dirty(pageframe))
					pframe_clean(pageframe);
					/*pframe_free(pageframe);*/
			}list_iterate_end();
		}
	}
	o->mmo_refcount--;
	if(o->mmo_refcount==0 && o->mmo_nrespages==0){
		slab_obj_free(anon_allocator,o);
	}
}

/* Get the corresponding page from the mmobj. No special handling is
 * required. */
static int
anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{

       /* NOT_YET_IMPLEMENTED("VM: anon_lookuppage");*/
	pframe_t *pageframe=pframe_get_resident(o,pagenum);
	if(pageframe!=NULL){
		while(!pframe_is_busy(pageframe)){
			*pf=pageframe;
			break;
		}
	}
        return 0;
}

/* The following three functions should not be difficult. */
static int
anon_fillpage(mmobj_t *o, pframe_t *pf)
{
        /*NOT_YET_IMPLEMENTED("VM: anon_fillpage");*/
	KASSERT(pframe_is_busy(pf));
	dbg(DBG_PRINT, "GRADING3A 4.d\n");
        KASSERT(!pframe_is_pinned(pf));
	dbg(DBG_PRINT, "GRADING3A 4.d\n");

	pframe_t *pageframe=pframe_get_resident(pf->pf_obj,pf->pf_pagenum);
	if(pageframe == NULL){
		return -EFAULT;
	}
	/*memcpy(pageframe->pf_addr,pf->pf_addr,PAGE_SIZE);*/
	memset(pf->pf_addr, 0, PAGE_SIZE);
	if(!pframe_is_pinned(pageframe))
		pframe_pin(pageframe);
        return 0;
}

static int
anon_dirtypage(mmobj_t *o, pframe_t *pf)
{
       /* NOT_YET_IMPLEMENTED("VM: anon_dirtypage");*/
	if(!(pframe_is_dirty(pf)))
		pframe_set_dirty(pf);
	return 0;
}

static int
anon_cleanpage(mmobj_t *o, pframe_t *pf)
{
        /*NOT_YET_IMPLEMENTED("VM: anon_cleanpage");*/
	pframe_t *pageframe=pframe_get_resident(pf->pf_obj,pf->pf_pagenum);
	if(pageframe == NULL){
		return -EFAULT;
	}
	memcpy(pageframe->pf_addr,pf->pf_addr,PAGE_SIZE);
        return 0;
}
