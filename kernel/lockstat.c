#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "lockstat.h"

// Use the public definition from lockstat.h and expose the array used
// internally by the kernel for tracking lock statistics.
struct lock_stat lock_stats[MAX_LOCKS];
int lock_count = 0;
struct spinlock lockstat_lock;
int lockstat_enabled = 0;  // flag to enable/disable tracking

// Helper function để copy string
static void
mystrncpy(char *dst, const char *src, int n)
{
  int i;
  for(i = 0; i < n-1 && src[i] != '\0'; i++)
    dst[i] = src[i];
  dst[i] = '\0';
}

// Helper function để so sánh string
static int
mystrcmp(const char *s1, const char *s2, int n)
{
  int i;
  for(i = 0; i < n && s1[i] != '\0' && s2[i] != '\0'; i++) {
    if(s1[i] != s2[i])
      return s1[i] - s2[i];
  }
  return 0;
}

void 
lockstat_init(void) 
{
    initlock(&lockstat_lock, "lockstat");
    for(int i = 0; i < MAX_LOCKS; i++) {
        lock_stats[i].enabled = 0;
        lock_stats[i].acquire_count = 0;
        lock_stats[i].contention_count = 0;
        lock_stats[i].total_hold_time = 0;
        lock_stats[i].total_wait_time = 0;
    }
    lockstat_enabled = 1;  // ← Enable tracking SAU KHI khởi tạo xong
}

// Tìm hoặc tạo entry cho lock
static int 
get_lock_index(struct spinlock *lk) 
{
    // TRÁNH tracking chính lockstat_lock để không bị recursion
    if(lk == &lockstat_lock)
        return -1;
    
    acquire(&lockstat_lock);
    
    // Tìm lock đã tồn tại
    for(int i = 0; i < lock_count; i++) {
        if(mystrcmp(lock_stats[i].name, lk->name, MAX_LOCK_NAME) == 0) {
            release(&lockstat_lock);
            return i;
        }
    }
    
    // Tạo entry mới
    if(lock_count < MAX_LOCKS) {
        int idx = lock_count++;
        mystrncpy(lock_stats[idx].name, lk->name, MAX_LOCK_NAME);
        lock_stats[idx].enabled = 1;
        release(&lockstat_lock);
        return idx;
    }
    
    release(&lockstat_lock);
    return -1;
}

void 
lockstat_record_acquire(struct spinlock *lk, uint64 wait_time) 
{
    if(!lockstat_enabled || lk->no_track)
        return;
    
    int idx = get_lock_index(lk);
    if(idx < 0) return;
    
    __sync_fetch_and_add(&lock_stats[idx].acquire_count, 1);
    
    if(wait_time > 0) {
        __sync_fetch_and_add(&lock_stats[idx].contention_count, 1);
        __sync_fetch_and_add(&lock_stats[idx].total_wait_time, wait_time);
        
        // Update max wait time
        if(wait_time > lock_stats[idx].max_wait_time)
            lock_stats[idx].max_wait_time = wait_time;
    }
    
    lock_stats[idx].last_acquire_time = r_time();
}

void 
lockstat_record_release(struct spinlock *lk, uint64 hold_time) 
{
    if(!lockstat_enabled || lk->no_track)
        return;
    
    int idx = get_lock_index(lk);
    if(idx < 0) return;
    
    __sync_fetch_and_add(&lock_stats[idx].total_hold_time, hold_time);
    
    // Update max hold time
    if(hold_time > lock_stats[idx].max_hold_time)
        lock_stats[idx].max_hold_time = hold_time;
}

void 
lockstat_print(void) 
{
    printf("=== Lock Profiling Statistics ===\n\n");
    
    for(int i = 0; i < lock_count; i++) {
        if(lock_stats[i].enabled) {
            printf("Lock: %s\n", lock_stats[i].name);
            printf("  Acquires:    %d\n", (int)lock_stats[i].acquire_count);
            printf("  Contentions: %d\n", (int)lock_stats[i].contention_count);
            printf("  Hold Time:   %d cycles\n", (int)lock_stats[i].total_hold_time);
            printf("  Wait Time:   %d cycles\n", (int)lock_stats[i].total_wait_time);
            printf("\n");
        }
    }
}

// Copy stats sang user space
uint64
lockstat_copy_to_user(uint64 addr, int max_locks)
{
  struct proc *p = myproc();
  int count = lock_count < max_locks ? lock_count : max_locks;
  
  for(int i = 0; i < count; i++) {
    if(copyout(p->pagetable, addr + i * sizeof(struct lock_stat),
               (char*)&lock_stats[i], sizeof(struct lock_stat)) < 0)
      return -1;
  }
  
  return count;
}