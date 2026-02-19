// NPES_256_X_iii_interference.C
// Non Point Entropy System
// Simultaneous Base 10 (even threads) and Base -3 (odd threads) with SIGNED SEEDS
// Author: Nicholas H. Valentine (eNVy) — 2026
// Compile: gcc -O3 -static -pthread

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#define MAX_ROLLS 256
#define NUM_THREADS 32
#define TEST_DURATION_SEC 10
#define LENGTH_RANGE 513  // -256 to +256 inclusive

// SplitMix64 - same for both bases
uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// Thread-local data
typedef struct {
    int thread_id;
    int base;  // 10 for even threads, -3 for odd threads
    uint64_t rng_state;
    uint64_t length_counts[LENGTH_RANGE];  // -256 to +256, indexed as [length + 256]
    uint64_t seeds_generated;
    uint64_t seeds_accepted;
    volatile int* keep_running;
} ThreadData;

// Generate seed with configurable flush rate and base (returns SIGNED length)
int generate_seed(uint64_t* rng_state, int flush_rate, int base) {
    int digit_count = 0;
    int abs_base = (base < 0) ? -base : base;
    
    for (int slot = 0; slot < MAX_ROLLS; slot++) {
        int roll = splitmix64(rng_state) % flush_rate;
        
        if (roll >= abs_base) {
            // Flush: skip this slot
            continue;
        }
        
        // Valid digit
        digit_count++;
    }
    
    // For negative bases, apply 50/50 sign flip
    if (base < 0) {
        int sign_flip = splitmix64(rng_state) & 1;  // 0 or 1
        if (sign_flip) {
            digit_count = -digit_count;
        }
    }
    
    return digit_count;
}

// Thread worker
void* thread_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    while (*(data->keep_running)) {
        int flush_rate = *(data->keep_running + 1);
        int length = generate_seed(&data->rng_state, flush_rate, data->base);
        data->seeds_generated++;
        data->seeds_accepted++;
        data->length_counts[length + 256]++;  // Offset indexing
    }
    
    return NULL;
}

// Results structure for one flush rate - tracks both bases separately and combined
typedef struct {
    int flush_rate;
    double elapsed_time;
    
    // Base 10 aggregate (even threads: 0,2,4,...30)
    uint64_t b10_seeds_generated;
    uint64_t b10_seeds_accepted;
    double b10_seeds_per_sec;
    uint64_t b10_length_counts[LENGTH_RANGE];
    double b10_mean_length;
    double b10_std_dev;
    int b10_min_length;
    int b10_max_length;
    uint64_t b10_count_at_10;
    uint64_t b10_count_at_100;
    double b10_expected_at_10;
    double b10_expected_at_100;
    double b10_delta_at_10;
    double b10_delta_at_100;
    double b10_delta_pct_at_10;
    double b10_delta_pct_at_100;
    
    // Base -3 aggregate (odd threads: 1,3,5,...31) - 10 boundaries (signed)
    uint64_t bneg3_seeds_generated;
    uint64_t bneg3_seeds_accepted;
    double bneg3_seeds_per_sec;
    uint64_t bneg3_length_counts[LENGTH_RANGE];
    double bneg3_mean_length;
    double bneg3_std_dev;
    int bneg3_min_length;
    int bneg3_max_length;
    // Negative boundaries
    uint64_t bneg3_count_at_neg243;
    uint64_t bneg3_count_at_neg81;
    uint64_t bneg3_count_at_neg27;
    uint64_t bneg3_count_at_neg9;
    uint64_t bneg3_count_at_neg3;
    double bneg3_expected_at_neg243;
    double bneg3_expected_at_neg81;
    double bneg3_expected_at_neg27;
    double bneg3_expected_at_neg9;
    double bneg3_expected_at_neg3;
    double bneg3_delta_at_neg243;
    double bneg3_delta_at_neg81;
    double bneg3_delta_at_neg27;
    double bneg3_delta_at_neg9;
    double bneg3_delta_at_neg3;
    double bneg3_delta_pct_at_neg243;
    double bneg3_delta_pct_at_neg81;
    double bneg3_delta_pct_at_neg27;
    double bneg3_delta_pct_at_neg9;
    double bneg3_delta_pct_at_neg3;
    // Positive boundaries
    uint64_t bneg3_count_at_3;
    uint64_t bneg3_count_at_9;
    uint64_t bneg3_count_at_27;
    uint64_t bneg3_count_at_81;
    uint64_t bneg3_count_at_243;
    double bneg3_expected_at_3;
    double bneg3_expected_at_9;
    double bneg3_expected_at_27;
    double bneg3_expected_at_81;
    double bneg3_expected_at_243;
    double bneg3_delta_at_3;
    double bneg3_delta_at_9;
    double bneg3_delta_at_27;
    double bneg3_delta_at_81;
    double bneg3_delta_at_243;
    double bneg3_delta_pct_at_3;
    double bneg3_delta_pct_at_9;
    double bneg3_delta_pct_at_27;
    double bneg3_delta_pct_at_81;
    double bneg3_delta_pct_at_243;
    
    // Combined aggregate (all 32 threads - interference manifold)
    uint64_t combined_seeds_generated;
    uint64_t combined_seeds_accepted;
    double combined_seeds_per_sec;
    uint64_t combined_length_counts[LENGTH_RANGE];
    double combined_mean_length;
    double combined_std_dev;
    int combined_min_length;
    int combined_max_length;
    
} DualFlushResult;

