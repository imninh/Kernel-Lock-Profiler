// lockstat.h - Lock profiling structures
#include "types.h"
#ifndef LOCKSTAT_H
#define LOCKSTAT_H

#define MAX_LOCKS 64
#define MAX_LOCK_NAME 32

struct lock_stat {
    char name[MAX_LOCK_NAME];
    uint64 acquire_count;      // Số lần lock được acquire
    uint64 contention_count;   // Số lần bị contention
    uint64 total_hold_time;    // Tổng thời gian giữ lock (cycles)
    uint64 total_wait_time;    // Tổng thời gian chờ lock (cycles)
    int enabled;
};

void lockstat_init(void);
void lockstat_record_acquire(struct spinlock *lk, uint64 wait_time);
void lockstat_record_release(struct spinlock *lk, uint64 hold_time);
void lockstat_print(void);

#endif