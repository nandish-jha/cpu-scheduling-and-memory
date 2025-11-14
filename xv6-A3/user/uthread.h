/*
AAKASH JANA AAJ284 11297343
NANDISH JHA NAJ474 11282001
*/

// Simple user-level threads interface for xv6 user programs
// Extracted from uthread.c to allow reuse by other programs

#ifndef UTHREAD_H
#define UTHREAD_H

void thread_init(void);
void thread_schedule(void);
void thread_create(void (*func)());
void thread_yield(void);
void thread_exit(void);

#endif // UTHREAD_H
