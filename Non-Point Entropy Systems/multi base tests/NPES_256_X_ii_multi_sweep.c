// NPES_256_X_ii.c
// Non-Point Entropy System - 256 bit range - X-ii
// Base 10 (Thread 0) + Base -2 (Thread 1)
// 6-Sweep Directional Coupling Test - SEALED EXECUTION
// Author: Nicholas H. Valentine (eNVy) — 2025
// Compile: gcc -O3 -static -pthread

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#define MAX_ROLLS 256
#define NUM_THREADS 2
#define TEST_DURATION_SEC 10
#define LENGTH_RANGE 513

uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

typedef struct {
    int thread_id;
    int base;
    uint64_t rng_state;
    uint64_t length_counts[LENGTH_RANGE];
    uint64_t seeds_generated;
    uint64_t seeds_accepted;
    volatile int* keep_running;
} ThreadData;

int generate_seed(uint64_t* rng_state, int flush_rate, int base) {
    int digit_count = 0;
    int abs_base = (base < 0) ? -base : base;
    
    for (int slot = 0; slot < MAX_ROLLS; slot++) {
        int roll = splitmix64(rng_state) % flush_rate;
        if (roll >= abs_base) continue;
        digit_count++;
    }
    
    if (base < 0) {
        int sign_flip = splitmix64(rng_state) & 1;
        if (sign_flip) digit_count = -digit_count;
    }
    
    return digit_count;
}

void* thread_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    while (*(data->keep_running)) {
        int flush_rate = *(data->keep_running + 1 + data->thread_id);
        int length = generate_seed(&data->rng_state, flush_rate, data->base);
        data->seeds_generated++;
        data->seeds_accepted++;
        data->length_counts[length + 256]++;
    }
    return NULL;
}

typedef struct {
    int sweep_number;
    char sweep_type[64];
    int step_in_sweep;
    int b10_flush_rate;
    int bneg2_flush_rate;
    double elapsed_time;
    uint64_t b10_seeds_generated, b10_seeds_accepted;
    double b10_seeds_per_sec;
    uint64_t b10_length_counts[LENGTH_RANGE];
    double b10_mean_length, b10_std_dev;
    int b10_min_length, b10_max_length;
    uint64_t b10_count_at_10, b10_count_at_100;
    double b10_expected_at_10, b10_expected_at_100;
    double b10_delta_at_10, b10_delta_at_100;
    double b10_delta_pct_at_10, b10_delta_pct_at_100;
    uint64_t bneg2_seeds_generated, bneg2_seeds_accepted;
    double bneg2_seeds_per_sec;
    uint64_t bneg2_length_counts[LENGTH_RANGE];
    double bneg2_mean_length, bneg2_std_dev;
    int bneg2_min_length, bneg2_max_length;
    uint64_t bneg2_count_at_neg128, bneg2_count_at_neg32, bneg2_count_at_neg8, bneg2_count_at_neg2;
    uint64_t bneg2_count_at_2, bneg2_count_at_4, bneg2_count_at_8, bneg2_count_at_16;
    uint64_t bneg2_count_at_32, bneg2_count_at_64, bneg2_count_at_128;
    double bneg2_expected_at_neg128, bneg2_expected_at_neg32, bneg2_expected_at_neg8, bneg2_expected_at_neg2;
    double bneg2_expected_at_2, bneg2_expected_at_4, bneg2_expected_at_8, bneg2_expected_at_16;
    double bneg2_expected_at_32, bneg2_expected_at_64, bneg2_expected_at_128;
    double bneg2_delta_at_neg128, bneg2_delta_at_neg32, bneg2_delta_at_neg8, bneg2_delta_at_neg2;
    double bneg2_delta_at_2, bneg2_delta_at_4, bneg2_delta_at_8, bneg2_delta_at_16;
    double bneg2_delta_at_32, bneg2_delta_at_64, bneg2_delta_at_128;
    double bneg2_delta_pct_at_neg128, bneg2_delta_pct_at_neg32, bneg2_delta_pct_at_neg8, bneg2_delta_pct_at_neg2;
    double bneg2_delta_pct_at_2, bneg2_delta_pct_at_4, bneg2_delta_pct_at_8, bneg2_delta_pct_at_16;
    double bneg2_delta_pct_at_32, bneg2_delta_pct_at_64, bneg2_delta_pct_at_128;
    uint64_t combined_seeds_generated, combined_seeds_accepted;
    double combined_seeds_per_sec;
    uint64_t combined_length_counts[LENGTH_RANGE];
    double combined_mean_length, combined_std_dev;
    int combined_min_length, combined_max_length;
} SweepResult;

