#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h" 
#include "user/lockstat.h" 

// Mã màu ANSI cho Terminal
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Khai báo hàm syscall wrapper 
int lockstat(void *buf, int max_locks); 
// 1. Buffer RAW: Dùng để nhận dữ liệu thô từ Kernel (ngăn Stack Overflow)
// Kích thước phải khớp với MAX_LOCKS và kích thước của struct RAW.
static struct lock_stat_raw raw_stats_buffer[MAX_LOCKS]; 

// 2. Mảng DATA: Dùng để phân tích và xếp hạng (chứa cả dữ liệu thô và các chỉ số tính toán)
static struct lock_stat_data analyzed_stats[MAX_LOCKS];

// Sắp xếp theo Contention Rate giảm dần(bubble sort)
void sort_stats(struct lock_stat_data *stats, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (stats[j].contention_x10 < stats[j + 1].contention_x10) {
                // Swap
                struct lock_stat_data temp = stats[j];
                stats[j] = stats[j + 1];
                stats[j + 1] = temp;
            }
        }
    }
}


// Tính toán Metrics (Contention Rate, Avg Hold Time) ---
// Chạy trên mảng dữ liệu phân tích
int calculate_metrics(struct lock_stat_data *stats, int lock_count) {
    for (int i = 0; i < lock_count; i++) {
        // Truy cập qua trường .raw
        if (stats[i].raw.acquire_count > 0) {
            // 1. Contention Rate = Contentions / Acquires
            // Store as tenths of percent to avoid floating point. Multiply first to preserve precision.
            // contention_x10 = (contention_count / acquire_count) * 100 * 10
            // = contention_count * 1000 / acquire_count
            if (stats[i].raw.acquire_count != 0) {
                stats[i].contention_x10 = (unsigned int)((stats[i].raw.contention_count * 1000ULL) / stats[i].raw.acquire_count);
            } else {
                stats[i].contention_x10 = 0;
            }

            // 2. Average Hold Time
            stats[i].avg_hold_time = stats[i].raw.total_hold_time / stats[i].raw.acquire_count;
        } else {
            stats[i].contention_x10 = 0;
            stats[i].avg_hold_time = 0;
        }
    }
    return 0;
}


// --- Helper: Print Final Results (Visualization Logic) ---
void print_results(struct lock_stat_data *stats, int count) {
    fprintf(1,"=================================================================\n");
    fprintf(1, "| %s | %s | %s | %s | %s | %s |\n",
        "RANK", "LOCK NAME", "ACQ", "CONT%", "AVG HOLD (cycles)", "SEV");
    fprintf(1, "=================================================================\n");

    for (int i = 0; i < count; i++) {
        // Lấy dữ liệu thô từ trường .raw để kiểm tra
        if (stats[i].raw.acquire_count > 0) {
            // Áp dụng logic Mã màu (Severity Marker)
            char *color = ANSI_COLOR_RESET;
            char *severity = "-";
            // thresholds use tenths of percent: 50.0% -> 500, 10.0% -> 100
            if (stats[i].contention_x10 > 200) {
                severity = "!!!"; // CRITICAL (Rất nóng)
                color = ANSI_COLOR_RED;
            } else if (stats[i].contention_x10 > 100) {
                severity = "**"; // HIGH (Nóng)
                color = ANSI_COLOR_YELLOW;
            } else {
                severity = "-"; // Normal
                color = ANSI_COLOR_GREEN;
            }

            // print contention as percent with one decimal using integer math
            int pct_int = stats[i].contention_x10 / 10; // integer part
            int pct_frac = stats[i].contention_x10 % 10; // tenths

            // In ra với mã màu bao quanh
            fprintf(1, "%s| %d | %s | %d | %d.%d%% | %d | %s |%s\n",
                   color,
                   i + 1,
                   stats[i].raw.name,
                   (int)stats[i].raw.acquire_count,
                   pct_int, pct_frac,
                   (int)stats[i].avg_hold_time,
                   severity,
                   ANSI_COLOR_RESET);
        }
    }
    fprintf( 1, "=================================================================\n");
}


// --- Helper: In định dạng CSV (Mới) ---
void print_csv(struct lock_stat_data *stats, int count) {
    fprintf(1, "Lock_Name,Acquire_Count,Contention_Count,Contention_Rate_Percent,Avg_Hold_Time_Cycles\n");

    for (int i = 0; i < count; i++) {
        if (stats[i].raw.acquire_count > 0) {
        /* Print percent as one-decimal fixed-point
         */
        int pct_int = stats[i].contention_x10 / 10;   // integer part
        int pct_frac = stats[i].contention_x10 % 10;  // one decimal digit
        fprintf(1, "%s,%d,%d,%d.%d,%d\n",
            stats[i].raw.name,
            (int)stats[i].raw.acquire_count,
            (int)stats[i].raw.contention_count,
            pct_int,
            pct_frac,
            (int)stats[i].avg_hold_time);
        }
    }
}


int main(int argc, char *argv[])
{
    // Kích thước bytes cần thiết để copy từ Kernel
    int len_bytes = sizeof(raw_stats_buffer); 

    fprintf(1,"Starting Lock Profiler Analysis...\n");

    // 1. Gọi System Call để nhận mảng struct RAW
    int lock_count = lockstat(raw_stats_buffer, len_bytes);
    
    if (lock_count <= 0) {
        fprintf(1, "lockstat: No data or error. Run 'locktest' first.\n");
        exit(1);
    }
    
    // 2. Chuyển dữ liệu RAW sang mảng PHÂN TÍCH và xử lý
    for (int i = 0; i < lock_count; i++) {
        // Gán dữ liệu thô từ buffer nhận được vào mảng phân tích
        analyzed_stats[i].raw = raw_stats_buffer[i]; 
    }

    // 3. Tính toán metrics và Sắp xếp
    calculate_metrics(analyzed_stats, lock_count);
    sort_stats(analyzed_stats, lock_count);

        int csv_mode = 0;
        int both_mode = 0;
        if (argc > 1) {
            if (strcmp(argv[1], "-c") == 0) {
                csv_mode = 1;
            } else if (strcmp(argv[1], "-b") == 0) {
                both_mode = 1;
            }
        }

        if (both_mode) {
            // In bảng rồi in CSV từ cùng một snapshot
            print_results(analyzed_stats, lock_count);
            print_csv(analyzed_stats, lock_count);
        } else if (csv_mode) {
            print_csv(analyzed_stats, lock_count);
        } else {
            // 4. In ra Bảng xếp hạng
            print_results(analyzed_stats, lock_count);
        }
    
    exit(0);
}