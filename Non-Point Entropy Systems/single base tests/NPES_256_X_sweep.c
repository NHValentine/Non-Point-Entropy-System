// NPES_256_X.C
// The Non Point Entropy System
// Time-based sweep test: 10 seconds per flush rate (11-111, odd numbers only)
// Base 10 digit generation
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
#define BASE 10

// SplitMix64
uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// Thread-local data
typedef struct {
    int thread_id;
    uint64_t rng_state;
    uint64_t length_counts[257];
    uint64_t seeds_generated;
    uint64_t seeds_accepted;
    volatile int* keep_running;
} ThreadData;

// Generate seed with configurable flush rate
int generate_seed(uint64_t* rng_state, int flush_rate) {
    int digit_count = 0;
    
    for (int slot = 0; slot < MAX_ROLLS; slot++) {
        int roll = splitmix64(rng_state) % flush_rate;
        
        if (roll >= BASE) {
            // Flush: skip this slot
            continue;
        }
        
        // Valid digit (0-9 for base 10)
        digit_count++;
    }
    
    return digit_count;
}

// Thread worker
void* thread_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    while (*(data->keep_running)) {
        // Generate seed
        int length = generate_seed(&data->rng_state, *(data->keep_running + 1)); // flush_rate in keep_running[1]
        data->seeds_generated++;
        data->seeds_accepted++;
        data->length_counts[length]++;
    }
    
    return NULL;
}

// Results structure for one flush rate
typedef struct {
    int flush_rate;
    double elapsed_time;
    uint64_t seeds_generated;
    uint64_t seeds_accepted;
    double seeds_per_sec_accepted;
    double seeds_per_sec_total;
    double acceptance_rate;
    uint64_t length_counts[257];
    double mean_length;
    double median_length;
    double std_dev;
    int min_length;
    int max_length;
    // Boundary metrics
    uint64_t count_at_10;
    uint64_t count_at_100;
    double expected_at_10;
    double expected_at_100;
    double delta_at_10;
    double delta_at_100;
    double delta_pct_at_10;
    double delta_pct_at_100;
} FlushResult;

// Calculate statistics
void calculate_stats(FlushResult* result) {
    uint64_t total = result->seeds_accepted;
    
    // Mean
    double sum = 0;
    for (int i = 0; i <= 256; i++) {
        sum += i * result->length_counts[i];
    }
    result->mean_length = sum / total;
    
    // Median
    uint64_t cumsum = 0;
    for (int i = 0; i <= 256; i++) {
        cumsum += result->length_counts[i];
        if (cumsum >= total / 2) {
            result->median_length = i;
            break;
        }
    }
    
    // Std dev
    double var_sum = 0;
    for (int i = 0; i <= 256; i++) {
        if (result->length_counts[i] > 0) {
            double diff = i - result->mean_length;
            var_sum += diff * diff * result->length_counts[i];
        }
    }
    result->std_dev = sqrt(var_sum / total);
    
    // Min/Max
    result->min_length = 256;
    result->max_length = 0;
    for (int i = 0; i <= 256; i++) {
        if (result->length_counts[i] > 0) {
            if (i < result->min_length) result->min_length = i;
            if (i > result->max_length) result->max_length = i;
        }
    }
    
    // Boundary analysis
    result->count_at_10 = result->length_counts[10];
    result->count_at_100 = result->length_counts[100];
    
    // Expected = average of neighbors
    if (result->count_at_10 > 0) {
        result->expected_at_10 = (result->length_counts[9] + result->length_counts[11]) / 2.0;
        result->delta_at_10 = result->count_at_10 - result->expected_at_10;
        result->delta_pct_at_10 = (result->delta_at_10 / result->expected_at_10) * 100.0;
    }
    
    if (result->count_at_100 > 0) {
        result->expected_at_100 = (result->length_counts[99] + result->length_counts[101]) / 2.0;
        result->delta_at_100 = result->count_at_100 - result->expected_at_100;
        result->delta_pct_at_100 = (result->delta_at_100 / result->expected_at_100) * 100.0;
    }
}

