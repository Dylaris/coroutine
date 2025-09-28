#define COROUTINE_IMPLEMENTATION
#include "coroutine.h"

static void counter(void *arg)
{
    size_t n = (size_t)arg;
    for (size_t i = 0; i < n; i++) {
        printf("[%s] %zu\n", coroutine_get_name(coroutine_get_current()), i);
        coroutine_yield(NULL);
    }

    coroutine_finish(NULL);
}

int main(void)
{
    coroutine_group_init();

    Coroutine *c1 = coroutine_create("c1", counter);
    Coroutine *c2 = coroutine_create("c2", counter);

    printf("------ counter ------\n");
    coroutine_resume(c1, (void*)10);
    coroutine_resume(c2, (void*)5);
    while (coroutine_get_status(c1) != COROUTINE_DEAD ||
           coroutine_get_status(c2) != COROUTINE_DEAD) {
        coroutine_resume(c1, NULL);
        coroutine_resume(c2, NULL);
    }
    printf("alive: %zu\n", coroutine_get_alive_count());
    printf("------ counter ------\n");

    coroutine_group_fini();
    return 0;
}
