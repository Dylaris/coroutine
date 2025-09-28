/*
coroutine.h - v0.02 - Dylaris 2025
===================================================

BRIEF:
  Lua-Style Coroutine implementation for C language.

NOTICE:
  This implementation uses GNU embedded assembly. Only
  support Linux because of ABI.
  And it is not compatible with C++ (no test).

USAGE:
  In exactly one source file, define the implementation macro
  before including this header:
  ```
    #define COROUTINE_IMPLEMENTATION
    #include "coroutine.h"
  ```
  In other files, just include the header without the macro.

HISTORY:
    v0.02 Remove struct 'coroutine_group' (coroutine groups can
          cause ambiguity in the attribution of the main coroutine).
          Support data exchange between resume() and yield().

LICENSE:
  See the end of this file for further details.
*/

#ifndef COROUTINE_H
#define COROUTINE_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

typedef enum Coroutine_Status {
    COROUTINE_READY,
    COROUTINE_RUNNING,
    COROUTINE_SUSPEND,
    COROUTINE_DEAD
} Coroutine_Status;

typedef struct Coroutine {
    void *regs[14];
    size_t stack_size;
    void *stack_base;
    Coroutine_Status status;
    const char *name;
    union {
        void *yield_value;  /* used to store the value passed by yield() */
        void *resume_value; /* used to store the value passed in by resume()
                               (as the return value of yield() or the parameter
                               value of startup function) */
    };
} Coroutine;

void coroutine_group_init(void);
void coroutine_group_fini(void);
Coroutine *coroutine_create(const char *name, void (*task)(void*));
bool coroutine_resume(Coroutine *next, void *resume_value);
bool coroutine_yield(void *yield_value);
bool coroutine_finish(void *return_value);
size_t coroutine_collect(void);
size_t coroutine_get_alive_count(void);
Coroutine_Status coroutine_get_status(const Coroutine *coroutine);
const char *coroutine_get_name(const Coroutine *coroutine);
void *coroutine_get_resume_value(const Coroutine *coroutine);
void *coroutine_get_yield_value(const Coroutine *coroutine);
const Coroutine *coroutine_get_current(void);

#endif /* COROUTINE_H */

#ifdef COROUTINE_IMPLEMENTATION

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

#define COROUTINE_STACK_SIZE (16*1024)
#define ARIS_PROTECT_REGION_SIZE  64

typedef struct aris_vec_tor_header {
    size_t size;
    size_t capacity;
} aris_vec_tor_header ;

#define aris_vec__header(vec) \
    ((aris_vec_tor_header*)((char*)(vec) - sizeof(aris_vec_tor_header)))
#define aris_vec__size(vec) ((vec) ? aris_vec__header(vec)->size : 0)
#define aris_vec__capacity(vec)  ((vec) ? aris_vec__header(vec)->capacity : 0)
#define aris_vec__push(vec, item)                                              \
    do {                                                                       \
        if (aris_vec__size(vec) + 1 > aris_vec__capacity(vec)) {               \
            size_t new_capacity, alloc_size;                                   \
            aris_vec_tor_header *new_header;                                   \
                                                                               \
            new_capacity = aris_vec__capacity(vec) == 0                        \
                           ? 16 : 2 * aris_vec__capacity(vec);                 \
            alloc_size = sizeof(aris_vec_tor_header) +                         \
                         new_capacity*sizeof(*(vec));                          \
                                                                               \
            if (vec) {                                                         \
                new_header = realloc(aris_vec__header(vec), alloc_size);       \
            } else {                                                           \
                new_header = malloc(alloc_size);                               \
                new_header->size = 0;                                          \
            }                                                                  \
            new_header->capacity = new_capacity;                               \
                                                                               \
            (vec) = (void*)((char*)new_header + sizeof(aris_vec_tor_header));  \
        }                                                                      \
                                                                               \
        (vec)[aris_vec__header(vec)->size++] = (item);                         \
    } while (0)
#define aris_vec__pop(vec) ((vec)[--aris_vec__header(vec)->size])
#define aris_vec__free(vec)                   \
    do {                                      \
        if (vec) free(aris_vec__header(vec)); \
        (vec) = NULL;                         \
    } while (0)
#define aris_vec__reset(vec) ((vec) ? aris_vec__header(vec)->size = 0 : 0)

static void __attribute__((naked)) coroutine__switch(
    const Coroutine *cur, const Coroutine *next)
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

static struct {
    Coroutine *coroutines; /* coroutine array */
    Coroutine **resumers;  /* stack to record the path of resume coroutine */
    Coroutine *current;    /* current running coroutine */
    size_t alive_count;
} group;

void coroutine_group_init(void)
{
    /* Return if it is not the first time */
    if (group.coroutines) return;

    Coroutine main_coroutine;

    memset(&main_coroutine, 0, sizeof(Coroutine));
    main_coroutine.status = COROUTINE_RUNNING;
    main_coroutine.name = "@main@";

    group.coroutines = NULL;
    group.resumers = NULL;
    aris_vec__push(group.coroutines, main_coroutine);
    group.current = &group.coroutines[0];
    group.alive_count = 1;
}