int main() {
    printf("NPES_256_X - Time-Based Sweep Test (Base %d)\n", BASE);
    printf("================================================\n");
    printf("Threads: %d\n", NUM_THREADS);
    printf("Duration per flush rate: %d seconds\n", TEST_DURATION_SEC);
    printf("Flush rates: 11 to 111 (odd numbers only)\n\n");
    
    // Results array
    FlushResult results[51]; // (111-11)/2 + 1 = 51
    int result_count = 0;
    
    // Thread data
    ThreadData thread_data[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    volatile int control[2]; // [0] = keep_running, [1] = flush_rate
    
    // Initialize threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
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
        
        // Combine results
        FlushResult* res = &results[result_count++];
        res->flush_rate = flush_rate;
        res->elapsed_time = elapsed;
        res->seeds_generated = 0;
        res->seeds_accepted = 0;
        memset(res->length_counts, 0, sizeof(res->length_counts));
        
        for (int i = 0; i < NUM_THREADS; i++) {
            res->seeds_generated += thread_data[i].seeds_generated;
            res->seeds_accepted += thread_data[i].seeds_accepted;
            for (int j = 0; j <= 256; j++) {
                res->length_counts[j] += thread_data[i].length_counts[j];
            }
        }
        
        res->seeds_per_sec_accepted = res->seeds_accepted / elapsed;
        res->seeds_per_sec_total = res->seeds_generated / elapsed;
        res->acceptance_rate = (res->seeds_accepted / (double)res->seeds_generated) * 100.0;
        
        calculate_stats(res);
        
        // Console output
        printf("Elapsed: %.2f sec\n", elapsed);
        printf("Seeds generated: %llu\n", (unsigned long long)res->seeds_generated);
        printf("Seeds accepted: %llu\n", (unsigned long long)res->seeds_accepted);
        printf("Seeds/sec (accepted): %.2f million\n", res->seeds_per_sec_accepted / 1000000.0);
        printf("Seeds/sec (total): %.2f million\n", res->seeds_per_sec_total / 1000000.0);
        printf("Acceptance rate: %.2f%%\n", res->acceptance_rate);
        printf("Mean length: %.2f\n", res->mean_length);
        printf("Std dev: %.2f\n", res->std_dev);
        printf("Range: %d - %d\n", res->min_length, res->max_length);
        printf("Boundary @10: actual=%llu, expected=%.2f, delta=%.2f (%.4f%%)\n",
               (unsigned long long)res->count_at_10, res->expected_at_10, 
               res->delta_at_10, res->delta_pct_at_10);
        printf("Boundary @100: actual=%llu, expected=%.2f, delta=%.2f (%.4f%%)\n",
               (unsigned long long)res->count_at_100, res->expected_at_100,
               res->delta_at_100, res->delta_pct_at_100);
    }
    
    // Write JSON
    printf("\n\nWriting JSON output...\n");
    FILE* fp = fopen("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "w");     //<---output path

    if (!fp) {
        printf("ERROR: Could not open JSON file for writing!\n");
        return 1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"test_name\": \"NPES_256_X\",\n");
    fprintf(fp, "  \"base\": %d,\n", BASE);
    fprintf(fp, "  \"threads\": %d,\n", NUM_THREADS);
    fprintf(fp, "  \"duration_per_flush\": %d,\n", TEST_DURATION_SEC);
    fprintf(fp, "  \"results\": [\n");
    
    for (int r = 0; r < result_count; r++) {
        FlushResult* res = &results[r];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"flush_rate\": %d,\n", res->flush_rate);
        fprintf(fp, "      \"elapsed_time\": %.2f,\n", res->elapsed_time);
        fprintf(fp, "      \"seeds_generated\": %llu,\n", (unsigned long long)res->seeds_generated);
        fprintf(fp, "      \"seeds_accepted\": %llu,\n", (unsigned long long)res->seeds_accepted);
        fprintf(fp, "      \"seeds_per_sec_accepted\": %.2f,\n", res->seeds_per_sec_accepted);
        fprintf(fp, "      \"seeds_per_sec_total\": %.2f,\n", res->seeds_per_sec_total);
        fprintf(fp, "      \"acceptance_rate\": %.2f,\n", res->acceptance_rate);
        fprintf(fp, "      \"mean_length\": %.2f,\n", res->mean_length);
        fprintf(fp, "      \"median_length\": %.0f,\n", res->median_length);
        fprintf(fp, "      \"std_dev\": %.2f,\n", res->std_dev);
        fprintf(fp, "      \"min_length\": %d,\n", res->min_length);
        fprintf(fp, "      \"max_length\": %d,\n", res->max_length);
        fprintf(fp, "      \"boundary_10\": {\n");
        fprintf(fp, "        \"actual\": %llu,\n", (unsigned long long)res->count_at_10);
        fprintf(fp, "        \"expected\": %.2f,\n", res->expected_at_10);
        fprintf(fp, "        \"delta\": %.2f,\n", res->delta_at_10);
        fprintf(fp, "        \"delta_pct\": %.4f\n", res->delta_pct_at_10);
        fprintf(fp, "      },\n");
        fprintf(fp, "      \"boundary_100\": {\n");
        fprintf(fp, "        \"actual\": %llu,\n", (unsigned long long)res->count_at_100);
        fprintf(fp, "        \"expected\": %.2f,\n", res->expected_at_100);
        fprintf(fp, "        \"delta\": %.2f,\n", res->delta_at_100);
        fprintf(fp, "        \"delta_pct\": %.4f\n", res->delta_pct_at_100);
        fprintf(fp, "      },\n");
        fprintf(fp, "      \"length_distribution\": [");
        for (int i = 0; i <= 256; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n        ");
            fprintf(fp, "%llu", (unsigned long long)res->length_counts[i]);
        }
        fprintf(fp, "\n      ]\n");
        fprintf(fp, "    }%s\n", (r < result_count - 1) ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    fclose(fp);
    
    printf("JSON written to: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");     //<---output path

    printf("\nTEST COMPLETE!\n");
    
    return 0;
}