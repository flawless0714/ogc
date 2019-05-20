#include <assert.h>
#include "gc.h"
#include "../src/gc_internal.h"


uint8_t dummy_data2[50];

void sample(void **h)
{
    void **ptr = gc_alloc(10);
    *ptr = gc_alloc(11);
    *ptr += 3;
    *h = *ptr;
}

int main(int argc, char *argv[])
{
    gc_init(&dummy_data2, 1);

    void *h = 0;
    sample(&h);
    assert(h);
    gc_dump_internals();

    h = gc_alloc(1);
    assert(h);
    gc_dump_internals();

    gc_destroy();
    return 0;
}
