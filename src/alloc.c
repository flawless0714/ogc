#include "gc_internal.h"
#include "gc.h"

void *gc_alloc(size_t size)
{
    uintptr_t ptr;
    pthread_t tid;

    /*
        I don't think this is a good practice, because the calling of
        funcs like this cause a little blocking (it's worst when
        multiple calling of this func since mutex is blocking us).
        The only pros I know is it improves the concurrency of
        programs executing with ogc.
    */
    pthread_create(&tid, NULL,  gc_alloc_worker, &size);
    pthread_join(tid, (void *) &ptr);

    return (void *) ptr;
}


/* 
    Currently not figured how to make this func running under threading
    since `gc_mfree` it called also being called by other `ogc` API, if
    I done threading for these two funcs, deadlock may occur. So the
    solution so far I thought is if `gc_free` should be threading, we
    need to change infrastructure of `ogc`. Honestly, I dont have
    scheme so far for doing so.
*/
void gc_free(void *ptr)
{
    pthread_t tid;
    
    pthread_create(&tid, NULL, gc_free_worker, ptr);
    pthread_join(tid, NULL);
}