// Calculate statistics for a distribution (Base 10 - 2 boundaries)
void calculate_base_stats(uint64_t* length_counts, uint64_t total,
                          double* mean, double* std_dev, int* min_len, int* max_len,
                          int boundary1, int boundary2,
                          uint64_t* count_b1, uint64_t* count_b2,
                          double* exp_b1, double* exp_b2,
                          double* delta_b1, double* delta_b2,
                          double* delta_pct_b1, double* delta_pct_b2) {
    // Mean (INTEGER ONLY until final conversion)
    int64_t sum = 0;
    for (int i = -256; i <= 256; i++) {
        sum += (int64_t)i * (int64_t)length_counts[i + 256];
    }
    *mean = (double)sum / (double)total;
    
    // Std dev - use integer mean for calculations
    int64_t mean_scaled = (sum * 1000) / (int64_t)total;  // Mean * 1000 for precision
    int64_t var_sum = 0;
    for (int i = -256; i <= 256; i++) {
        if (length_counts[i + 256] > 0) {
            int64_t i_scaled = (int64_t)i * 1000;
            int64_t diff = i_scaled - mean_scaled;
            var_sum += ((diff * diff) / 1000) * (int64_t)length_counts[i + 256];
        }
    }
    *std_dev = sqrt((double)var_sum / ((double)total * 1000.0));
    
    // Min/Max
    *min_len = 256;
    *max_len = -256;
    for (int i = -256; i <= 256; i++) {
        if (length_counts[i + 256] > 0) {
            if (i < *min_len) *min_len = i;
            if (i > *max_len) *max_len = i;
        }
    }
    
    // Boundary 1
    *count_b1 = length_counts[boundary1 + 256];
    if (*count_b1 > 0 && boundary1 > -256 && boundary1 < 256) {
        if (length_counts[boundary1 - 1 + 256] > 0 && length_counts[boundary1 + 1 + 256] > 0) {
            *exp_b1 = (length_counts[boundary1 - 1 + 256] + length_counts[boundary1 + 1 + 256]) / 2.0;
            *delta_b1 = *count_b1 - *exp_b1;
            *delta_pct_b1 = (*delta_b1 / *exp_b1) * 100.0;
        }
    }
    
    // Boundary 2
    *count_b2 = length_counts[boundary2 + 256];
    if (*count_b2 > 0 && boundary2 > -256 && boundary2 < 256) {
        if (length_counts[boundary2 - 1 + 256] > 0 && length_counts[boundary2 + 1 + 256] > 0) {
            *exp_b2 = (length_counts[boundary2 - 1 + 256] + length_counts[boundary2 + 1 + 256]) / 2.0;
            *delta_b2 = *count_b2 - *exp_b2;
            *delta_pct_b2 = (*delta_b2 / *exp_b2) * 100.0;
        }
    }
}

