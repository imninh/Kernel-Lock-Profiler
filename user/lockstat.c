// user/lockstat.c - User Tool for Kernel Lock Profiler (Final 80% Logic)

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h" 
#include "user/lockstat.h" // CHỨA STRUCT CỦA BẠN (lock_stat_raw, lock_stat_data, MAX_LOCKS)

// Khai báo hàm syscall wrapper (Phải dùng tên đã đăng ký: lockstat)
// Kernel sẽ trả về số lượng lock hợp lệ đã được copy.
int lockstat(void *buf, int max_locks); 

// --- BUFFER TOÀN CỤC VÀ DỮ LIỆU PHÂN TÍCH ---
// 1. Buffer RAW: Dùng để nhận dữ liệu thô từ Kernel (ngăn Stack Overflow)
// Kích thước phải khớp với MAX_LOCKS và kích thước của struct RAW.
static struct lock_stat_raw raw_stats_buffer[MAX_LOCKS]; 

// 2. Mảng DATA: Dùng để phân tích và xếp hạng (chứa cả dữ liệu thô và các chỉ số tính toán)
static struct lock_stat_data analyzed_stats[MAX_LOCKS];


// --- Helper: Implement Sorting Logic (Bubble Sort) ---
// Sắp xếp theo Contention Rate giảm dần
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


// --- Helper: Tính toán Metrics (Contention Rate, Avg Hold Time) ---
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
    printf("=================================================================\n");
    printf("| %s | %s | %s | %s | %s | %s |\n",
        "RANK", "LOCK NAME", "ACQ", "CONT%", "AVG HOLD (cycles)", "SEV");
    printf("=================================================================\n");

    for (int i = 0; i < count; i++) {
        // Lấy dữ liệu thô từ trường .raw để kiểm tra
        if (stats[i].raw.acquire_count > 0) { 
            
            // Áp dụng logic Mã màu (Severity Marker)
            char *severity;
            // thresholds use tenths of percent: 50.0% -> 500, 10.0% -> 100
            if (stats[i].contention_x10 > 500) {
                severity = "!!!"; // CRITICAL (Rất nóng)
            } else if (stats[i].contention_x10 > 100) {
                severity = "**"; // HIGH (Nóng)
            } else {
                severity = "-"; // Normal
            }

            // print contention as percent with one decimal using integer math
            int pct_int = stats[i].contention_x10 / 10; // integer part
            int pct_frac = stats[i].contention_x10 % 10; // tenths

         printf("| %d | %s | %d | %d.%d%% | %d | %s |\n",
             i + 1,
             stats[i].raw.name,
             (int)stats[i].raw.acquire_count,
             pct_int, pct_frac,
             (int)stats[i].avg_hold_time,
             severity);
        }
    }
    printf("=================================================================\n");
}


int main(int argc, char *argv[])
{
    // Kích thước bytes cần thiết để copy từ Kernel (chỉ cần kích thước RAW)
    int len_bytes = sizeof(raw_stats_buffer); 

    printf("Starting Lock Profiler Analysis...\n");

    // 1. Gọi System Call để nhận mảng struct RAW
    int lock_count = lockstat(raw_stats_buffer, len_bytes);
    
    if (lock_count <= 0) {
        printf("lockstat: No data or error. Run 'locktest' first.\n");
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

    // 4. In ra Bảng xếp hạng (Visualization)
    print_results(analyzed_stats, lock_count); 
    
    exit(0);
}