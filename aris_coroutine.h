/*
coroutine.h - v0.01 - Dylaris 2025
===================================================

BRIEF:
  Coroutine implementation for C language.

NOTICE:
  This implementation uses GNU embedded assembly
  and is not compatible with C++.

USAGE:
  In exactly one source file, define the implementation macro
  before including this header:
  ```
    #define ARIS_COROUTINE_IMPLEMENTATION
    #include "aris_coroutine.h"
  ```
  In other files, just include the header without the macro.

LICENSE:
  See the end of this file for further details.
*/

#ifndef ARIS_COROUTINE_H
#define ARIS_COROUTINE_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

typedef enum aris_coroutine_state {
    ARIS_STATE_READY,
    ARIS_STATE_RUNNING,
    ARIS_STATE_SUSPEND,
    ARIS_STATE_DEAD
} aris_coroutine_state;

typedef struct aris_coroutine {
    void *regs[14];
    size_t stack_size;
    void *stack_base;
    aris_coroutine_state state;
    const char *name;
} aris_coroutine;

typedef struct aris_coroutine_group {
    aris_coroutine *coroutines; /* coroutine array */
    aris_coroutine **resumers;  /* stack to record the path of resume coroutine */
    aris_coroutine *current;    /* current running coroutine */
} aris_coroutine_group;

void aris_coroutine_group_init(aris_coroutine_group *group);
void aris_coroutine_group_free(aris_coroutine_group *group);
aris_coroutine *aris_coroutine_create(aris_coroutine_group *group,
                      const char *name, void (*task)(void *), void *arg);
bool aris_coroutine_resume(aris_coroutine_group *group, aris_coroutine *next);
bool aris_coroutine_loop_resume(aris_coroutine_group *group);
bool aris_coroutine_yield(aris_coroutine_group *group);
bool aris_coroutine_finish(aris_coroutine_group *group);
size_t aris_coroutine_collect(aris_coroutine_group *group);
size_t aris_coroutine_alive(aris_coroutine_group *group);
const char *aris_coroutine_name(aris_coroutine_group *group);

#endif /* ARIS_COROUTINE_H */

#ifdef ARIS_COROUTINE_IMPLEMENTATION

/*
64bit (context.regs[14])
low
     | regs[0]:  r15 |
     | regs[1]:  r14 |
     | regs[2]:  r13 |
     | regs[3]:  r12 |
     | regs[4]:  r9  |
     | regs[5]:  r8  |
     | regs[6]:  rbp |
     | regs[7]:  rdi |
     | regs[8]:  rsi |
     | regs[9]:  ret |   // return address
     | regs[10]: rdx |
     | regs[11]: rcx |
     | regs[12]: rbx |
     | regs[13]: rsp |
high
*/

typedef enum aris_register {
    CTX_R15, CTX_R14, CTX_R13, CTX_R12, CTX_R9,  CTX_R8,  CTX_RBP,
    CTX_RDI, CTX_RSI, CTX_RET, CTX_RDX, CTX_RCX, CTX_RBX, CTX_RSP
} aris_register;

#define ARIS_COROUTINE_STACK_SIZE (16*1024)
#define ARIS_PROTECT_REGION_SIZE  64

typedef struct aris__vector_header {
    size_t size;
    size_t capacity;
} aris__vector_header ;

#define aris__vec_header(vec) \
    ((aris__vector_header*)((char*)(vec) - sizeof(aris__vector_header)))
#define aris__vec_size(vec) ((vec) ? aris__vec_header(vec)->size : 0)
#define aris__vec_capacity(vec)  ((vec) ? aris__vec_header(vec)->capacity : 0)
#define aris__vec_push(vec, item)                                              \
    do {                                                                       \
        if (aris__vec_size(vec) + 1 > aris__vec_capacity(vec)) {               \
            size_t new_capacity, alloc_size;                                   \
            aris__vector_header *new_header;                                   \
                                                                               \
            new_capacity = aris__vec_capacity(vec) == 0                        \
                           ? 16 : 2 * aris__vec_capacity(vec);                 \
            alloc_size = sizeof(aris__vector_header) +                         \
                         new_capacity*sizeof(*(vec));                          \
                                                                               \
            if (vec) {                                                         \
                new_header = realloc(aris__vec_header(vec), alloc_size);       \
            } else {                                                           \
                new_header = malloc(alloc_size);                               \
                new_header->size = 0;                                          \
            }                                                                  \
            new_header->capacity = new_capacity;                               \
                                                                               \
            (vec) = (void*)((char*)new_header + sizeof(aris__vector_header));  \
        }                                                                      \
                                                                               \
        (vec)[aris__vec_header(vec)->size++] = (item);                         \
    } while (0)
