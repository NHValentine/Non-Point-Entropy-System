// NPES_2048_x.C
// Non-Point Entropy System
// Base -10 (ALL 32 threads)
// Author: Nicholas H. Valentine (eNVy) — 2026
// Compile: gcc -O3 -static -pthread

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#define MAX_ROLLS 2048
#define NUM_THREADS 32
#define TEST_DURATION_SEC 10
#define LENGTH_RANGE 4097  // -2048 to +2048

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
    uint64_t* length_counts;  // Heap allocated - too big for stack
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
        int flush_rate = *(data->keep_running + 1);
        int length = generate_seed(&data->rng_state, flush_rate, data->base);
        data->seeds_generated++;
        data->seeds_accepted++;
        data->length_counts[length + 2048]++;
    }
    return NULL;
}

typedef struct {
    int flush_rate;
    double elapsed_time;
    uint64_t seeds_generated;
    uint64_t seeds_accepted;
    double seeds_per_sec;
    uint64_t* length_counts;  // Heap allocated
    double mean_length;
    double std_dev;
    int min_length;
    int max_length;
    // Boundaries: -1000, -100, 100, 1000
    uint64_t count_at_neg1000;
    uint64_t count_at_neg100;
    uint64_t count_at_100;
    uint64_t count_at_1000;
    double expected_at_neg1000;
    double expected_at_neg100;
    double expected_at_100;
    double expected_at_1000;
    double delta_at_neg1000;
    double delta_at_neg100;
    double delta_at_100;
    double delta_at_1000;
    double delta_pct_at_neg1000;
    double delta_pct_at_neg100;
    double delta_pct_at_100;
    double delta_pct_at_1000;
} FlushResult;

void calculate_stats(uint64_t* lc, uint64_t total, double* mean, double* std_dev,
                    int* min_len, int* max_len,
                    uint64_t* cn1000, uint64_t* cn100, uint64_t* c100, uint64_t* c1000,
                    double* en1000, double* en100, double* e100, double* e1000,
                    double* dn1000, double* dn100, double* d100, double* d1000,
                    double* dpn1000, double* dpn100, double* dp100, double* dp1000) {
    // Mean (INTEGER ONLY)
    int64_t sum = 0;
    for (int i = -2048; i <= 2048; i++) sum += (int64_t)i * (int64_t)lc[i + 2048];
    *mean = (double)sum / (double)total;
    
    // Std dev
    int64_t mean_scaled = (sum * 1000) / (int64_t)total;
    int64_t var_sum = 0;
    for (int i = -2048; i <= 2048; i++) {
        if (lc[i + 2048] > 0) {
            int64_t i_scaled = (int64_t)i * 1000;
            int64_t diff = i_scaled - mean_scaled;
            var_sum += ((diff * diff) / 1000) * (int64_t)lc[i + 2048];
        }
    }
    *std_dev = sqrt((double)var_sum / ((double)total * 1000.0));
    
    // Min/Max
    *min_len = 2048;
    *max_len = -2048;
    for (int i = -2048; i <= 2048; i++) {
        if (lc[i + 2048] > 0) {
            if (i < *min_len) *min_len = i;
            if (i > *max_len) *max_len = i;
        }
    }
    
    // Boundary calculations
    #define CB(v,c,e,d,dp) *c=lc[(v)+2048]; if(*c>0&&(v)>-2048&&(v)<2048) { \
        if(lc[(v)-1+2048]>0&&lc[(v)+1+2048]>0) { \
            *e=(lc[(v)-1+2048]+lc[(v)+1+2048])/2.0; *d=*c-*e; *dp=(*d/ *e)*100.0; }}
    
    CB(-1000, cn1000, en1000, dn1000, dpn1000);
    CB(-100, cn100, en100, dn100, dpn100);
    CB(100, c100, e100, d100, dp100);
    CB(1000, c1000, e1000, d1000, dp1000);
    
    #undef CB
}

