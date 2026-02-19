// NPES_256_Mk_iv2_orbit.c
// Non-Point Entropy System - 256 bit range - iv^2
// Base -4 (Thread Group 0: 0-15) + Base -4 (Thread Group 1: 16-31)
// 360 pulses at 12Hz, 3ms generation per pulse
// Stable orbits are for cowards!
// Author: Nicholas H. Valentine (eNVy) — 2026
// Compile: gcc -O3 -static -pthread 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>

#define MAX_ROLLS 256
#define NUM_THREADS 32
#define PULSE_DURATION_MS 3  // <---- see usleep below
#define NUM_PULSES 360
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
        int group = data->thread_id / 16;
        int flush_rate = *(data->keep_running + 1 + group);
        int length = generate_seed(&data->rng_state, flush_rate, data->base);
        data->seeds_generated++;
        data->seeds_accepted++;
        data->length_counts[length + 256]++;
    }
    return NULL;
}

typedef struct {
    int pulse_number;
    int group0_flush_rate;
    int group1_flush_rate;
    
    // Orbital dynamics
    int distance;
    int delta_magnitude;
    int velocity;
    char group0_movement;  // ↑ or ↓
    char group1_movement;
    char system_state[16];      // "red↓↓", "blue↓↑", etc
    char predicted_state[16];
    int state_code;             // 0-3 (2-bit)
    
    double elapsed_time_ms;
    
    // Group 0 stats
    uint64_t group0_seeds_generated, group0_seeds_accepted;
    double group0_seeds_per_sec;
    uint64_t group0_length_counts[LENGTH_RANGE];
    double group0_mean_length, group0_std_dev;
    int group0_min_length, group0_max_length;
    
    uint64_t group0_count_at_neg128, group0_count_at_neg64, group0_count_at_neg32, group0_count_at_neg16;
    uint64_t group0_count_at_neg8, group0_count_at_neg4, group0_count_at_neg2;
    uint64_t group0_count_at_2, group0_count_at_4, group0_count_at_8;
    uint64_t group0_count_at_16, group0_count_at_32, group0_count_at_64, group0_count_at_128;
    
    double group0_expected_at_neg128, group0_expected_at_neg64, group0_expected_at_neg32, group0_expected_at_neg16;
    double group0_expected_at_neg8, group0_expected_at_neg4, group0_expected_at_neg2;
    double group0_expected_at_2, group0_expected_at_4, group0_expected_at_8;
    double group0_expected_at_16, group0_expected_at_32, group0_expected_at_64, group0_expected_at_128;
    
    double group0_delta_at_neg128, group0_delta_at_neg64, group0_delta_at_neg32, group0_delta_at_neg16;
    double group0_delta_at_neg8, group0_delta_at_neg4, group0_delta_at_neg2;
    double group0_delta_at_2, group0_delta_at_4, group0_delta_at_8;
    double group0_delta_at_16, group0_delta_at_32, group0_delta_at_64, group0_delta_at_128;
    
    double group0_delta_pct_at_neg128, group0_delta_pct_at_neg64, group0_delta_pct_at_neg32, group0_delta_pct_at_neg16;
    double group0_delta_pct_at_neg8, group0_delta_pct_at_neg4, group0_delta_pct_at_neg2;
    double group0_delta_pct_at_2, group0_delta_pct_at_4, group0_delta_pct_at_8;
    double group0_delta_pct_at_16, group0_delta_pct_at_32, group0_delta_pct_at_64, group0_delta_pct_at_128;
    
    // Group 1 stats (same structure)
    uint64_t group1_seeds_generated, group1_seeds_accepted;
    double group1_seeds_per_sec;
    uint64_t group1_length_counts[LENGTH_RANGE];
    double group1_mean_length, group1_std_dev;
    int group1_min_length, group1_max_length;
    
    uint64_t group1_count_at_neg128, group1_count_at_neg64, group1_count_at_neg32, group1_count_at_neg16;
    uint64_t group1_count_at_neg8, group1_count_at_neg4, group1_count_at_neg2;
    uint64_t group1_count_at_2, group1_count_at_4, group1_count_at_8;
    uint64_t group1_count_at_16, group1_count_at_32, group1_count_at_64, group1_count_at_128;
    
    double group1_expected_at_neg128, group1_expected_at_neg64, group1_expected_at_neg32, group1_expected_at_neg16;
    double group1_expected_at_neg8, group1_expected_at_neg4, group1_expected_at_neg2;
    double group1_expected_at_2, group1_expected_at_4, group1_expected_at_8;
    double group1_expected_at_16, group1_expected_at_32, group1_expected_at_64, group1_expected_at_128;
    
    double group1_delta_at_neg128, group1_delta_at_neg64, group1_delta_at_neg32, group1_delta_at_neg16;
    double group1_delta_at_neg8, group1_delta_at_neg4, group1_delta_at_neg2;
    double group1_delta_at_2, group1_delta_at_4, group1_delta_at_8;
    double group1_delta_at_16, group1_delta_at_32, group1_delta_at_64, group1_delta_at_128;
    
    double group1_delta_pct_at_neg128, group1_delta_pct_at_neg64, group1_delta_pct_at_neg32, group1_delta_pct_at_neg16;
    double group1_delta_pct_at_neg8, group1_delta_pct_at_neg4, group1_delta_pct_at_neg2;
    double group1_delta_pct_at_2, group1_delta_pct_at_4, group1_delta_pct_at_8;
    double group1_delta_pct_at_16, group1_delta_pct_at_32, group1_delta_pct_at_64, group1_delta_pct_at_128;
    
    // Combined
    uint64_t combined_seeds_generated, combined_seeds_accepted;
    double combined_seeds_per_sec;
    uint64_t combined_length_counts[LENGTH_RANGE];
    double combined_mean_length, combined_std_dev;
    int combined_min_length, combined_max_length;
} PulseResult;