#define aris__vec_pop(vec) ((vec)[--aris__vec_header(vec)->size])
#define aris__vec_free(vec)                   \
    do {                                      \
        if (vec) free(aris__vec_header(vec)); \
        (vec) = NULL;                         \
    } while (0)
#define aris__vec_reset(vec) ((vec) ? aris__vec_header(vec)->size = 0 : 0)

static void __attribute__((naked)) aris__coroutine_switch(
    aris_coroutine *cur, aris_coroutine *next)
{
    (void)cur;
    (void)next;

    __asm__ __volatile__(
        /* store current coroutine's context */
        "leaq (%rsp), %rax\n\t"
        "movq %rax, 104(%rdi)\n\t"
        "movq %rbx, 96(%rdi)\n\t"
        "movq %rcx, 88(%rdi)\n\t"
        "movq %rdx, 80(%rdi)\n\t"
        "movq 0(%rax), %rax\n\t"
        "movq %rax, 72(%rdi)\n\t"
        "movq %rsi, 64(%rdi)\n\t"
        "movq %rdi, 56(%rdi)\n\t"
        "movq %rbp, 48(%rdi)\n\t"
        "movq %r8, 40(%rdi)\n\t"
        "movq %r9, 32(%rdi)\n\t"
        "movq %r12, 24(%rdi)\n\t"
        "movq %r13, 16(%rdi)\n\t"
        "movq %r14, 8(%rdi)\n\t"
        "movq %r15, (%rdi)\n\t"
        "xorq %rax, %rax\n\t"

        /* restore next coroutine's context */
        "movq 48(%rsi), %rbp\n\t"
        "movq 104(%rsi), %rsp\n\t"
        "movq (%rsi), %r15\n\t"
        "movq 8(%rsi), %r14\n\t"
        "movq 16(%rsi), %r13\n\t"
        "movq 24(%rsi), %r12\n\t"
        "movq 32(%rsi), %r9\n\t"
        "movq 40(%rsi), %r8\n\t"
        "movq 56(%rsi), %rdi\n\t"
        "movq 80(%rsi), %rdx\n\t"
        "movq 88(%rsi), %rcx\n\t"
        "movq 96(%rsi), %rbx\n\t"
        "leaq 8(%rsp), %rsp\n\t"
        "pushq 72(%rsi)\n\t"
        "movq 64(%rsi), %rsi\n\t"
        "ret\n\t"
    );
}

void aris_coroutine_group_init(aris_coroutine_group *group)
{
    aris_coroutine main_coroutine;

    memset(&main_coroutine, 0, sizeof(aris_coroutine));
    main_coroutine.state = ARIS_STATE_RUNNING;

    group->coroutines = NULL;
    group->resumers = NULL;
    aris__vec_push(group->coroutines, main_coroutine);
    group->current = &group->coroutines[0];
}

void aris_coroutine_group_free(aris_coroutine_group *group)
{
    for (size_t i = 1; i < aris__vec_size(group->coroutines); i++) {
        aris_coroutine *coroutine = &group->coroutines[i];
        if (coroutine->stack_base) free(coroutine->stack_base);
    }

    aris__vec_free(group->coroutines);
    aris__vec_free(group->resumers);
    group->current = NULL;
}

aris_coroutine *aris_coroutine_create(aris_coroutine_group *group,
                      const char *name, void (*task)(void *), void *arg)
{
    aris_coroutine coroutine;

    memset(&coroutine, 0, sizeof(aris_coroutine));
    coroutine.stack_size = ARIS_COROUTINE_STACK_SIZE;
    coroutine.stack_base = malloc(ARIS_COROUTINE_STACK_SIZE);
    if (!coroutine.stack_base) {
        perror("malloc");
        return NULL;
    }

    coroutine.regs[CTX_RET] = (void*)task;
    coroutine.regs[CTX_RSP] = (char*)coroutine.stack_base +
                              coroutine.stack_size -
                              ARIS_PROTECT_REGION_SIZE;
                              /* reserve some bytes for protection */
    coroutine.regs[CTX_RDI] = arg;
    coroutine.state = ARIS_STATE_READY;
    coroutine.name = name;

    aris__vec_push(group->coroutines, coroutine);
    return &group->coroutines[aris__vec_size(group->coroutines) - 1];
}

