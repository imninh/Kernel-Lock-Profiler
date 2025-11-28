#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/lockstat.h"

#define BAR_WIDTH 50

void print_bar(int value, int max_value) {
    int bar_len = (value * BAR_WIDTH) / (max_value > 0 ? max_value : 1);
    printf("[");
    for(int i = 0; i < BAR_WIDTH; i++) {
        if(i < bar_len)
            printf("#");
        else
            printf(" ");
    }
    printf("]");
}

int
main(int argc, char *argv[])
{
    struct lock_stat stats[64];
    int count = getlockstats((uint64)stats, 64);
    
    if(count < 0) {
        printf("Failed to get lock statistics\n");
        exit(1);
    }
    
    printf("\n========== LOCK CONTENTION HEATMAP ==========\n\n");
    
    // Tìm max contention để scale bars
    int max_contention = 0;
    for(int i = 0; i < count; i++) {
        if(stats[i].contention_count > max_contention)
            max_contention = stats[i].contention_count;
    }
    
    // In heatmap
    for(int i = 0; i < count; i++) {
        if(stats[i].enabled) {
            printf("%-20s ", stats[i].name);
            print_bar(stats[i].contention_count, max_contention);
            printf(" %d\n", (int)stats[i].contention_count);
        }
    }
    
    printf("\n========== DETAILED STATISTICS ==========\n\n");
    
    for(int i = 0; i < count; i++) {
        if(stats[i].enabled) {
            printf("Lock: %s\n", stats[i].name);
            printf("  Acquires:    %d\n", (int)stats[i].acquire_count);
            printf("  Contentions: %d (%.1d%%)\n", 
                   (int)stats[i].contention_count,
                   (int)((stats[i].contention_count * 100) / 
                         (stats[i].acquire_count > 0 ? stats[i].acquire_count : 1)));
            printf("  Avg Hold:    %d cycles\n", 
                   (int)(stats[i].total_hold_time / 
                         (stats[i].acquire_count > 0 ? stats[i].acquire_count : 1)));
            printf("  Avg Wait:    %d cycles\n", 
                   (int)(stats[i].total_wait_time / 
                         (stats[i].contention_count > 0 ? stats[i].contention_count : 1)));
            printf("\n");
        }
    }
    
    exit(0);
}