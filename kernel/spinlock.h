// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?
  uint64 acquire_time; // Thời điểm acquire lock (THÊM DÒNG NÀY)

  // For debugging:
  char *name;        // Name of lock.
  int no_track;      // If set, this lock should not be tracked by lockstat
  struct cpu *cpu;   // The cpu holding the lock.
};