int main() {
    // Results array - HEAP ALLOCATED
    FlushResult* results = (FlushResult*)malloc(78 * sizeof(FlushResult));
    if (!results) {
        printf("Failed to allocate results array!\n");
        return 1;
    }
    
    int rc = 0;
    ThreadData td[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    volatile int control[2];
    
    // Initialize threads - ALL base -10
    for (int i = 0; i < NUM_THREADS; i++) {
        td[i].thread_id = i;
        td[i].base = -10;
        td[i].rng_state = (uint64_t)time(NULL) + i * 0x123456789ABCDEFULL;
        td[i].keep_running = (volatile int*)control;
        td[i].seeds_generated = 0;
        td[i].seeds_accepted = 0;
        td[i].length_counts = (uint64_t*)calloc(LENGTH_RANGE, sizeof(uint64_t));
        if (!td[i].length_counts) {
            printf("Failed to allocate length_counts for thread %d!\n", i);
            return 1;
        }
    }
    
    // Sweep flush rates 11 to 165 (odds only, 78 steps)
    for (int flush_rate = 11; flush_rate <= 165; flush_rate += 2) {
        // Reset counts
        for (int i = 0; i < NUM_THREADS; i++) {
            td[i].seeds_generated = 0;
            td[i].seeds_accepted = 0;
            memset(td[i].length_counts, 0, LENGTH_RANGE * sizeof(uint64_t));
        }
        
        control[0] = 1;
        control[1] = flush_rate;
        
        time_t start = time(NULL);
        for (int i = 0; i < NUM_THREADS; i++) pthread_create(&threads[i], NULL, thread_worker, &td[i]);
        
        time_t now;
        do { now = time(NULL); } while (difftime(now, start) < TEST_DURATION_SEC);
        
        control[0] = 0;
        for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);
        
        double elapsed = difftime(time(NULL), start);
        
        // Aggregate results
        FlushResult* res = &results[rc++];
        memset(res, 0, sizeof(FlushResult));
        res->flush_rate = flush_rate;
        res->elapsed_time = elapsed;
        res->length_counts = (uint64_t*)calloc(LENGTH_RANGE, sizeof(uint64_t));
        if (!res->length_counts) {
            printf("Failed to allocate result length_counts!\n");
            return 1;
        }
        
        for (int i = 0; i < NUM_THREADS; i++) {
            res->seeds_generated += td[i].seeds_generated;
            res->seeds_accepted += td[i].seeds_accepted;
            for (int j = 0; j < LENGTH_RANGE; j++) {
                res->length_counts[j] += td[i].length_counts[j];
            }
        }
        
        res->seeds_per_sec = res->seeds_accepted / elapsed;
        
        calculate_stats(res->length_counts, res->seeds_accepted,
                       &res->mean_length, &res->std_dev,
                       &res->min_length, &res->max_length,
                       &res->count_at_neg1000, &res->count_at_neg100,
                       &res->count_at_100, &res->count_at_1000,
                       &res->expected_at_neg1000, &res->expected_at_neg100,
                       &res->expected_at_100, &res->expected_at_1000,
                       &res->delta_at_neg1000, &res->delta_at_neg100,
                       &res->delta_at_100, &res->delta_at_1000,
                       &res->delta_pct_at_neg1000, &res->delta_pct_at_neg100,
                       &res->delta_pct_at_100, &res->delta_pct_at_1000);
    }
    
    printf("\n");
    printf("===================================================================\n");
    printf("  NPES_2048_x - MORE BITS!\n");
    printf("===================================================================\n");
    printf("Total samples: %d\n", rc);
    printf("Base -10 (ALL 32 threads)\n");
    printf("Range: -2048 to +2048\n");
    printf("Flush rates: 11 to 165 (odds)\n\n");
    
    uint64_t total_seeds = 0;
    for (int i = 0; i < rc; i++) total_seeds += results[i].seeds_accepted;
    printf("Total seeds generated: %llu\n", (unsigned long long)total_seeds);
    printf("Average seeds per sample: %.2f million\n", (double)total_seeds / rc / 1000000.0);
    
    printf("\nWriting JSON output...\n");
    FILE* fp = fopen("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "w");    // <----- output path
    if (!fp) {
        printf("ERROR: Could not open JSON file!\n");
        return 1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"test_name\": \"NPES_2048_MK_neg_X\",\n");
    fprintf(fp, "  \"full_name\": \"Non-Point Entropy System 2048 - Mark neg-X\",\n");
    fprintf(fp, "  \"base\": -10,\n");
    fprintf(fp, "  \"threads\": %d,\n", NUM_THREADS);
    fprintf(fp, "  \"max_rolls\": %d,\n", MAX_ROLLS);
    fprintf(fp, "  \"range\": \"-2048 to +2048\",\n");
    fprintf(fp, "  \"duration_per_sample\": %d,\n", TEST_DURATION_SEC);
    fprintf(fp, "  \"total_samples\": %d,\n", rc);
    fprintf(fp, "  \"total_seeds\": %llu,\n", (unsigned long long)total_seeds);
    fprintf(fp, "  \"results\": [\n");
    
    for (int r = 0; r < rc; r++) {
        FlushResult* res = &results[r];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"flush_rate\": %d,\n", res->flush_rate);
        fprintf(fp, "      \"elapsed_time\": %.2f,\n", res->elapsed_time);
        fprintf(fp, "      \"seeds_generated\": %llu,\n", (unsigned long long)res->seeds_generated);
        fprintf(fp, "      \"seeds_accepted\": %llu,\n", (unsigned long long)res->seeds_accepted);
        fprintf(fp, "      \"seeds_per_sec\": %.2f,\n", res->seeds_per_sec);
        fprintf(fp, "      \"mean_length\": %.2f,\n", res->mean_length);
        fprintf(fp, "      \"std_dev\": %.2f,\n", res->std_dev);
        fprintf(fp, "      \"min_length\": %d,\n", res->min_length);
        fprintf(fp, "      \"max_length\": %d,\n", res->max_length);
        fprintf(fp, "      \"boundary_neg1000\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->count_at_neg1000, res->expected_at_neg1000, res->delta_at_neg1000, res->delta_pct_at_neg1000);
        fprintf(fp, "      \"boundary_neg100\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->count_at_neg100, res->expected_at_neg100, res->delta_at_neg100, res->delta_pct_at_neg100);
        fprintf(fp, "      \"boundary_100\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->count_at_100, res->expected_at_100, res->delta_at_100, res->delta_pct_at_100);
        fprintf(fp, "      \"boundary_1000\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->count_at_1000, res->expected_at_1000, res->delta_at_1000, res->delta_pct_at_1000);
        fprintf(fp, "      \"length_distribution\": [");
        for (int i = 0; i < LENGTH_RANGE; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n        ");
            fprintf(fp, "%llu", (unsigned long long)res->length_counts[i]);
        }
        fprintf(fp, "\n      ]\n");
        fprintf(fp, "    }%s\n", (r < rc - 1) ? "," : "");
    }
    
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    
    printf("JSON written to: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");    // <----- output path
    printf("  NPES-2048 Mark neg-X COMPLETE\n");
    printf("  The theoretical limit has been tested.\n");
    printf("  The substrate has been hammered.\n");
    printf("  FOR SCIENCE.\n");
    
    // Cleanup
    for (int i = 0; i < NUM_THREADS; i++) free(td[i].length_counts);
    for (int i = 0; i < rc; i++) free(results[i].length_counts);
    free(results);
    
    return 0;
}