void calculate_base_stats(uint64_t* lc, uint64_t total, double* mean, double* std_dev, 
                          int* min_len, int* max_len, int b1, int b2,
                          uint64_t* cb1, uint64_t* cb2, double* eb1, double* eb2,
                          double* db1, double* db2, double* dpb1, double* dpb2) {
    int64_t sum = 0;
    for (int i = -256; i <= 256; i++) sum += (int64_t)i * (int64_t)lc[i + 256];
    *mean = (double)sum / (double)total;
    int64_t mean_scaled = (sum * 1000) / (int64_t)total;
    int64_t var_sum = 0;
    for (int i = -256; i <= 256; i++) {
        if (lc[i + 256] > 0) {
            int64_t i_scaled = (int64_t)i * 1000;
            int64_t diff = i_scaled - mean_scaled;
            var_sum += ((diff * diff) / 1000) * (int64_t)lc[i + 256];
        }
    }
    *std_dev = sqrt((double)var_sum / ((double)total * 1000.0));
    *min_len = 256; *max_len = -256;
    for (int i = -256; i <= 256; i++) {
        if (lc[i + 256] > 0) {
            if (i < *min_len) *min_len = i;
            if (i > *max_len) *max_len = i;
        }
    }
    *cb1 = lc[b1 + 256];
    if (*cb1 > 0 && b1 > -256 && b1 < 256) {
        if (lc[b1-1+256] > 0 && lc[b1+1+256] > 0) {
            *eb1 = (lc[b1-1+256] + lc[b1+1+256]) / 2.0;
            *db1 = *cb1 - *eb1;
            *dpb1 = (*db1 / *eb1) * 100.0;
        }
    }
    *cb2 = lc[b2 + 256];
    if (*cb2 > 0 && b2 > -256 && b2 < 256) {
        if (lc[b2-1+256] > 0 && lc[b2+1+256] > 0) {
            *eb2 = (lc[b2-1+256] + lc[b2+1+256]) / 2.0;
            *db2 = *cb2 - *eb2;
            *dpb2 = (*db2 / *eb2) * 100.0;
        }
    }
}

