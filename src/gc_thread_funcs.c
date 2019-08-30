#include "gc_internal.h"
#include "gc.h"

#define debug(str) puts(str)

pthread_mutex_t mutex_gc_object = PTHREAD_MUTEX_INITIALIZER;
int is_gc_locked = 0;
extern pthread_spinlock_t spin_re_gc;

/*
    Due to this ogc API calls to another ogc API, we use an additional flag worked
    with spinlock to wait for the resource.
*/
void* gc_alloc_worker(void* arg)
{
    uintptr_t alloc_addr;
    size_t size = *((size_t*) arg);

    if (!(alloc_addr = (uintptr_t) malloc(size)))
        pthread_exit(NULL);
    gc_ptr_t p = (gc_ptr_t){
        .start = alloc_addr,
        .size = *((size_t*) arg),
        .marked = true,
    };
    
    pthread_mutex_lock(&mutex_gc_object);

    pthread_spin_lock(&spin_re_gc);

    is_gc_locked = 1;

    if (__gc_object.min > alloc_addr)
        __gc_object.min = alloc_addr;
    if (__gc_object.max < alloc_addr + size)
        __gc_object.max = alloc_addr + size;
    gc_list_add(&__gc_object.ptr_map[HASH(alloc_addr) % PTR_MAP_SIZE], p);
    __gc_object.ptr_num++;
    if (__gc_object.ptr_num >= __gc_object.limit)
        gc_run();
    else {
        is_gc_locked = 0;

        pthread_spin_unlock(&spin_re_gc);
    }

    pthread_mutex_unlock(&mutex_gc_object);

    pthread_exit((void *) alloc_addr);
}

void* gc_mfree_worker(void* arg)
{
    int rt;

    rt = pthread_spin_trylock(&spin_re_gc);

    if (is_gc_locked) {
        free((void *) ((gc_list_t *) arg)->data.start);
        __gc_object.ptr_num--;

        is_gc_locked = 0;
    }
    else {
        free((void *) ((gc_list_t *) arg)->data.start);
        __gc_object.ptr_num--;

        if (!rt)
            pthread_spin_unlock(&spin_re_gc);
    }
    
    pthread_exit(NULL);
}

/* Same mechanism, please refer to  `gc_run_worker` below */
void* gc_free_worker(void* arg)
{
    pthread_mutex_lock(&mutex_gc_object);

    pthread_spin_lock(&spin_re_gc);

    is_gc_locked = 1;

    gc_list_t *lst = __gc_object.ptr_map[HASH(arg) % PTR_MAP_SIZE];
    if (lst && gc_list_exist(lst, (uintptr_t) lst)) {
        gc_list_del(&lst, (uintptr_t) lst);
        gc_mfree(lst);
    }
    else {
        is_gc_locked = 0;

        pthread_spin_unlock(&spin_re_gc);
    }

    pthread_mutex_unlock(&mutex_gc_object);

    pthread_exit(NULL);
}

/*
    This worker may execute within two scenario:
    1. Work for `ogc` thread, which is main thread of `ogc`
    2. Work for thread of `ogc`, which is child thread of `ogc` thread
    Since we may encounter deadlock if we lock (mutex_gc_object) first
    and then lock again. At secondary lock, if it's lock from thread
    which is created by child thread of `ogc`, then deadlock is
    occured.
*/
void* gc_run_worker(void* arg)
{
    int rt;

    /*
        Lock it because we are going to read spin_re_gc, and check if we
        are owning the lock. If the lock isn't owned by us, then this
        worker is working for `ogc` instead of child thread of it.
    */
    rt = pthread_spin_trylock(&spin_re_gc);

    if (is_gc_locked) {
        gc_mark_stack();
        gc_sweep();
        is_gc_locked = 0;

        pthread_spin_unlock(&spin_re_gc);
    }
    else {
        pthread_mutex_lock(&mutex_gc_object);

        gc_mark_stack();
        gc_sweep();

        /*
            If we unintentionally aquired the lock, release it. If we
            release it regardless of whether the lock is belong to us
            , we may unlock someone's lock, which is a thief action
        */
        if (!rt)
            pthread_spin_unlock(&spin_re_gc);

        pthread_mutex_unlock(&mutex_gc_object);
    }

    pthread_exit(NULL);
}

void* gc_destroy_worker(void* arg)
{
    pthread_mutex_lock(&mutex_gc_object);

    __gc_object.ref_count--;
    if (__gc_object.ref_count <= 0) {
        __gc_object.ref_count = 0;
        for (int i = -1; ++i < PTR_MAP_SIZE;) {
            gc_list_t *m = __gc_object.ptr_map[i];
            while (m) {
                gc_list_t *tmp = m;
                free((void *) (m->data.start));
                m = m->next;
                free(tmp);
            }
            __gc_object.ptr_map[i] = 0;
        }
    }

    pthread_mutex_unlock(&mutex_gc_object);

    pthread_exit(NULL);
}
