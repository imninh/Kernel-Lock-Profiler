// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?
  uint64 acquire_time; // Thời điểm acquire lock (THÊM DÒNG NÀY)

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};
