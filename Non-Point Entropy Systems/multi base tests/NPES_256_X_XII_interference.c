// NPES_256_X_XII_interference.C
// Non Point Entropy System
// Simultaneous Base 10 (even threads) and Base 12 (odd threads)
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
    int base;  // 10 for even threads, 12 for odd threads
    uint64_t rng_state;
    uint64_t length_counts[257];
    uint64_t seeds_generated;
    uint64_t seeds_accepted;
    volatile int* keep_running;
} ThreadData;

// Generate seed with configurable flush rate and base
int generate_seed(uint64_t* rng_state, int flush_rate, int base) {
    int digit_count = 0;
    
    for (int slot = 0; slot < MAX_ROLLS; slot++) {
        int roll = splitmix64(rng_state) % flush_rate;
        
        if (roll >= base) {
            // Flush: skip this slot
            continue;
        }
        
        // Valid digit
        digit_count++;
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
        data->length_counts[length]++;
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
    uint64_t b10_length_counts[257];
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
    
    // Base 12 aggregate (odd threads: 1,3,5,...31)
    uint64_t b12_seeds_generated;
    uint64_t b12_seeds_accepted;
    double b12_seeds_per_sec;
    uint64_t b12_length_counts[257];
    double b12_mean_length;
    double b12_std_dev;
    int b12_min_length;
    int b12_max_length;
    uint64_t b12_count_at_12;
    uint64_t b12_count_at_144;
    double b12_expected_at_12;
    double b12_expected_at_144;
    double b12_delta_at_12;
    double b12_delta_at_144;
    double b12_delta_pct_at_12;
    double b12_delta_pct_at_144;
    
    // Combined aggregate (all 32 threads - interference manifold)
    uint64_t combined_seeds_generated;
    uint64_t combined_seeds_accepted;
    double combined_seeds_per_sec;
    uint64_t combined_length_counts[257];
    double combined_mean_length;
    double combined_std_dev;
    int combined_min_length;
    int combined_max_length;
    
} DualFlushResult;

// Calculate statistics for a distribution
void calculate_base_stats(uint64_t* length_counts, uint64_t total,
                          double* mean, double* std_dev, int* min_len, int* max_len,
                          int boundary1, int boundary2,
                          uint64_t* count_b1, uint64_t* count_b2,
                          double* exp_b1, double* exp_b2,
                          double* delta_b1, double* delta_b2,
                          double* delta_pct_b1, double* delta_pct_b2) {
    // Mean
    double sum = 0;
    for (int i = 0; i <= 256; i++) {
        sum += i * length_counts[i];
    }
    *mean = sum / total;
    
    // Std dev
    double var_sum = 0;
    for (int i = 0; i <= 256; i++) {
        if (length_counts[i] > 0) {
            double diff = i - *mean;
            var_sum += diff * diff * length_counts[i];
        }
    }
    *std_dev = sqrt(var_sum / total);
    
    // Min/Max
    *min_len = 256;
    *max_len = 0;
    for (int i = 0; i <= 256; i++) {
        if (length_counts[i] > 0) {
            if (i < *min_len) *min_len = i;
            if (i > *max_len) *max_len = i;
        }
    }
    
    // Boundary 1
    *count_b1 = length_counts[boundary1];
    if (*count_b1 > 0 && boundary1 > 0 && boundary1 < 256) {
        if (length_counts[boundary1-1] > 0 && length_counts[boundary1+1] > 0) {
            *exp_b1 = (length_counts[boundary1-1] + length_counts[boundary1+1]) / 2.0;
            *delta_b1 = *count_b1 - *exp_b1;
            *delta_pct_b1 = (*delta_b1 / *exp_b1) * 100.0;
        }
    }
    
    // Boundary 2
    *count_b2 = length_counts[boundary2];
    if (*count_b2 > 0 && boundary2 > 0 && boundary2 < 256) {
        if (length_counts[boundary2-1] > 0 && length_counts[boundary2+1] > 0) {
            *exp_b2 = (length_counts[boundary2-1] + length_counts[boundary2+1]) / 2.0;
            *delta_b2 = *count_b2 - *exp_b2;
            *delta_pct_b2 = (*delta_b2 / *exp_b2) * 100.0;
        }
    }
}

int main() {
    printf("NPES_256_X_XII_interference - INTERFERENCE TEST\n");
    printf("==================================\n");
    printf("Base 10 (even threads 0,2,4...30) + Base 12 (odd threads 1,3,5...31)\n");
    printf("Threads: %d\n", NUM_THREADS);
    printf("Duration per flush rate: %d seconds\n", TEST_DURATION_SEC);
    printf("Flush rates: 11 to 111 (odd numbers only)\n");
    printf("Testing for computational superposition in shared L3 cache\n\n");
    
    // Results array
    DualFlushResult results[51];
    int result_count = 0;
    
    // Thread data
    ThreadData thread_data[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    volatile int control[2]; // [0] = keep_running, [1] = flush_rate
    
    // Initialize threads - even=base10, odd=base12
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].base = (i % 2 == 0) ? 10 : 12;
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
        
        // Separate base 10 and base 12, plus combined
        for (int i = 0; i < NUM_THREADS; i++) {
            if (thread_data[i].base == 10) {
                // Base 10 thread (even)
                res->b10_seeds_generated += thread_data[i].seeds_generated;
                res->b10_seeds_accepted += thread_data[i].seeds_accepted;
                for (int j = 0; j <= 256; j++) {
                    res->b10_length_counts[j] += thread_data[i].length_counts[j];
                }
            } else {
                // Base 12 thread (odd)
                res->b12_seeds_generated += thread_data[i].seeds_generated;
                res->b12_seeds_accepted += thread_data[i].seeds_accepted;
                for (int j = 0; j <= 256; j++) {
                    res->b12_length_counts[j] += thread_data[i].length_counts[j];
                }
            }
            
            // Combined (all threads)
            res->combined_seeds_generated += thread_data[i].seeds_generated;
            res->combined_seeds_accepted += thread_data[i].seeds_accepted;
            for (int j = 0; j <= 256; j++) {
                res->combined_length_counts[j] += thread_data[i].length_counts[j];
            }
        }
        
        // Calculate per-second rates
        res->b10_seeds_per_sec = res->b10_seeds_accepted / elapsed;
        res->b12_seeds_per_sec = res->b12_seeds_accepted / elapsed;
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
        
        // Calculate statistics for base 12
        calculate_base_stats(res->b12_length_counts, res->b12_seeds_accepted,
                           &res->b12_mean_length, &res->b12_std_dev,
                           &res->b12_min_length, &res->b12_max_length,
                           12, 144,
                           &res->b12_count_at_12, &res->b12_count_at_144,
                           &res->b12_expected_at_12, &res->b12_expected_at_144,
                           &res->b12_delta_at_12, &res->b12_delta_at_144,
                           &res->b12_delta_pct_at_12, &res->b12_delta_pct_at_144);
        
        // Calculate combined statistics (no boundary tracking here - that's for analysis)
        double dummy_exp, dummy_delta, dummy_pct;
        uint64_t dummy_count;
        calculate_base_stats(res->combined_length_counts, res->combined_seeds_accepted,
                           &res->combined_mean_length, &res->combined_std_dev,
                           &res->combined_min_length, &res->combined_max_length,
                           0, 0, &dummy_count, &dummy_count,
                           &dummy_exp, &dummy_exp, &dummy_delta, &dummy_delta,
                           &dummy_pct, &dummy_pct);
        
        // Console output
        printf("Elapsed: %.2f sec\n", elapsed);
        printf("\n[BASE 10 - Even Threads]\n");
        printf("  Seeds: %llu (%.2f M/sec)\n", 
               (unsigned long long)res->b10_seeds_accepted, 
               res->b10_seeds_per_sec / 1000000.0);
        printf("  Mean length: %.2f, Std dev: %.2f\n", 
               res->b10_mean_length, res->b10_std_dev);
        printf("  Boundary @10: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->b10_count_at_10, 
               res->b10_expected_at_10, res->b10_delta_pct_at_10);
        printf("  Boundary @100: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->b10_count_at_100,
               res->b10_expected_at_100, res->b10_delta_pct_at_100);
        
        printf("\n[BASE 12 - Odd Threads]\n");
        printf("  Seeds: %llu (%.2f M/sec)\n",
               (unsigned long long)res->b12_seeds_accepted,
               res->b12_seeds_per_sec / 1000000.0);
        printf("  Mean length: %.2f, Std dev: %.2f\n",
               res->b12_mean_length, res->b12_std_dev);
        printf("  Boundary @12: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->b12_count_at_12,
               res->b12_expected_at_12, res->b12_delta_pct_at_12);
        printf("  Boundary @144: %llu vs %.2f (%.4f%%)\n",
               (unsigned long long)res->b12_count_at_144,
               res->b12_expected_at_144, res->b12_delta_pct_at_144);
        
        printf("\n[COMBINED - All Threads]\n");
        printf("  Total seeds: %llu (%.2f M/sec)\n",
               (unsigned long long)res->combined_seeds_accepted,
               res->combined_seeds_per_sec / 1000000.0);
        printf("  Mean length: %.2f, Std dev: %.2f\n",
               res->combined_mean_length, res->combined_std_dev);
    }
    
    // Write JSON
    printf("\n\nWriting JSON output...\n");
    FILE* fp = fopen("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "w");     // <----- output path
    if (!fp) {
        printf("ERROR: Could not open JSON file for writing!\n");
        return 1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"test_name\": \"NPES_256_X_XII_interference\",\n");
    fprintf(fp, "  \"threads\": %d,\n", NUM_THREADS);
    fprintf(fp, "  \"base10_threads\": \"0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30\",\n");
    fprintf(fp, "  \"base12_threads\": \"1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31\",\n");
    fprintf(fp, "  \"duration_per_flush\": %d,\n", TEST_DURATION_SEC);
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
        for (int i = 0; i <= 256; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->b10_length_counts[i]);
        }
        fprintf(fp, "\n        ]\n");
        fprintf(fp, "      },\n");
        
        // Base 12 data
        fprintf(fp, "      \"base12\": {\n");
        fprintf(fp, "        \"seeds_generated\": %llu,\n", (unsigned long long)res->b12_seeds_generated);
        fprintf(fp, "        \"seeds_accepted\": %llu,\n", (unsigned long long)res->b12_seeds_accepted);
        fprintf(fp, "        \"seeds_per_sec\": %.2f,\n", res->b12_seeds_per_sec);
        fprintf(fp, "        \"mean_length\": %.2f,\n", res->b12_mean_length);
        fprintf(fp, "        \"std_dev\": %.2f,\n", res->b12_std_dev);
        fprintf(fp, "        \"min_length\": %d,\n", res->b12_min_length);
        fprintf(fp, "        \"max_length\": %d,\n", res->b12_max_length);
        fprintf(fp, "        \"boundary_12\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->b12_count_at_12);
        fprintf(fp, "          \"expected\": %.2f,\n", res->b12_expected_at_12);
        fprintf(fp, "          \"delta\": %.2f,\n", res->b12_delta_at_12);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->b12_delta_pct_at_12);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"boundary_144\": {\n");
        fprintf(fp, "          \"actual\": %llu,\n", (unsigned long long)res->b12_count_at_144);
        fprintf(fp, "          \"expected\": %.2f,\n", res->b12_expected_at_144);
        fprintf(fp, "          \"delta\": %.2f,\n", res->b12_delta_at_144);
        fprintf(fp, "          \"delta_pct\": %.4f\n", res->b12_delta_pct_at_144);
        fprintf(fp, "        },\n");
        fprintf(fp, "        \"length_distribution\": [");
        for (int i = 0; i <= 256; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->b12_length_counts[i]);
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
        for (int i = 0; i <= 256; i++) {
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
    
    printf("JSON written to: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");     // <----- output path
    printf("\nINTERFERENCE TEST COMPLETE!\n");
    printf("Analyze the combined distribution for superposition effects.\n");
    
    return 0;
}