bool aris_coroutine_resume(aris_coroutine_group *group, aris_coroutine *next)
{
    if (!next && next->state != ARIS_STATE_SUSPEND &&
        next->state != ARIS_STATE_READY) {
        return false;
    }

    aris_coroutine *current = group->current;
    group->current->state = ARIS_STATE_SUSPEND;
    group->current = next;
    group->current->state = ARIS_STATE_RUNNING;

    aris__vec_push(group->resumers, current);
    aris__coroutine_switch(current, next);

    return true;
}

bool aris_coroutine_loop_resume(aris_coroutine_group *group)
{
    /* main coroutine should be alive */
    size_t end_alive_count = 1;

    /* Do take the current coroutine into account in the loop.
       If it is not the main coroutine, add another 1. */
    if (group->current->stack_base) end_alive_count++;

    while (aris_coroutine_alive(group) > end_alive_count) {
        for (size_t i = 0; i < aris__vec_size(group->coroutines); i++) {
            aris_coroutine *coroutine = &group->coroutines[i];
            if (coroutine->state == ARIS_STATE_DEAD ||
                coroutine->stack_base == group->current->stack_base) continue;
            if (!aris_coroutine_resume(group, coroutine)) return false;
        }
    }

    return true;
}

bool aris_coroutine_yield(aris_coroutine_group *group)
{
    if (group->current->state != ARIS_STATE_RUNNING ||
        aris__vec_size(group->resumers) == 0) {
        return false;
    }

    aris_coroutine *current = group->current;
    aris_coroutine *next = aris__vec_pop(group->resumers);
    if (next->state != ARIS_STATE_SUSPEND) return false;
    group->current->state = ARIS_STATE_SUSPEND;
    group->current = next;
    group->current->state = ARIS_STATE_RUNNING;

    aris__coroutine_switch(current, next);

    return true;
}

bool aris_coroutine_finish(aris_coroutine_group *group)
{
    if (group->current->state != ARIS_STATE_RUNNING) return false;
    if (aris__vec_size(group->resumers) == 0) exit(100);

    aris_coroutine *current = group->current;
    aris_coroutine *next = aris__vec_pop(group->resumers);
    if (next->state != ARIS_STATE_SUSPEND) return false;
    group->current->state = ARIS_STATE_DEAD;
    group->current = next;
    group->current->state = ARIS_STATE_RUNNING;

    aris__coroutine_switch(current, next);

    return true;
}

size_t aris_coroutine_collect(aris_coroutine_group *group)
{
    size_t dead_count = 0;

    for (size_t i = 0; i < aris__vec_size(group->coroutines); i++) {
        aris_coroutine *coroutine = &group->coroutines[i];
        if (coroutine->state == ARIS_STATE_DEAD) {
            if (coroutine->stack_base) free(coroutine->stack_base);
            coroutine->stack_base = NULL; /* avoid double free in group free */
            dead_count++;
        }
    }

    return dead_count;
}

size_t aris_coroutine_alive(aris_coroutine_group *group)
{
    size_t alive_count = 0;

    for (size_t i = 0; i < aris__vec_size(group->coroutines); i++) {
        aris_coroutine *coroutine = &group->coroutines[i];
        if (coroutine->state != ARIS_STATE_DEAD) alive_count++;
    }

    return alive_count;
}

const char *aris_coroutine_name(aris_coroutine_group *group)
{
    return group->current->name ? group->current->name : "@null@";
}

#endif /* ARIS_COROUTINE_IMPLEMENTATION */

#ifdef ARIS_COROUTINE_STRIP_PREFIX

#define coroutine_group_init  aris_coroutine_group_init
#define coroutine_group_free  aris_coroutine_group_free
#define coroutine_create      aris_coroutine_create
#define coroutine_resume      aris_coroutine_resume
#define coroutine_loop_resume aris_coroutine_loop_resume
#define coroutine_yield       aris_coroutine_yield
#define coroutine_finish      aris_coroutine_finish
#define coroutine_collect     aris_coroutine_collect
#define coroutine_alive       aris_coroutine_alive
#define coroutine_name        aris_coroutine_name

#endif /* ARIS_COROUTINE_STRIP_PREFIX */

/*
------------------------------------------------------------------------------
This software is available under MIT license.
------------------------------------------------------------------------------
Copyright (c) 2025 Dylaris
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