void calculate_base_neg2_stats(uint64_t* lc, uint64_t total, double* mean, double* std_dev,
                               int* min_len, int* max_len,
                               uint64_t* cn128, uint64_t* cn32, uint64_t* cn8, uint64_t* cn2,
                               uint64_t* c2, uint64_t* c4, uint64_t* c8, uint64_t* c16,
                               uint64_t* c32, uint64_t* c64, uint64_t* c128,
                               double* en128, double* en32, double* en8, double* en2,
                               double* e2, double* e4, double* e8, double* e16,
                               double* e32, double* e64, double* e128,
                               double* dn128, double* dn32, double* dn8, double* dn2,
                               double* d2, double* d4, double* d8, double* d16,
                               double* d32, double* d64, double* d128,
                               double* dpn128, double* dpn32, double* dpn8, double* dpn2,
                               double* dp2, double* dp4, double* dp8, double* dp16,
                               double* dp32, double* dp64, double* dp128) {
    int64_t sum = 0;
    for (int i = -256; i <= 256; i++) sum += (int64_t)i * (int64_t)lc[i + 256];
    *mean = (double)sum / (double)total;
    int64_t mean_scaled = (sum * 1000) / (int64_t)total;
    int64_t var_sum = 0;
    for (int i = -256; i <= 256; i++) {
        if (lc[i + 256] > 0) {
            int64_t i_scaled = (int64_t)i * 1000;
            int64_t diff = i_scaled - mean_scaled;
            var_sum += ((diff * diff) / 1000) * (int64_t)lc[i + 256];
        }
    }
    *std_dev = sqrt((double)var_sum / ((double)total * 1000.0));
    *min_len = 256; *max_len = -256;
    for (int i = -256; i <= 256; i++) {
        if (lc[i + 256] > 0) {
            if (i < *min_len) *min_len = i;
            if (i > *max_len) *max_len = i;
        }
    }
    #define CB(v,c,e,d,dp) *c=lc[(v)+256]; if(*c>0&&(v)>-256&&(v)<256) { \
        if(lc[(v)-1+256]>0&&lc[(v)+1+256]>0) { \
            *e=(lc[(v)-1+256]+lc[(v)+1+256])/2.0; *d=*c-*e; *dp=(*d/ *e)*100.0; }}
    CB(-128,cn128,en128,dn128,dpn128); CB(-32,cn32,en32,dn32,dpn32);
    CB(-8,cn8,en8,dn8,dpn8); CB(-2,cn2,en2,dn2,dpn2);
    CB(2,c2,e2,d2,dp2); CB(4,c4,e4,d4,dp4);
    CB(8,c8,e8,d8,dp8); CB(16,c16,e16,d16,dp16);
    CB(32,c32,e32,d32,dp32); CB(64,c64,e64,d64,dp64); CB(128,c128,e128,d128,dp128);
    #undef CB
}