void coroutine_group_fini(void)
{
    /* Return if it is not main coroutine */
    if (group.current != &group.coroutines[0]) return;

    for (size_t i = 1; i < aris_vec__size(group.coroutines); i++) {
        Coroutine *coroutine = &group.coroutines[i];
        if (coroutine->stack_base) free(coroutine->stack_base);
    }

    aris_vec__free(group.coroutines);
    aris_vec__free(group.resumers);
}

Coroutine *coroutine_create(const char *name, void (*task)(void*))
{
    Coroutine coroutine;

    memset(&coroutine, 0, sizeof(Coroutine));
    coroutine.stack_size = COROUTINE_STACK_SIZE;
    coroutine.stack_base = malloc(COROUTINE_STACK_SIZE);
    if (!coroutine.stack_base) {
        perror("malloc");
        return NULL;
    }

    coroutine.regs[CTX_RET] = (void*)task;
    coroutine.regs[CTX_RSP] = (char*)coroutine.stack_base +
                              coroutine.stack_size -
                              ARIS_PROTECT_REGION_SIZE;
                              /* reserve some bytes for protection */
    coroutine.status = COROUTINE_READY;
    coroutine.name = name;
    coroutine.yield_value = NULL;
    coroutine.resume_value = NULL;

    aris_vec__push(group.coroutines, coroutine);
    group.alive_count++;

    return &group.coroutines[aris_vec__size(group.coroutines) - 1];
}

bool coroutine_resume(Coroutine *next, void *resume_value)
{
    if (!next || (next->status != COROUTINE_SUSPEND &&
        next->status != COROUTINE_READY)) {
        return false;
    }

    Coroutine *current = group.current;

    /* If it is just started, set resume_value to the parameter value of
       the startup function; otherwise, store it in the resume_value field. */
    if (next->status == COROUTINE_READY) {
        next->regs[CTX_RDI] = resume_value;
    } else {
        next->resume_value = resume_value;
    }

    group.current->status = COROUTINE_SUSPEND;
    group.current = next;
    group.current->status = COROUTINE_RUNNING;

    aris_vec__push(group.resumers, current);
    coroutine__switch(current, next);

    return true;
}

bool coroutine_yield(void *yield_value)
{
    if (group.current->status != COROUTINE_RUNNING ||
        aris_vec__size(group.resumers) == 0) {
        return false;
    }

    Coroutine *current = group.current;
    Coroutine *next = aris_vec__pop(group.resumers);
    if (next->status != COROUTINE_SUSPEND) return false;

    next->yield_value = yield_value;

    group.current->status = COROUTINE_SUSPEND;
    group.current = next;
    group.current->status = COROUTINE_RUNNING;

    coroutine__switch(current, next);

    return true;
}

bool coroutine_finish(void *return_value)
{
    if (group.current->status != COROUTINE_RUNNING) return false;
    if (aris_vec__size(group.resumers) == 0) exit(100);

    Coroutine *current = group.current;
    Coroutine *next = aris_vec__pop(group.resumers);
    if (next->status != COROUTINE_SUSPEND) return false;

    next->yield_value = return_value;

    group.current->status = COROUTINE_DEAD;
    group.current = next;
    group.current->status = COROUTINE_RUNNING;
    group.alive_count--;

    coroutine__switch(current, next);

    return true;
}

size_t coroutine_collect(void)
{
    size_t dead_count = 0;

    for (size_t i = 0; i < aris_vec__size(group.coroutines); i++) {
        Coroutine *coroutine = &group.coroutines[i];
        if (coroutine->status == COROUTINE_DEAD) {
            if (coroutine->stack_base) free(coroutine->stack_base);
            coroutine->stack_base = NULL; /* avoid double free in group free */
            dead_count++;
        }
    }

    return dead_count;
}

size_t coroutine_get_alive_count(void)
{
    return group.alive_count;
}

const Coroutine *coroutine_get_current(void)
{
    return group.current;
}

Coroutine_Status coroutine_get_status(const Coroutine *coroutine)
{
    return coroutine->status;
}

const char *coroutine_get_name(const Coroutine *coroutine)
{
    return coroutine->name ? coroutine->name : "@null@";
}

void *coroutine_get_resume_value(const Coroutine *coroutine)
{
    return coroutine->resume_value;
}

void *coroutine_get_yield_value(const Coroutine *coroutine)
{
    return coroutine->yield_value;
}

#undef aris_vec__header
#undef aris_vec__size
#undef aris_vec__capacity
#undef aris_vec__push
#undef aris_vec__pop
#undef aris_vec__free
#undef aris_vec__reset

#endif /* COROUTINE_IMPLEMENTATION */

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
