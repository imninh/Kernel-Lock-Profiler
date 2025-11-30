// user/lockstat.h
#include "kernel/types.h"

#define MAX_LOCKS 64
#define MAX_LOCK_NAME 32

// Struct này phải khớp với struct lock_stat trong kernel (ít nhất là các trường chính)
struct lock_stat_raw {
    char name[MAX_LOCK_NAME];
    uint64 acquire_count;
    uint64 contention_count;
    uint64 total_hold_time;
    uint64 total_wait_time;
    uint64 max_hold_time;
    uint64 max_wait_time;
    uint64 last_acquire_time; 
    int enabled;
};

struct lock_stat_data {
    struct lock_stat_raw raw; // Nhận dữ liệu thô
    
    // Các trường để tính toán và xếp hạng (chỉ có trong user space)
    // contention_rate stored as tenths of percent (e.g. 125 -> 12.5%)
    unsigned int contention_x10;
    uint64 avg_hold_time;
};
