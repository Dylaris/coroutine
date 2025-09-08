#define ARIS_COROUTINE_IMPLEMENTATION
#define ARIS_COROUTINE_STRIP_PREFIX
#include "aris_coroutine.h"

static aris_coroutine_group group;

static void counter(void *arg)
{
    size_t n = (size_t)arg;
    for (size_t i = 0; i < n; i++) {
        printf("[%s] %zu\n", coroutine_name(&group), i);
        coroutine_yield(&group);
    }

    coroutine_finish(&group);
}

int main(void)
{
    coroutine_group_init(&group);

    aris_coroutine *c1 = coroutine_create(&group, "c1", counter, (void*)10);
    aris_coroutine *c2 = coroutine_create(&group, "c2", counter, (void*)5);
    aris_coroutine *c3 = coroutine_create(&group, "c3", counter, (void*)6);
    aris_coroutine *c4 = coroutine_create(&group, "c4", counter, (void*)2);

    coroutine_resume(&group, c1);
    coroutine_resume(&group, c4);

    coroutine_loop_resume(&group);

    printf("alive: %zu\n", coroutine_alive(&group));

    coroutine_group_free(&group);
    return 0;
}