void calculate_base_neg4_stats(uint64_t* lc, uint64_t total, double* mean, double* std_dev,
                               int* min_len, int* max_len,
                               uint64_t* cn128, uint64_t* cn64, uint64_t* cn32, uint64_t* cn16,
                               uint64_t* cn8, uint64_t* cn4, uint64_t* cn2,
                               uint64_t* c2, uint64_t* c4, uint64_t* c8,
                               uint64_t* c16, uint64_t* c32, uint64_t* c64, uint64_t* c128,
                               double* en128, double* en64, double* en32, double* en16,
                               double* en8, double* en4, double* en2,
                               double* e2, double* e4, double* e8,
                               double* e16, double* e32, double* e64, double* e128,
                               double* dn128, double* dn64, double* dn32, double* dn16,
                               double* dn8, double* dn4, double* dn2,
                               double* d2, double* d4, double* d8,
                               double* d16, double* d32, double* d64, double* d128,
                               double* dpn128, double* dpn64, double* dpn32, double* dpn16,
                               double* dpn8, double* dpn4, double* dpn2,
                               double* dp2, double* dp4, double* dp8,
                               double* dp16, double* dp32, double* dp64, double* dp128) {
    if (total == 0) {
        *mean = 0; *std_dev = 0; *min_len = 0; *max_len = 0;
        return;
    }
    
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
    
    CB(-128, cn128, en128, dn128, dpn128);
    CB(-64, cn64, en64, dn64, dpn64);
    CB(-32, cn32, en32, dn32, dpn32);
    CB(-16, cn16, en16, dn16, dpn16);
    CB(-8, cn8, en8, dn8, dpn8);
    CB(-4, cn4, en4, dn4, dpn4);
    CB(-2, cn2, en2, dn2, dpn2);
    CB(2, c2, e2, d2, dp2);
    CB(4, c4, e4, d4, dp4);
    CB(8, c8, e8, d8, dp8);
    CB(16, c16, e16, d16, dp16);
    CB(32, c32, e32, d32, dp32);
    CB(64, c64, e64, d64, dp64);
    CB(128, c128, e128, d128, dp128);
    
    #undef CB
}