// Calculate statistics for Base -3 (10 signed boundaries)
void calculate_base_neg3_stats(uint64_t* length_counts, uint64_t total,
                               double* mean, double* std_dev, int* min_len, int* max_len,
                               // Negative boundaries
                               uint64_t* count_neg243, uint64_t* count_neg81, uint64_t* count_neg27,
                               uint64_t* count_neg9, uint64_t* count_neg3,
                               double* exp_neg243, double* exp_neg81, double* exp_neg27,
                               double* exp_neg9, double* exp_neg3,
                               double* delta_neg243, double* delta_neg81, double* delta_neg27,
                               double* delta_neg9, double* delta_neg3,
                               double* delta_pct_neg243, double* delta_pct_neg81, double* delta_pct_neg27,
                               double* delta_pct_neg9, double* delta_pct_neg3,
                               // Positive boundaries
                               uint64_t* count_3, uint64_t* count_9, uint64_t* count_27,
                               uint64_t* count_81, uint64_t* count_243,
                               double* exp_3, double* exp_9, double* exp_27,
                               double* exp_81, double* exp_243,
                               double* delta_3, double* delta_9, double* delta_27,
                               double* delta_81, double* delta_243,
                               double* delta_pct_3, double* delta_pct_9, double* delta_pct_27,
                               double* delta_pct_81, double* delta_pct_243) {
    // Mean (INTEGER ONLY until final conversion)
    int64_t sum = 0;
    for (int i = -256; i <= 256; i++) {
        sum += (int64_t)i * (int64_t)length_counts[i + 256];
    }
    *mean = (double)sum / (double)total;
    
    // Std dev - use integer mean for calculations
    int64_t mean_scaled = (sum * 1000) / (int64_t)total;  // Mean * 1000 for precision
    int64_t var_sum = 0;
    for (int i = -256; i <= 256; i++) {
        if (length_counts[i + 256] > 0) {
            int64_t i_scaled = (int64_t)i * 1000;
            int64_t diff = i_scaled - mean_scaled;
            var_sum += ((diff * diff) / 1000) * (int64_t)length_counts[i + 256];
        }
    }
    *std_dev = sqrt((double)var_sum / ((double)total * 1000.0));
    
    // Min/Max
    *min_len = 256;
    *max_len = -256;
    for (int i = -256; i <= 256; i++) {
        if (length_counts[i + 256] > 0) {
            if (i < *min_len) *min_len = i;
            if (i > *max_len) *max_len = i;
        }
    }
    
    // Helper macro for boundary calculation
    #define CALC_BOUNDARY(val, count_ptr, exp_ptr, delta_ptr, delta_pct_ptr) \
        *count_ptr = length_counts[(val) + 256]; \
        if (*count_ptr > 0 && (val) > -256 && (val) < 256) { \
            if (length_counts[(val) - 1 + 256] > 0 && length_counts[(val) + 1 + 256] > 0) { \
                *exp_ptr = (length_counts[(val) - 1 + 256] + length_counts[(val) + 1 + 256]) / 2.0; \
                *delta_ptr = *count_ptr - *exp_ptr; \
                *delta_pct_ptr = (*delta_ptr / *exp_ptr) * 100.0; \
            } \
        }
    
    // Negative boundaries
    CALC_BOUNDARY(-243, count_neg243, exp_neg243, delta_neg243, delta_pct_neg243);
    CALC_BOUNDARY(-81, count_neg81, exp_neg81, delta_neg81, delta_pct_neg81);
    CALC_BOUNDARY(-27, count_neg27, exp_neg27, delta_neg27, delta_pct_neg27);
    CALC_BOUNDARY(-9, count_neg9, exp_neg9, delta_neg9, delta_pct_neg9);
    CALC_BOUNDARY(-3, count_neg3, exp_neg3, delta_neg3, delta_pct_neg3);
    
    // Positive boundaries
    CALC_BOUNDARY(3, count_3, exp_3, delta_3, delta_pct_3);
    CALC_BOUNDARY(9, count_9, exp_9, delta_9, delta_pct_9);
    CALC_BOUNDARY(27, count_27, exp_27, delta_27, delta_pct_27);
    CALC_BOUNDARY(81, count_81, exp_81, delta_81, delta_pct_81);
    CALC_BOUNDARY(243, count_243, exp_243, delta_243, delta_pct_243);
    
    #undef CALC_BOUNDARY
}

