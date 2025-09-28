#define COROUTINE_IMPLEMENTATION
#include "coroutine.h"

static void sum(void *arg)
{
    size_t count = (size_t)arg;
    coroutine_yield(NULL);
    while (count--) {
        size_t n = (size_t)coroutine_get_resume_value(coroutine_get_current());
        size_t sum = 0;
        for (size_t i = 1; i <= n; i++) {
            sum += i;
        }
        coroutine_yield((void*)sum);
    }
    coroutine_finish(NULL);
}

int main(void)
{
    coroutine_group_init();

    Coroutine *c1 = coroutine_create("c1", sum);

    printf("------ sum ------\n");
    coroutine_resume(c1, (void*)3);

    size_t sum, n;

    n = 10;
    coroutine_resume(c1, (void*)n);
    sum = (size_t)coroutine_get_resume_value(coroutine_get_current());
    printf("sum of 1 to %zu: %zu\n", n, sum);

    n = 3;
    coroutine_resume(c1, (void*)n);
    sum = (size_t)coroutine_get_resume_value(coroutine_get_current());
    printf("sum of 1 to %zu: %zu\n", n, sum);

    n = 100;
    coroutine_resume(c1, (void*)n);
    sum = (size_t)coroutine_get_resume_value(coroutine_get_current());
    printf("sum of 1 to %zu: %zu\n", n, sum);

    n = 1000;
    coroutine_resume(c1, (void*)n);
    if (!coroutine_get_resume_value(coroutine_get_current())) {
        printf("over over over\n");
    }

    printf("alive: %zu\n", coroutine_get_alive_count());
    printf("------ sum ------\n");

    coroutine_group_fini();
    return 0;
}