void run_pulse(ThreadData* td, pthread_t* threads, volatile int* control,
               int group0_flush, int group1_flush, PulseResult* res, 
               int pulse_num, int prev_distance, double prev_mean0, double prev_mean1,
               struct timespec* start_time) {
    
    // Reset all thread counters
    for (int i = 0; i < NUM_THREADS; i++) {
        td[i].seeds_generated = 0;
        td[i].seeds_accepted = 0;
        memset((void*)td[i].length_counts, 0, sizeof(td[i].length_counts));
    }
    
    // Set control values
    control[0] = 1;
    control[1] = group0_flush;
    control[2] = group1_flush;
    
    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) pthread_create(&threads[i], NULL, thread_worker, &td[i]);
    
    // Run for 3ms
    usleep(PULSE_DURATION_MS * 1000);
    
    // Stop threads
    control[0] = 0;
    for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);
    
    // Calculate elapsed time
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed_ms = (now.tv_sec - start_time->tv_sec) * 1000.0 + 
                        (now.tv_nsec - start_time->tv_nsec) / 1000000.0;
    
    // Initialize result
    memset(res, 0, sizeof(PulseResult));
    res->pulse_number = pulse_num;
    res->group0_flush_rate = group0_flush;
    res->group1_flush_rate = group1_flush;
    res->elapsed_time_ms = elapsed_ms;
    
    // Aggregate Group 0
    for (int i = 0; i < 16; i++) {
        res->group0_seeds_generated += td[i].seeds_generated;
        res->group0_seeds_accepted += td[i].seeds_accepted;
        for (int j = 0; j < LENGTH_RANGE; j++) {
            res->group0_length_counts[j] += td[i].length_counts[j];
        }
    }
    double pulse_time_sec = PULSE_DURATION_MS / 1000.0;
    res->group0_seeds_per_sec = res->group0_seeds_accepted / pulse_time_sec;
    
    // Aggregate Group 1
    for (int i = 16; i < 32; i++) {
        res->group1_seeds_generated += td[i].seeds_generated;
        res->group1_seeds_accepted += td[i].seeds_accepted;
        for (int j = 0; j < LENGTH_RANGE; j++) {
            res->group1_length_counts[j] += td[i].length_counts[j];
        }
    }
    res->group1_seeds_per_sec = res->group1_seeds_accepted / pulse_time_sec;
    
    // Combined
    res->combined_seeds_generated = res->group0_seeds_generated + res->group1_seeds_generated;
    res->combined_seeds_accepted = res->group0_seeds_accepted + res->group1_seeds_accepted;
    res->combined_seeds_per_sec = res->combined_seeds_accepted / pulse_time_sec;
    for (int j = 0; j < LENGTH_RANGE; j++) {
        res->combined_length_counts[j] = res->group0_length_counts[j] + res->group1_length_counts[j];
    }
    
    // Calculate stats
    if (res->group0_seeds_accepted > 0) {
        calculate_base_neg4_stats(res->group0_length_counts, res->group0_seeds_accepted,
                                 &res->group0_mean_length, &res->group0_std_dev,
                                 &res->group0_min_length, &res->group0_max_length,
                                 &res->group0_count_at_neg128, &res->group0_count_at_neg64,
                                 &res->group0_count_at_neg32, &res->group0_count_at_neg16,
                                 &res->group0_count_at_neg8, &res->group0_count_at_neg4,
                                 &res->group0_count_at_neg2,
                                 &res->group0_count_at_2, &res->group0_count_at_4,
                                 &res->group0_count_at_8, &res->group0_count_at_16,
                                 &res->group0_count_at_32, &res->group0_count_at_64,
                                 &res->group0_count_at_128,
                                 &res->group0_expected_at_neg128, &res->group0_expected_at_neg64,
                                 &res->group0_expected_at_neg32, &res->group0_expected_at_neg16,
                                 &res->group0_expected_at_neg8, &res->group0_expected_at_neg4,
                                 &res->group0_expected_at_neg2,
                                 &res->group0_expected_at_2, &res->group0_expected_at_4,
                                 &res->group0_expected_at_8, &res->group0_expected_at_16,
                                 &res->group0_expected_at_32, &res->group0_expected_at_64,
                                 &res->group0_expected_at_128,
                                 &res->group0_delta_at_neg128, &res->group0_delta_at_neg64,
                                 &res->group0_delta_at_neg32, &res->group0_delta_at_neg16,
                                 &res->group0_delta_at_neg8, &res->group0_delta_at_neg4,
                                 &res->group0_delta_at_neg2,
                                 &res->group0_delta_at_2, &res->group0_delta_at_4,
                                 &res->group0_delta_at_8, &res->group0_delta_at_16,
                                 &res->group0_delta_at_32, &res->group0_delta_at_64,
                                 &res->group0_delta_at_128,
                                 &res->group0_delta_pct_at_neg128, &res->group0_delta_pct_at_neg64,
                                 &res->group0_delta_pct_at_neg32, &res->group0_delta_pct_at_neg16,
                                 &res->group0_delta_pct_at_neg8, &res->group0_delta_pct_at_neg4,
                                 &res->group0_delta_pct_at_neg2,
                                 &res->group0_delta_pct_at_2, &res->group0_delta_pct_at_4,
                                 &res->group0_delta_pct_at_8, &res->group0_delta_pct_at_16,
                                 &res->group0_delta_pct_at_32, &res->group0_delta_pct_at_64,
                                 &res->group0_delta_pct_at_128);
    }
    
    if (res->group1_seeds_accepted > 0) {
        calculate_base_neg4_stats(res->group1_length_counts, res->group1_seeds_accepted,
                                 &res->group1_mean_length, &res->group1_std_dev,
                                 &res->group1_min_length, &res->group1_max_length,
                                 &res->group1_count_at_neg128, &res->group1_count_at_neg64,
                                 &res->group1_count_at_neg32, &res->group1_count_at_neg16,
                                 &res->group1_count_at_neg8, &res->group1_count_at_neg4,
                                 &res->group1_count_at_neg2,
                                 &res->group1_count_at_2, &res->group1_count_at_4,
                                 &res->group1_count_at_8, &res->group1_count_at_16,
                                 &res->group1_count_at_32, &res->group1_count_at_64,
                                 &res->group1_count_at_128,
                                 &res->group1_expected_at_neg128, &res->group1_expected_at_neg64,
                                 &res->group1_expected_at_neg32, &res->group1_expected_at_neg16,
                                 &res->group1_expected_at_neg8, &res->group1_expected_at_neg4,
                                 &res->group1_expected_at_neg2,
                                 &res->group1_expected_at_2, &res->group1_expected_at_4,
                                 &res->group1_expected_at_8, &res->group1_expected_at_16,
                                 &res->group1_expected_at_32, &res->group1_expected_at_64,
                                 &res->group1_expected_at_128,
                                 &res->group1_delta_at_neg128, &res->group1_delta_at_neg64,
                                 &res->group1_delta_at_neg32, &res->group1_delta_at_neg16,
                                 &res->group1_delta_at_neg8, &res->group1_delta_at_neg4,
                                 &res->group1_delta_at_neg2,
                                 &res->group1_delta_at_2, &res->group1_delta_at_4,
                                 &res->group1_delta_at_8, &res->group1_delta_at_16,
                                 &res->group1_delta_at_32, &res->group1_delta_at_64,
                                 &res->group1_delta_at_128,
                                 &res->group1_delta_pct_at_neg128, &res->group1_delta_pct_at_neg64,
                                 &res->group1_delta_pct_at_neg32, &res->group1_delta_pct_at_neg16,
                                 &res->group1_delta_pct_at_neg8, &res->group1_delta_pct_at_neg4,
                                 &res->group1_delta_pct_at_neg2,
                                 &res->group1_delta_pct_at_2, &res->group1_delta_pct_at_4,
                                 &res->group1_delta_pct_at_8, &res->group1_delta_pct_at_16,
                                 &res->group1_delta_pct_at_32, &res->group1_delta_pct_at_64,
                                 &res->group1_delta_pct_at_128);
    }
    
    if (res->combined_seeds_accepted > 0) {
        double dummy_d; uint64_t dummy_u;
        calculate_base_neg4_stats(res->combined_length_counts, res->combined_seeds_accepted,
                                 &res->combined_mean_length, &res->combined_std_dev,
                                 &res->combined_min_length, &res->combined_max_length,
                                 &dummy_u,&dummy_u,&dummy_u,&dummy_u,&dummy_u,&dummy_u,&dummy_u,
                                 &dummy_u,&dummy_u,&dummy_u,&dummy_u,&dummy_u,&dummy_u,&dummy_u,
                                 &dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,
                                 &dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,
                                 &dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,
                                 &dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,
                                 &dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,
                                 &dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d,&dummy_d);
    }
    
    // Calculate orbital dynamics
    res->distance = abs(group1_flush - group0_flush);
    res->velocity = prev_distance - res->distance;  // negative = approaching
    
    // Determine movement directions
    res->group0_movement = (group0_flush > control[1]) ? 'U' : 'D';  // U=↑, D=↓
    res->group1_movement = (group1_flush > control[2]) ? 'U' : 'D';
    
    // Encode system state (2-bit)
    int bit0 = (res->group0_movement == 'U') ? 1 : 0;
    int bit1 = (res->group1_movement == 'U') ? 1 : 0;
    res->state_code = (bit0 << 1) | bit1;
    
    // Color encoding
    const char* colors[] = {"red", "yellow", "blue", "green"};
    const char* arrows[] = {"DD", "DU", "UD", "UU"};
    sprintf(res->system_state, "%s%s", colors[res->state_code], arrows[res->state_code]);
    
    // Predict next state based on mean length trends
    char pred0 = (res->group0_mean_length > prev_mean0) ? 'U' : 'D';
    char pred1 = (res->group1_mean_length > prev_mean1) ? 'U' : 'D';
    int pred_bit0 = (pred0 == 'U') ? 1 : 0;
    int pred_bit1 = (pred1 == 'U') ? 1 : 0;
    int pred_code = (pred_bit0 << 1) | pred_bit1;
    sprintf(res->predicted_state, "%s%c%c", colors[pred_code], pred0, pred1);
}