void run_sample(ThreadData* td, pthread_t* threads, volatile int* control,
                int b10_flush, int bneg2_flush, SweepResult* res,
                int sweep_num, const char* sweep_type, int step) {
    for (int i = 0; i < NUM_THREADS; i++) {
        td[i].seeds_generated = 0;
        td[i].seeds_accepted = 0;
        memset((void*)td[i].length_counts, 0, sizeof(td[i].length_counts));
    }
    control[0] = 1;
    control[1] = b10_flush;
    control[2] = bneg2_flush;
    time_t start = time(NULL);
    for (int i = 0; i < NUM_THREADS; i++) pthread_create(&threads[i], NULL, thread_worker, &td[i]);
    time_t now;
    do { now = time(NULL); } while (difftime(now, start) < TEST_DURATION_SEC);
    control[0] = 0;
    for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);
    double elapsed = difftime(time(NULL), start);
    
    memset(res, 0, sizeof(SweepResult));
    res->sweep_number = sweep_num;
    strcpy(res->sweep_type, sweep_type);
    res->step_in_sweep = step;
    res->b10_flush_rate = b10_flush;
    res->bneg2_flush_rate = bneg2_flush;
    res->elapsed_time = elapsed;
    res->b10_seeds_generated = td[0].seeds_generated;
    res->b10_seeds_accepted = td[0].seeds_accepted;
    res->b10_seeds_per_sec = res->b10_seeds_accepted / elapsed;
    memcpy(res->b10_length_counts, td[0].length_counts, sizeof(res->b10_length_counts));
    res->bneg2_seeds_generated = td[1].seeds_generated;
    res->bneg2_seeds_accepted = td[1].seeds_accepted;
    res->bneg2_seeds_per_sec = res->bneg2_seeds_accepted / elapsed;
    memcpy(res->bneg2_length_counts, td[1].length_counts, sizeof(res->bneg2_length_counts));
    res->combined_seeds_generated = res->b10_seeds_generated + res->bneg2_seeds_generated;
    res->combined_seeds_accepted = res->b10_seeds_accepted + res->bneg2_seeds_accepted;
    res->combined_seeds_per_sec = res->combined_seeds_accepted / elapsed;
    for (int j = 0; j < LENGTH_RANGE; j++)
        res->combined_length_counts[j] = res->b10_length_counts[j] + res->bneg2_length_counts[j];
    
    calculate_base_stats(res->b10_length_counts, res->b10_seeds_accepted,
                       &res->b10_mean_length, &res->b10_std_dev,
                       &res->b10_min_length, &res->b10_max_length, 10, 100,
                       &res->b10_count_at_10, &res->b10_count_at_100,
                       &res->b10_expected_at_10, &res->b10_expected_at_100,
                       &res->b10_delta_at_10, &res->b10_delta_at_100,
                       &res->b10_delta_pct_at_10, &res->b10_delta_pct_at_100);
    
    calculate_base_neg2_stats(res->bneg2_length_counts, res->bneg2_seeds_accepted,
                             &res->bneg2_mean_length, &res->bneg2_std_dev,
                             &res->bneg2_min_length, &res->bneg2_max_length,
                             &res->bneg2_count_at_neg128, &res->bneg2_count_at_neg32,
                             &res->bneg2_count_at_neg8, &res->bneg2_count_at_neg2,
                             &res->bneg2_count_at_2, &res->bneg2_count_at_4,
                             &res->bneg2_count_at_8, &res->bneg2_count_at_16,
                             &res->bneg2_count_at_32, &res->bneg2_count_at_64,
                             &res->bneg2_count_at_128,
                             &res->bneg2_expected_at_neg128, &res->bneg2_expected_at_neg32,
                             &res->bneg2_expected_at_neg8, &res->bneg2_expected_at_neg2,
                             &res->bneg2_expected_at_2, &res->bneg2_expected_at_4,
                             &res->bneg2_expected_at_8, &res->bneg2_expected_at_16,
                             &res->bneg2_expected_at_32, &res->bneg2_expected_at_64,
                             &res->bneg2_expected_at_128,
                             &res->bneg2_delta_at_neg128, &res->bneg2_delta_at_neg32,
                             &res->bneg2_delta_at_neg8, &res->bneg2_delta_at_neg2,
                             &res->bneg2_delta_at_2, &res->bneg2_delta_at_4,
                             &res->bneg2_delta_at_8, &res->bneg2_delta_at_16,
                             &res->bneg2_delta_at_32, &res->bneg2_delta_at_64,
                             &res->bneg2_delta_at_128,
                             &res->bneg2_delta_pct_at_neg128, &res->bneg2_delta_pct_at_neg32,
                             &res->bneg2_delta_pct_at_neg8, &res->bneg2_delta_pct_at_neg2,
                             &res->bneg2_delta_pct_at_2, &res->bneg2_delta_pct_at_4,
                             &res->bneg2_delta_pct_at_8, &res->bneg2_delta_pct_at_16,
                             &res->bneg2_delta_pct_at_32, &res->bneg2_delta_pct_at_64,
                             &res->bneg2_delta_pct_at_128);
    
    double pe, pd, pdp;
    uint64_t pc;
    calculate_base_stats(res->combined_length_counts, res->combined_seeds_accepted,
                       &res->combined_mean_length, &res->combined_std_dev,
                       &res->combined_min_length, &res->combined_max_length,
                       0, 0, &pc, &pc, &pe, &pe, &pd, &pd, &pdp, &pdp);
}