int main() {
    printf("NPES_256_X_iii_interference - SIGNED INTERFERENCE TEST\n");
    printf("=========================================\n");
    printf("Base 10 (even threads 0,2,4...30) + Base -3 (odd threads 1,3,5...31)\n");
    printf("Threads: %d\n", NUM_THREADS);
    printf("Duration per flush rate: %d seconds\n", TEST_DURATION_SEC);
    printf("Flush rates: 11 to 111 (odd numbers only)\n");
    printf("Range: -256 to +256 (signed seeds)\n");
    printf("Testing for directional computational superposition in shared L3 cache\n\n");
    
    // Results array
    DualFlushResult results[51];
    int result_count = 0;
    
    // Thread data
    ThreadData thread_data[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    volatile int control[2]; // [0] = keep_running, [1] = flush_rate
    
    // Initialize threads - even=base10, odd=base-3
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].base = (i % 2 == 0) ? 10 : -3;
        thread_data[i].rng_state = (uint64_t)time(NULL) + i * 0x123456789ABCDEFULL;
        thread_data[i].keep_running = (volatile int*)control;
        thread_data[i].seeds_generated = 0;
        thread_data[i].seeds_accepted = 0;
        memset((void*)thread_data[i].length_counts, 0, sizeof(thread_data[i].length_counts));
    }
    
    // Sweep through flush rates
    for (int flush_rate = 11; flush_rate <= 111; flush_rate += 2) {
        printf("\n=== FLUSH RATE: %d ===\n", flush_rate);
        
        // Reset counts
        for (int i = 0; i < NUM_THREADS; i++) {
            thread_data[i].seeds_generated = 0;
            thread_data[i].seeds_accepted = 0;
            memset((void*)thread_data[i].length_counts, 0, sizeof(thread_data[i].length_counts));
        }
        
        // Set control
        control[0] = 1;
        control[1] = flush_rate;
        
        // Launch threads
        time_t start_time = time(NULL);
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, thread_worker, &thread_data[i]);
        }
        
        // Run for TEST_DURATION_SEC seconds
        time_t now;
        do {
            now = time(NULL);
        } while (difftime(now, start_time) < TEST_DURATION_SEC);
        
        // Stop threads
        control[0] = 0;
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        
        time_t end_time = time(NULL);
        double elapsed = difftime(end_time, start_time);
        
        // Aggregate results
        DualFlushResult* res = &results[result_count++];
        memset(res, 0, sizeof(DualFlushResult));
        res->flush_rate = flush_rate;
        res->elapsed_time = elapsed;
        
        // Separate base 10 and base -3, plus combined
        for (int i = 0; i < NUM_THREADS; i++) {
            if (thread_data[i].base == 10) {
                // Base 10 thread (even)
                res->b10_seeds_generated += thread_data[i].seeds_generated;
                res->b10_seeds_accepted += thread_data[i].seeds_accepted;
                for (int j = 0; j < LENGTH_RANGE; j++) {
                    res->b10_length_counts[j] += thread_data[i].length_counts[j];
                }
            } else {
                // Base -3 thread (odd)
                res->bneg3_seeds_generated += thread_data[i].seeds_generated;
                res->bneg3_seeds_accepted += thread_data[i].seeds_accepted;
                for (int j = 0; j < LENGTH_RANGE; j++) {
                    res->bneg3_length_counts[j] += thread_data[i].length_counts[j];
                }
            }
            
            // Combined (all threads)
            res->combined_seeds_generated += thread_data[i].seeds_generated;
            res->combined_seeds_accepted += thread_data[i].seeds_accepted;
            for (int j = 0; j < LENGTH_RANGE; j++) {
                res->combined_length_counts[j] += thread_data[i].length_counts[j];
            }
        }
        
        // Calculate per-second rates
        res->b10_seeds_per_sec = res->b10_seeds_accepted / elapsed;
        res->bneg3_seeds_per_sec = res->bneg3_seeds_accepted / elapsed;
        res->combined_seeds_per_sec = res->combined_seeds_accepted / elapsed;
        
        // Calculate statistics for base 10
        calculate_base_stats(res->b10_length_counts, res->b10_seeds_accepted,
                           &res->b10_mean_length, &res->b10_std_dev,
                           &res->b10_min_length, &res->b10_max_length,
                           10, 100,
                           &res->b10_count_at_10, &res->b10_count_at_100,
                           &res->b10_expected_at_10, &res->b10_expected_at_100,
                           &res->b10_delta_at_10, &res->b10_delta_at_100,
                           &res->b10_delta_pct_at_10, &res->b10_delta_pct_at_100);
        
        // Calculate statistics for base -3
        calculate_base_neg3_stats(res->bneg3_length_counts, res->bneg3_seeds_accepted,
                                 &res->bneg3_mean_length, &res->bneg3_std_dev,
                                 &res->bneg3_min_length, &res->bneg3_max_length,
                                 &res->bneg3_count_at_neg243, &res->bneg3_count_at_neg81,
                                 &res->bneg3_count_at_neg27, &res->bneg3_count_at_neg9,
                                 &res->bneg3_count_at_neg3,
                                 &res->bneg3_expected_at_neg243, &res->bneg3_expected_at_neg81,
                                 &res->bneg3_expected_at_neg27, &res->bneg3_expected_at_neg9,
                                 &res->bneg3_expected_at_neg3,
                                 &res->bneg3_delta_at_neg243, &res->bneg3_delta_at_neg81,
                                 &res->bneg3_delta_at_neg27, &res->bneg3_delta_at_neg9,
                                 &res->bneg3_delta_at_neg3,
                                 &res->bneg3_delta_pct_at_neg243, &res->bneg3_delta_pct_at_neg81,
                                 &res->bneg3_delta_pct_at_neg27, &res->bneg3_delta_pct_at_neg9,
                                 &res->bneg3_delta_pct_at_neg3,
                                 &res->bneg3_count_at_3, &res->bneg3_count_at_9,
                                 &res->bneg3_count_at_27, &res->bneg3_count_at_81,
                                 &res->bneg3_count_at_243,
                                 &res->bneg3_expected_at_3, &res->bneg3_expected_at_9,
                                 &res->bneg3_expected_at_27, &res->bneg3_expected_at_81,
                                 &res->bneg3_expected_at_243,
                                 &res->bneg3_delta_at_3, &res->bneg3_delta_at_9,
                                 &res->bneg3_delta_at_27, &res->bneg3_delta_at_81,
                                 &res->bneg3_delta_at_243,
                                 &res->bneg3_delta_pct_at_3, &res->bneg3_delta_pct_at_9,
                                 &res->bneg3_delta_pct_at_27, &res->bneg3_delta_pct_at_81,
                                 &res->bneg3_delta_pct_at_243);
        
        // Calculate combined statistics (no boundary tracking here - that's for analysis)
        double placeholder_exp, placeholder_delta, placeholder_pct;
        uint64_t placeholder_count;
        calculate_base_stats(res->combined_length_counts, res->combined_seeds_accepted,
                           &res->combined_mean_length, &res->combined_std_dev,
                           &res->combined_min_length, &res->combined_max_length,
                           0, 0, &placeholder_count, &placeholder_count,
                           &placeholder_exp, &placeholder_exp, &placeholder_delta, &placeholder_delta,
                           &placeholder_pct, &placeholder_pct);
        
        // Console output
        printf("Elapsed: %.2f sec\n", elapsed);
        printf("\n[BASE 10 - Even Threads]\n");
        printf("  Seeds: %llu (%.2f M/sec)\n", 
               (unsigned long long)res->b10_seeds_accepted, 
               res->b10_seeds_per_sec / 1000000.0);
        printf("  Mean length: %.2f, Std dev: %.2f\n", 
               res->b10_mean_length, res->b10_std_dev);
        printf("  Range: [%d, %d]\n", res->b10_min_length, res->b10_max_length);
        printf("  Boundary @10: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->b10_count_at_10, 
               res->b10_expected_at_10, res->b10_delta_pct_at_10);
        printf("  Boundary @100: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->b10_count_at_100,
               res->b10_expected_at_100, res->b10_delta_pct_at_100);
        
        printf("\n[BASE -3 - Odd Threads]\n");
        printf("  Seeds: %llu (%.2f M/sec)\n",
               (unsigned long long)res->bneg3_seeds_accepted,
               res->bneg3_seeds_per_sec / 1000000.0);
        printf("  Mean length: %.2f, Std dev: %.2f\n",
               res->bneg3_mean_length, res->bneg3_std_dev);
        printf("  Range: [%d, %d]\n", res->bneg3_min_length, res->bneg3_max_length);
        printf("  Negative boundaries:\n");
        printf("    @-243: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_neg243,
               res->bneg3_expected_at_neg243, res->bneg3_delta_pct_at_neg243);
        printf("    @-81: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_neg81,
               res->bneg3_expected_at_neg81, res->bneg3_delta_pct_at_neg81);
        printf("    @-27: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_neg27,
               res->bneg3_expected_at_neg27, res->bneg3_delta_pct_at_neg27);
        printf("    @-9: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_neg9,
               res->bneg3_expected_at_neg9, res->bneg3_delta_pct_at_neg9);
        printf("    @-3: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_neg3,
               res->bneg3_expected_at_neg3, res->bneg3_delta_pct_at_neg3);
        printf("  Positive boundaries:\n");
        printf("    @3: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_3,
               res->bneg3_expected_at_3, res->bneg3_delta_pct_at_3);
        printf("    @9: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_9,
               res->bneg3_expected_at_9, res->bneg3_delta_pct_at_9);
        printf("    @27: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_27,
               res->bneg3_expected_at_27, res->bneg3_delta_pct_at_27);
        printf("    @81: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_81,
               res->bneg3_expected_at_81, res->bneg3_delta_pct_at_81);
        printf("    @243: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->bneg3_count_at_243,
               res->bneg3_expected_at_243, res->bneg3_delta_pct_at_243);
        
        printf("\n[COMBINED - All Threads]\n");
        printf("  Total seeds: %llu (%.2f M/sec)\n",
               (unsigned long long)res->combined_seeds_accepted,
               res->combined_seeds_per_sec / 1000000.0);
        printf("  Mean length: %.2f, Std dev: %.2f\n",
               res->combined_mean_length, res->combined_std_dev);
        printf("  Range: [%d, %d]\n", res->combined_min_length, res->combined_max_length);
    }
    
    // Write JSON
    printf("\n\nWriting JSON output...\n");
    FILE* fp = fopen("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "w");     // <----- output path
    if (!fp) {
        printf("ERROR: Could not open JSON file for writing!\n");
        return 1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"test_name\": \"NEPS_256_X_iii_interference\",\n");
    fprintf(fp, "  \"threads\": %d,\n", NUM_THREADS);
    fprintf(fp, "  \"base10_threads\": \"0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30\",\n");
    fprintf(fp, "  \"base_neg3_threads\": \"1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31\",\n");
    fprintf(fp, "  \"duration_per_flush\": %d,\n", TEST_DURATION_SEC);
    fprintf(fp, "  \"length_range\": \"-256 to +256\",\n");
    fprintf(fp, "  \"results\": [\n");
    
    for (int r = 0; r < result_count; r++) {
        DualFlushResult* res = &results[r];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"flush_rate\": %d,\n", res->flush_rate);
        fprintf(fp, "      \"elapsed_time\": %.2f,\n", res->elapsed_time);
        
        // Base 10 data
        fprintf(fp, "      \"base10\": {\n");
        fprintf(fp, "        \"seeds_generated\": %llu,\n", (unsigned long long)res->b10_seeds_generated);
        fprintf(fp, "        \"seeds_accepted\": %llu,\n", (unsigned long long)res->b10_seeds_accepted);
        fprintf(fp, "        \"seeds_per_sec\": %.2f,\n", res->b10_seeds_per_sec);
        fprintf(fp, "        \"mean_length\": %.2f,\n", res->b10_mean_length);
        fprintf(fp, "        \"std_dev\": %.2f,\n", res->b10_std_dev);
        fprintf(fp, "        \"min_length\": %d,\n", res->b10_min_length);
        fprintf(fp, "        \"max_length\": %d,\n", res->b10_max_length);
        fprintf(fp, "        \"boundary_10\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->b10_count_at_10);
        fprintf(fp, "          \"expected\": %.2f,\n", res->b10_expected_at_10);
        fprintf(fp, "          \"delta\": %.2f,\n", res->b10_delta_at_10);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->b10_delta_pct_at_10);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_100\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->b10_count_at_100);
        fprintf(fp, "          \"expected\": %.2f,\n", res->b10_expected_at_100);
        fprintf(fp, "          \"delta\": %.2f,\n", res->b10_delta_at_100);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->b10_delta_pct_at_100);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"length_distribution\": [");
        for (int i = 0; i < LENGTH_RANGE; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->b10_length_counts[i]);
        }
        fprintf(fp, "\n        ]\n");
        fprintf(fp, "      },\n");
        
        // Base -3 data
        fprintf(fp, "      \"base_neg3\": {\n");
        fprintf(fp, "        \"seeds_generated\": %llu,\n", (unsigned long long)res->bneg3_seeds_generated);
        fprintf(fp, "        \"seeds_accepted\": %llu,\n", (unsigned long long)res->bneg3_seeds_accepted);
        fprintf(fp, "        \"seeds_per_sec\": %.2f,\n", res->bneg3_seeds_per_sec);
        fprintf(fp, "        \"mean_length\": %.2f,\n", res->bneg3_mean_length);
        fprintf(fp, "        \"std_dev\": %.2f,\n", res->bneg3_std_dev);
        fprintf(fp, "        \"min_length\": %d,\n", res->bneg3_min_length);
        fprintf(fp, "        \"max_length\": %d,\n", res->bneg3_max_length);
        
        // Negative boundaries
        fprintf(fp, "        \"boundary_neg243\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_neg243);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_neg243);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_neg243);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_neg243);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_neg81\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_neg81);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_neg81);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_neg81);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_neg81);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_neg27\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_neg27);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_neg27);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_neg27);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_neg27);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_neg9\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_neg9);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_neg9);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_neg9);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_neg9);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_neg3\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_neg3);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_neg3);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_neg3);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_neg3);
        fprintf(fp, "        },\n");
        
        // Positive boundaries
        fprintf(fp, "        \"boundary_3\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_3);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_3);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_3);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_3);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_9\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_9);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_9);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_9);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_9);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_27\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_27);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_27);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_27);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_27);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_81\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_81);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_81);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_81);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_81);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_243\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->bneg3_count_at_243);
        fprintf(fp, "          \"expected\": %.2f,\n", res->bneg3_expected_at_243);
        fprintf(fp, "          \"delta\": %.2f,\n", res->bneg3_delta_at_243);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->bneg3_delta_pct_at_243);
        fprintf(fp, "        },\n");
        
        fprintf(fp, "        \"length_distribution\": [");
        for (int i = 0; i < LENGTH_RANGE; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->bneg3_length_counts[i]);
        }
        fprintf(fp, "\n        ]\n");
        fprintf(fp, "      },\n");
        
        // Combined data
        fprintf(fp, "      \"combined\": {\n");
        fprintf(fp, "        \"seeds_generated\": %llu,\n", (unsigned long long)res->combined_seeds_generated);
        fprintf(fp, "        \"seeds_accepted\": %llu,\n", (unsigned long long)res->combined_seeds_accepted);
        fprintf(fp, "        \"seeds_per_sec\": %.2f,\n", res->combined_seeds_per_sec);
        fprintf(fp, "        \"mean_length\": %.2f,\n", res->combined_mean_length);
        fprintf(fp, "        \"std_dev\": %.2f,\n", res->combined_std_dev);
        fprintf(fp, "        \"min_length\": %d,\n", res->combined_min_length);
        fprintf(fp, "        \"max_length\": %d,\n", res->combined_max_length);
        fprintf(fp, "        \"length_distribution\": [");
        for (int i = 0; i < LENGTH_RANGE; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->combined_length_counts[i]);
        }
        fprintf(fp, "\n        ]\n");
        fprintf(fp, "      }\n");
        
        fprintf(fp, "    }%s\n", (r < result_count - 1) ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    fclose(fp);
    
    printf("JSON written to: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");     // <----- output path
    printf("\nSIGNED INTERFERENCE TEST COMPLETE!\n");
    printf("Analyze the combined distribution for directional superposition effects.\n");
    printf("Base -3 creates symmetric pressure around zero.\n");
    printf("Base 10 creates positive-only pressure.\n");
    
    return 0;
}