int main() {
    PulseResult* results = (PulseResult*)malloc(NUM_PULSES * sizeof(PulseResult));
    if (!results) {
        printf("Memory allocation failed!\n");
        return 1;
    }
    
    ThreadData td[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    volatile int control[3];
    
    // Initialize threads
    uint64_t base_seed = (uint64_t)time(NULL);
    for (int i = 0; i < NUM_THREADS; i++) {
        td[i].thread_id = i;
        td[i].base = -4;
        td[i].rng_state = base_seed + (uint64_t)i * 0x123456789ABCDEFULL;
        td[i].keep_running = (volatile int*)control;
        td[i].seeds_generated = 0;
        td[i].seeds_accepted = 0;
        memset((void*)td[i].length_counts, 0, sizeof(td[i].length_counts));
    }
    
    printf("\n");
    printf("  NPES-256 Mark iv^2 - INITIALIZING\n");
    printf("Configuration:\n");
    printf("  Threads: %d (16 per group)\n", NUM_THREADS);
    printf("  Base: -4 (both groups)\n");
    printf("  Pulse duration: %dms\n", PULSE_DURATION_MS);
    printf("  Number of pulses: %d\n", NUM_PULSES);
    printf("  Pulse rate: 12Hz\n");
 
    
    // Initial conditions
    int group0_flush = 5;
    int group1_flush = 105;
    int prev_distance = abs(group1_flush - group0_flush);
    double prev_mean0 = 0.0;
    double prev_mean1 = 0.0;
    
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    // Run the simulation
    for (int pulse = 0; pulse < NUM_PULSES
    ; pulse++) {
    printf("Pulse %d/%d: g0=%d, g1=%d, distance=%d\n",
    pulse + 1, NUM_PULSES, group0_flush, group1_flush, abs(group1_flush - group0_flush));
    run_pulse(td, threads, control, group0_flush, group1_flush, 
              &results[pulse], pulse + 1, prev_distance, prev_mean0, prev_mean1,
              &start_time);
    
    // Save current state for next iteration
    prev_distance = results[pulse].distance;
    prev_mean0 = results[pulse].group0_mean_length;
    prev_mean1 = results[pulse].group1_mean_length;
    
    // Calculate new flush rates using quadratic potential
    int distance = abs(group1_flush - group0_flush);
    
    // Normalize distance to [-1, 1] range (max distance is 100)
    float normalized = (distance - 50.0) / 50.0;
    
    // Quadratic force (marble track)
    float force = normalized * normalized * (normalized < 0 ? -1 : 1);
    
    // Scale to delta (tuning parameter)
    int delta = (int)(force * 10.0);
    if (delta == 0) delta = (distance > 50) ? 1 : -1;
    
    // Apply 95% coupling
    delta = (int)(delta * 0.95);
    if (delta == 0) delta = 1;
    
    results[pulse].delta_magnitude = abs(delta);
    
    // Apply movement
    if (group0_flush < group1_flush) {
        group0_flush += abs(delta);
        group1_flush -= abs(delta);
    } else {
        group0_flush -= abs(delta);
        group1_flush += abs(delta);
    }
    
    // Hard clamps (track walls)
    if (group0_flush < 5) group0_flush = 5;
    if (group0_flush > 105) group0_flush = 105;
    if (group1_flush < 5) group1_flush = 5;
    if (group1_flush > 105) group1_flush = 105;
    
    // Sleep to maintain 12Hz rate (83ms per pulse - 3ms generation = 80ms sleep)
    usleep(80000);                                                                   // <---- usleep control
}

printf("\n");
printf("═══════════════════════════════════════════════════════════════\n");
printf("  NPES_256_iv^2  - COMPLETE\n");
printf("═══════════════════════════════════════════════════════════════\n");

uint64_t total_seeds = 0;
for (int i = 0; i < NUM_PULSES; i++) total_seeds += results[i].combined_seeds_accepted;
printf("Total seeds generated: %llu\n", (unsigned long long)total_seeds);
printf("Average seeds per pulse: %.2f\n", (double)total_seeds / NUM_PULSES);

printf("\nWriting JSON output...\n");
FILE* fp = fopen("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "w");     // <----- output path
if (!fp) {
    printf("ERROR: Could not open JSON file!\n");
    return 1;
}

fprintf(fp, "{\n");
fprintf(fp, "  \"test_name\": \"NPES_256_Mk_iv2_orbit\",\n");
fprintf(fp, "  \"full_name\": \"Non-Point Entropy System 256 - iv^2\",\n");
fprintf(fp, "  \"base_group0\": -4,\n");
fprintf(fp, "  \"base_group1\": -4,\n");
fprintf(fp, "  \"threads_per_group\": 16,\n");
fprintf(fp, "  \"total_threads\": %d,\n", NUM_THREADS);
fprintf(fp, "  \"pulse_duration_ms\": %d,\n", PULSE_DURATION_MS);
fprintf(fp, "  \"num_pulses\": %d,\n", NUM_PULSES);
fprintf(fp, "  \"pulse_rate_hz\": 12,\n");
fprintf(fp, "  \"coupling_factor\": 0.95,\n");
fprintf(fp, "  \"total_seeds\": %llu,\n", (unsigned long long)total_seeds);
fprintf(fp, "  \"pulses\": [\n");

for (int p = 0; p < NUM_PULSES; p++) {
    PulseResult* res = &results[p];
    fprintf(fp, "    {\n");
    fprintf(fp, "      \"pulse_number\": %d,\n", res->pulse_number);
    fprintf(fp, "      \"group0_flush_rate\": %d,\n", res->group0_flush_rate);
    fprintf(fp, "      \"group1_flush_rate\": %d,\n", res->group1_flush_rate);
    fprintf(fp, "      \"elapsed_time_ms\": %.2f,\n", res->elapsed_time_ms);
    
    fprintf(fp, "      \"orbital_dynamics\": {\n");
    fprintf(fp, "        \"distance\": %d,\n", res->distance);
    fprintf(fp, "        \"delta_magnitude\": %d,\n", res->delta_magnitude);
    fprintf(fp, "        \"velocity\": %d,\n", res->velocity);
    fprintf(fp, "        \"group0_movement\": \"%c\",\n", res->group0_movement);
    fprintf(fp, "        \"group1_movement\": \"%c\",\n", res->group1_movement);
    fprintf(fp, "        \"system_state\": \"%s\",\n", res->system_state);
    fprintf(fp, "        \"predicted_state\": \"%s\",\n", res->predicted_state);
    fprintf(fp, "        \"state_code\": %d\n", res->state_code);
    fprintf(fp, "      },\n");
    
    // Group 0
    fprintf(fp, "      \"group0\": {\n");
    fprintf(fp, "        \"seeds_generated\": %llu,\n", (unsigned long long)res->group0_seeds_generated);
    fprintf(fp, "        \"seeds_accepted\": %llu,\n", (unsigned long long)res->group0_seeds_accepted);
    fprintf(fp, "        \"seeds_per_sec\": %.2f,\n", res->group0_seeds_per_sec);
    fprintf(fp, "        \"mean_length\": %.2f,\n", res->group0_mean_length);
    fprintf(fp, "        \"std_dev\": %.2f,\n", res->group0_std_dev);
    fprintf(fp, "        \"min_length\": %d,\n", res->group0_min_length);
    fprintf(fp, "        \"max_length\": %d,\n", res->group0_max_length);
    
    fprintf(fp, "        \"boundary_neg128\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_neg128, res->group0_expected_at_neg128, 
            res->group0_delta_at_neg128, res->group0_delta_pct_at_neg128);
    fprintf(fp, "        \"boundary_neg64\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_neg64, res->group0_expected_at_neg64,
            res->group0_delta_at_neg64, res->group0_delta_pct_at_neg64);
    fprintf(fp, "        \"boundary_neg32\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_neg32, res->group0_expected_at_neg32,
            res->group0_delta_at_neg32, res->group0_delta_pct_at_neg32);
    fprintf(fp, "        \"boundary_neg16\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_neg16, res->group0_expected_at_neg16,
            res->group0_delta_at_neg16, res->group0_delta_pct_at_neg16);
    fprintf(fp, "        \"boundary_neg8\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_neg8, res->group0_expected_at_neg8,
            res->group0_delta_at_neg8, res->group0_delta_pct_at_neg8);
    fprintf(fp, "        \"boundary_neg4\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_neg4, res->group0_expected_at_neg4,
            res->group0_delta_at_neg4, res->group0_delta_pct_at_neg4);
    fprintf(fp, "        \"boundary_neg2\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_neg2, res->group0_expected_at_neg2,
            res->group0_delta_at_neg2, res->group0_delta_pct_at_neg2);
    fprintf(fp, "        \"boundary_2\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_2, res->group0_expected_at_2,
            res->group0_delta_at_2, res->group0_delta_pct_at_2);
    fprintf(fp, "        \"boundary_4\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_4, res->group0_expected_at_4,
            res->group0_delta_at_4, res->group0_delta_pct_at_4);
    fprintf(fp, "        \"boundary_8\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_8, res->group0_expected_at_8,
            res->group0_delta_at_8, res->group0_delta_pct_at_8);
    fprintf(fp, "        \"boundary_16\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_16, res->group0_expected_at_16,
            res->group0_delta_at_16, res->group0_delta_pct_at_16);
    fprintf(fp, "        \"boundary_32\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_32, res->group0_expected_at_32,
            res->group0_delta_at_32, res->group0_delta_pct_at_32);
    fprintf(fp, "        \"boundary_64\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group0_count_at_64, res->group0_expected_at_64,
            res->group0_delta_at_64, res->group0_delta_pct_at_64);
    fprintf(fp, "        \"boundary_128\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->group1_count_at_128, res->group1_expected_at_128,
                res->group1_delta_at_128, res->group1_delta_pct_at_128);
        
        fprintf(fp, "        \"length_distribution\": [");
        for (int i = 0; i < LENGTH_RANGE; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->group1_length_counts[i]);
        }
        fprintf(fp, "\n        ]\n");
        fprintf(fp, "      },\n");
    
    // Group 1
    fprintf(fp, "      \"group1\": {\n");
    fprintf(fp, "        \"seeds_generated\": %llu,\n", (unsigned long long)res->group1_seeds_generated);
    fprintf(fp, "        \"seeds_accepted\": %llu,\n", (unsigned long long)res->group1_seeds_accepted);
    fprintf(fp, "        \"seeds_per_sec\": %.2f,\n", res->group1_seeds_per_sec);
    fprintf(fp, "        \"mean_length\": %.2f,\n", res->group1_mean_length);
    fprintf(fp, "        \"std_dev\": %.2f,\n", res->group1_std_dev);
    fprintf(fp, "        \"min_length\": %d,\n", res->group1_min_length);
    fprintf(fp, "        \"max_length\": %d,\n", res->group1_max_length);
    
    fprintf(fp, "        \"boundary_neg128\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_neg128, res->group1_expected_at_neg128,
            res->group1_delta_at_neg128, res->group1_delta_pct_at_neg128);
    fprintf(fp, "        \"boundary_neg64\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_neg64, res->group1_expected_at_neg64,
            res->group1_delta_at_neg64, res->group1_delta_pct_at_neg64);
    fprintf(fp, "        \"boundary_neg32\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_neg32, res->group1_expected_at_neg32,
            res->group1_delta_at_neg32, res->group1_delta_pct_at_neg32);
    fprintf(fp, "        \"boundary_neg16\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_neg16, res->group1_expected_at_neg16,
            res->group1_delta_at_neg16, res->group1_delta_pct_at_neg16);
    fprintf(fp, "        \"boundary_neg8\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_neg8, res->group1_expected_at_neg8,
            res->group1_delta_at_neg8, res->group1_delta_pct_at_neg8);
    fprintf(fp, "        \"boundary_neg4\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_neg4, res->group1_expected_at_neg4,
            res->group1_delta_at_neg4, res->group1_delta_pct_at_neg4);
    fprintf(fp, "        \"boundary_neg2\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_neg2, res->group1_expected_at_neg2,
            res->group1_delta_at_neg2, res->group1_delta_pct_at_neg2);
    fprintf(fp, "        \"boundary_2\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_2, res->group1_expected_at_2,
            res->group1_delta_at_2, res->group1_delta_pct_at_2);
    fprintf(fp, "        \"boundary_4\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_4, res->group1_expected_at_4,
            res->group1_delta_at_4, res->group1_delta_pct_at_4);
    fprintf(fp, "        \"boundary_8\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_8, res->group1_expected_at_8,
            res->group1_delta_at_8, res->group1_delta_pct_at_8);
    fprintf(fp, "        \"boundary_16\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_16, res->group1_expected_at_16,
            res->group1_delta_at_16, res->group1_delta_pct_at_16);
    fprintf(fp, "        \"boundary_32\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_32, res->group1_expected_at_32,
            res->group1_delta_at_32, res->group1_delta_pct_at_32);
    fprintf(fp, "        \"boundary_64\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
            (unsigned long long)res->group1_count_at_64, res->group1_expected_at_64,
            res->group1_delta_at_64, res->group1_delta_pct_at_64);
    fprintf(fp, "        \"boundary_128\": {\"actual\": %llu, \"expected\": %.2f, \"delta\": %.2f, \"delta_pct\": %.4f},\n",
                (unsigned long long)res->group0_count_at_128, res->group0_expected_at_128,
                res->group0_delta_at_128, res->group0_delta_pct_at_128);
        
        fprintf(fp, "        \"length_distribution\": [");
        for (int i = 0; i < LENGTH_RANGE; i++) {
            if (i > 0) fprintf(fp, ",");
            if (i % 16 == 0) fprintf(fp, "\n          ");
            fprintf(fp, "%llu", (unsigned long long)res->group0_length_counts[i]);
        }
        fprintf(fp, "\n        ]\n");
        fprintf(fp, "      },\n");
    
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
        fprintf(fp, "\n        ]\n");
        fprintf(fp, "      }\n");
    fprintf(fp, "    }%s\n", (p < NUM_PULSES - 1) ? "," : "");
}

fprintf(fp, "  ]\n}\n");
fclose(fp);

printf("JSON written to: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");     // <----- output path
printf("  NPES-256 Mark iv^2 COMPLETE\n");

free(results);
return 0;
}