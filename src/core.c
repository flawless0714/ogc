/*
    NOTE:
    1. Thread pool is a better impl if the calling of API of `ogc` is intensive, since
         it saved the overhead of thread creation.

    TODO:
    -1. Seperate program (`dummy` and `string-separate`) and ogc into different thread,
        or we are losing the meaning of threading.
    1. Currently, only `dummy` can pass the verification since `string-separate` is
        still using local variable (belongs to thread, which of memory address we
        can't use) as argument of calling of `gc_mark_stack`. How I made `dummy`
        runable is through using global dummy data instead of local variable of
        thread.
*/

#include "gc_internal.h"
#include "gc.h"


/*
    When we call API of `ogc` within API of ogc, it may do mutex lock twice, which
    cause deadlock due to the lock is aquired by first mutex lock within same
    thread.
*/
pthread_spinlock_t spin_re_gc;

gc_t __gc_object = (gc_t){.ref_count = 0};

void gc_init(void *ptr, size_t limit)
{
    if (__gc_object.ref_count) {
        __gc_object.ref_count++;
        return;
    }
    __gc_object = (gc_t){.stack_start = ptr,
                         .ptr_map = {NULL},
                         .ptr_num = 0,
                         .ref_count = 1,
                         .limit = limit,
                         .min = UINTPTR_MAX,
                         .max = 0,
                         .globals = NULL};

    pthread_spin_init(&spin_re_gc, PTHREAD_PROCESS_PRIVATE);
}

static inline void swap_ptr(uint8_t **a, uint8_t **b)
{
    uint8_t *tmp = *a;
    *a = *b;
    *b = tmp;
}

void gc_mark(uint8_t *start, uint8_t *end)
{
    if (start > end)
        swap_ptr(&start, &end);
    while (start < end) {
        gc_list_t *idx = gc_ptr_index((uintptr_t)(*((void **) start)));
        if (idx && idx->data.marked != true) {
            idx->data.marked = true;
            gc_mark((uint8_t *) (idx->data.start),
                    (uint8_t *) (idx->data.start + idx->data.size));
        }
        start++;
    }
}

void gc_sweep(void)
{
    for (int i = 0; ++i < PTR_MAP_SIZE;) {
        gc_list_t *e = __gc_object.ptr_map[i];
        int k = 0;
        while (e) {
            if (!e->data.marked) {
                gc_mfree(e);
                e = e->next;
                gc_list_del(&__gc_object.ptr_map[i], k);
            } else {
                e->data.marked = false;
                e = e->next;
            }
            k++;
        }
    }
}

void gc_run(void)
{
   pthread_t tid;

   pthread_create(&tid, NULL, gc_run_worker, NULL);
   pthread_join(tid, NULL);
}

void gc_destroy(void)
{
    pthread_t tid;

    pthread_create(&tid, NULL, gc_destroy_worker, NULL);
    pthread_join(tid, NULL);
}