int main() {
    SweepResult* results = (SweepResult*)malloc(303 * sizeof(SweepResult));
    if (!results) {
    printf("Memory allocation failed!\n");
    return 1;
}
    int rc = 0;
    ThreadData td[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    volatile int control[3];
    
    td[0].thread_id = 0; td[0].base = 10;
    td[0].rng_state = (uint64_t)time(NULL);
    td[0].keep_running = (volatile int*)control;
    td[1].thread_id = 1; td[1].base = -2;
    td[1].rng_state = (uint64_t)time(NULL) + 0x123456789ABCDEFULL;
    td[1].keep_running = (volatile int*)control;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        td[i].seeds_generated = 0;
        td[i].seeds_accepted = 0;
        memset((void*)td[i].length_counts, 0, sizeof(td[i].length_counts));
    }
    
    // SWEEP 1: Both ascending
    for (int s = 0; s < 51; s++)
        run_sample(td, threads, control, 11+s*2, 3+s*2, &results[rc++], 1, "both_ascending", s+1);
    
    // SWEEP 2: Both descending
    for (int s = 0; s < 50; s++)
        run_sample(td, threads, control, 109-s*2, 101-s*2, &results[rc++], 2, "both_descending", s+1);
    
    // SWEEP 3: B10 ascending, B-2 fixed@3
    for (int s = 0; s < 51; s++)
        run_sample(td, threads, control, 11+s*2, 3, &results[rc++], 3, "b10_ascending_bneg2_fixed", s+1);
    
    // SWEEP 4: B10 descending, B-2 fixed@3
    for (int s = 0; s < 50; s++)
        run_sample(td, threads, control, 109-s*2, 3, &results[rc++], 4, "b10_descending_bneg2_fixed", s+1);
    
    // SWEEP 5: B-2 ascending, B10 fixed@11
    for (int s = 0; s < 51; s++)
        run_sample(td, threads, control, 11, 3+s*2, &results[rc++], 5, "bneg2_ascending_b10_fixed", s+1);
    
    // SWEEP 6: B-2 descending, B10 fixed@11
    for (int s = 0; s < 50; s++)
        run_sample(td, threads, control, 11, 101-s*2, &results[rc++], 6, "bneg2_descending_b10_fixed", s+1);
    
    // RESULTS
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  NPES_256_X_ii\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Total samples: %d\n", rc);
    printf("Base 10 (Thread 0) + Base -2 (Thread 1)\n");
    printf("Directional Coupling Test - 6 Sweeps Complete\n\n");
    
    uint64_t total_seeds = 0;
    for (int i = 0; i < rc; i++) total_seeds += results[i].combined_seeds_accepted;
    printf("Total seeds generated: %llu\n", (unsigned long long)total_seeds);
    printf("Average seeds per sample: %.2f million\n", (double)total_seeds / rc / 1000000.0);
    
    printf("\nWriting JSON output...\n");
    FILE* fp = fopen("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "w");     // <----- output path
    if (!fp) {
        printf("ERROR: Could not open JSON file!\n");
        return 1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"test_name\": \"NPES_256_X_ii\",\n");
    fprintf(fp, "  \"full_name\": \"Non-Point Entropy System 256 - X-ii\",\n");
    fprintf(fp, "  \"base_thread0\": 10,\n");
    fprintf(fp, "  \"base_thread1\": -2,\n");
    fprintf(fp, "  \"threads\": %d,\n", NUM_THREADS);
    fprintf(fp, "  \"duration_per_sample\": %d,\n", TEST_DURATION_SEC);
    fprintf(fp, "  \"total_samples\": %d,\n", rc);
    fprintf(fp, "  \"total_seeds\": %llu,\n", (unsigned long long)total_seeds);
    fprintf(fp, "  \"results\": [\n");
    
    for (int r = 0; r < rc; r++) {
        SweepResult* res = &results[r];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"sweep_number\": %d,\n", res->sweep_number);
        fprintf(fp, "      \"sweep_type\": \"%s\",\n", res->sweep_type);
        fprintf(fp, "      \"step_in_sweep\": %d,\n", res->step_in_sweep);
        fprintf(fp, "      \"b10_flush_rate\": %d,\n", res->b10_flush_rate);
        fprintf(fp, "      \"bneg2_flush_rate\": %d,\n", res->bneg2_flush_rate);
        fprintf(fp, "      \"elapsed_time\": %.2f,\n", res->elapsed_time);
        
        // Base 10
        fprintf(fp, "      \"base10\": {\n");
        fprintf(fp, "        \"seeds_generated\": %llu,\n", (unsigned long long)res->b10_seeds_generated);
        fprintf(fp, "        \"seeds_accepted\": %llu,\n", (unsigned long long)res->b10_seeds_accepted);
        fprintf(fp, "        \"seeds_per_sec\": %.2f,\n", res->b10_seeds_per_sec);
        fprintf(fp, "        \"mean_length\": %.2f,\n", res->b10_mean_length);
        fprintf(fp, "        \"std_dev\": %.2f,\n", res->b10_std_dev);
        fprintf(fp, "        \"min_length\": %d,\n", res->b10_min_length);
        fprintf(fp, "        \"max_length\": %d,\n", res->b10_max_length);
        fprintf(fp, "        \"boundary_10\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->b10_count_at_10, res->b10_expected_at_10, res->b10_delta_at_10, res->b10_delta_pct_at_10);
        fprintf(fp, "        \"boundary_100\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->b10_count_at_100, res->b10_expected_at_100, res->b10_delta_at_100, res->b10_delta_pct_at_100);
        fprintf(fp, "        \"length_distribution\": [");
        for (int i = 0; i < LENGTH_RANGE; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->b10_length_counts[i]);
        }
        fprintf(fp, "\n        ]\n      },\n");
        
        // Base -2
        fprintf(fp, "      \"base_neg2\": {\n");
        fprintf(fp, "        \"seeds_generated\": %llu,\n", (unsigned long long)res->bneg2_seeds_generated);
        fprintf(fp, "        \"seeds_accepted\": %llu,\n", (unsigned long long)res->bneg2_seeds_accepted);
        fprintf(fp, "        \"seeds_per_sec\": %.2f,\n", res->bneg2_seeds_per_sec);
        fprintf(fp, "        \"mean_length\": %.2f,\n", res->bneg2_mean_length);
        fprintf(fp, "        \"std_dev\": %.2f,\n", res->bneg2_std_dev);
        fprintf(fp, "        \"min_length\": %d,\n", res->bneg2_min_length);
        fprintf(fp, "        \"max_length\": %d,\n", res->bneg2_max_length);
        fprintf(fp, "        \"boundary_neg128\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_neg128, res->bneg2_expected_at_neg128, res->bneg2_delta_at_neg128, res->bneg2_delta_pct_at_neg128);
        fprintf(fp, "        \"boundary_neg32\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_neg32, res->bneg2_expected_at_neg32, res->bneg2_delta_at_neg32, res->bneg2_delta_pct_at_neg32);
        fprintf(fp, "        \"boundary_neg8\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_neg8, res->bneg2_expected_at_neg8, res->bneg2_delta_at_neg8, res->bneg2_delta_pct_at_neg8);
        fprintf(fp, "        \"boundary_neg2\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_neg2, res->bneg2_expected_at_neg2, res->bneg2_delta_at_neg2, res->bneg2_delta_pct_at_neg2);
        fprintf(fp, "        \"boundary_2\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_2, res->bneg2_expected_at_2, res->bneg2_delta_at_2, res->bneg2_delta_pct_at_2);
        fprintf(fp, "        \"boundary_4\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_4, res->bneg2_expected_at_4, res->bneg2_delta_at_4, res->bneg2_delta_pct_at_4);
        fprintf(fp, "        \"boundary_8\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_8, res->bneg2_expected_at_8, res->bneg2_delta_at_8, res->bneg2_delta_pct_at_8);
        fprintf(fp, "        \"boundary_16\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_16, res->bneg2_expected_at_16, res->bneg2_delta_at_16, res->bneg2_delta_pct_at_16);
        fprintf(fp, "        \"boundary_32\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_32, res->bneg2_expected_at_32, res->bneg2_delta_at_32, res->bneg2_delta_pct_at_32);
        fprintf(fp, "        \"boundary_64\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_64, res->bneg2_expected_at_64, res->bneg2_delta_at_64, res->bneg2_delta_pct_at_64);
        fprintf(fp, "        \"boundary_128\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->bneg2_count_at_128, res->bneg2_expected_at_128, res->bneg2_delta_at_128, res->bneg2_delta_pct_at_128);
        fprintf(fp, "        \"length_distribution\": [");
        for (int i = 0; i < LENGTH_RANGE; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->bneg2_length_counts[i]);
        }
        fprintf(fp, "\n        ]\n      },\n");
        
        // Combined
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
        fprintf(fp, "\n        ]\n      }\n");
        fprintf(fp, "    }%s\n", (r < rc - 1) ? "," : "");
    }
    
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    
    printf("JSON written to: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");     // <----- output path
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  NPES_256_X_ii COMPLETE\n");
    
    free(results);
    return 0;
}