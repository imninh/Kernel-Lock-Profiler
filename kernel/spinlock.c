#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
  lk->acquire_time = 0;
}


void
acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  uint64 wait_time = 0;
  int first = __sync_lock_test_and_set(&lk->locked, 1);
  uint64 start_time = 0, end_time = 0;
  if (first != 0) {
    // measure how long we spin until we actually acquire it.
    start_time = r_time();
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
      ;
    end_time = r_time();
    wait_time = end_time - start_time;
    // Ignore negligible waits (possible due to timer granularity or
    // instruction timing) to avoid counting tiny deltas as contention.
    if (wait_time <= 1)
      wait_time = 0;
  } else {
    // Fast path: acquired immediately. Use current time as acquire_time.
    end_time = r_time();
    wait_time = 0;
  }

  __sync_synchronize();

  // Record that this cpu holds the lock.
  lk->cpu = mycpu();
  lk->acquire_time = end_time; // store the acquire timestamp
  
  // Record statistics
  lockstat_record_acquire(lk, wait_time); // ← Track statistics
}

void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  uint64 hold_time = r_time() - lk->acquire_time; // ← Tính thời gian giữ lock
  lockstat_record_release(lk, hold_time); // ← Track statistics

  lk->cpu = 0;

  __sync_synchronize();
  __sync_lock_release(&lk->locked);

  pop_off();
}
// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}


void
push_off(void)
{
  int old = intr_get();

  // disable interrupts to prevent an involuntary context
  // switch while using mycpu().
  intr_off();

  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}
