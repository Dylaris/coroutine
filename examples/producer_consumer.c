#define ARIS_COROUTINE_IMPLEMENTATION
#define ARIS_COROUTINE_STRIP_PREFIX
#include "aris_coroutine.h"

static void producer(void *arg)
{
    const char *fruits[] = {
        "apple", "banana", "orange", "grape"
    };

    for (size_t i = 0; i < sizeof(fruits)/sizeof(fruits[0]); i++) {
        printf("[%s] produce %s\n", coroutine_get_name(coroutine_get_current()), fruits[i]);
        coroutine_yield((void*)fruits[i]);
    }

    coroutine_finish(NULL);
}

static void consumer(void *arg)
{
    aris_coroutine *producer_coroutine = (aris_coroutine*)arg;

    while (1) {
        coroutine_resume(producer_coroutine, NULL);
        const char *value = (const char*)coroutine_get_yield_value(coroutine_get_current());
        if (!value) {
            printf("[consumer] no more products\n");
            break;
        } else {
            printf("[consumer] process %s\n", value);
        }
    }
}


int main(void)
{
    coroutine_group_init();

    aris_coroutine *producer_coroutine = coroutine_create("producer", producer);

    printf("------ produce/consume ------\n");
    consumer((void*)producer_coroutine);
    printf("alive: %zu\n", coroutine_get_alive_count());
    printf("------ produce/consume ------\n");

    coroutine_group_free();
    return 0;
}
