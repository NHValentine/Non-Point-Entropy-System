// NPES_256_X_ChaCha
// Time-based sweep test: 10 seconds per flush rate (11-111, odd numbers only)
// Base 10 digit generation with ChaCha20 PRNG
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

// ChaCha20 State
typedef struct {
    uint32_t state[16];
    uint32_t keystream[16];
    int keystream_index;
} ChaChaState;

// ChaCha20 quarter round
#define ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define QR(a, b, c, d) \
    a += b; d ^= a; d = ROTL(d,16); \
    c += d; b ^= c; b = ROTL(b,12); \
    a += b; d ^= a; d = ROTL(d,8); \
    c += d; b ^= c; b = ROTL(b,7);

// ChaCha20 block function
void chacha_block(ChaChaState* s) {
    uint32_t x[16];
    memcpy(x, s->state, sizeof(x));
    
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }
    
    for (int i = 0; i < 16; i++) {
        s->keystream[i] = x[i] + s->state[i];
    }
    
    s->state[12]++;
    if (s->state[12] == 0) s->state[13]++;
    
    s->keystream_index = 0;
}

// Initialize ChaCha20
void chacha_init(ChaChaState* s, uint64_t seed) {
    const char* constants = "expand 32-byte k";
    memcpy(&s->state[0], constants, 16);
    
    s->state[4] = (uint32_t)(seed);
    s->state[5] = (uint32_t)(seed >> 32);
    s->state[6] = (uint32_t)(seed ^ 0x123456789ABCDEF0ULL);
    s->state[7] = (uint32_t)((seed ^ 0x123456789ABCDEF0ULL) >> 32);
    s->state[8] = (uint32_t)(seed * 0x9e3779b97f4a7c15ULL);
    s->state[9] = (uint32_t)((seed * 0x9e3779b97f4a7c15ULL) >> 32);
    s->state[10] = (uint32_t)(~seed);
    s->state[11] = (uint32_t)((~seed) >> 32);
    
    s->state[12] = 0;
    s->state[13] = 0;
    s->state[14] = 0;
    s->state[15] = 0;
    
    s->keystream_index = 16;
}

// Get next random uint64
uint64_t chacha_next(ChaChaState* s) {
    if (s->keystream_index >= 16) {
        chacha_block(s);
    }
    
    uint64_t result = ((uint64_t)s->keystream[s->keystream_index] |
                      ((uint64_t)s->keystream[s->keystream_index + 1] << 32));
    s->keystream_index += 2;
    
    return result;
}

// Thread-local data
typedef struct {
    int thread_id;
    ChaChaState rng_state;
    uint64_t length_counts[257];
    uint64_t seeds_generated;
    uint64_t seeds_accepted;
    volatile int* keep_running;
} ThreadData;

// Generate seed with configurable flush rate
int generate_seed(ChaChaState* rng_state, int flush_rate) {
    int digit_count = 0;
    
    for (int slot = 0; slot < MAX_ROLLS; slot++) {
        int roll = chacha_next(rng_state) % flush_rate;
        
        if (roll >= BASE) {
            continue;
        }
        
        digit_count++;
    }
    
    return digit_count;
}

// Thread worker
void* thread_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    while (*(data->keep_running)) {
        int length = generate_seed(&data->rng_state, *(data->keep_running + 1));
        data->seeds_generated++;
        data->seeds_accepted++;
        data->length_counts[length]++;
    }
    
    return NULL;
}

// Results structure
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
    
    double sum = 0;
    for (int i = 0; i <= 256; i++) {
        sum += i * result->length_counts[i];
    }
    result->mean_length = sum / total;
    
    uint64_t cumsum = 0;
    for (int i = 0; i <= 256; i++) {
        cumsum += result->length_counts[i];
        if (cumsum >= total / 2) {
            result->median_length = i;
            break;
        }
    }
    
    double var_sum = 0;
    for (int i = 0; i <= 256; i++) {
        if (result->length_counts[i] > 0) {
            double diff = i - result->mean_length;
            var_sum += diff * diff * result->length_counts[i];
        }
    }
    result->std_dev = sqrt(var_sum / total);
    
    result->min_length = 256;
    result->max_length = 0;
    for (int i = 0; i <= 256; i++) {
        if (result->length_counts[i] > 0) {
            if (i < result->min_length) result->min_length = i;
            if (i > result->max_length) result->max_length = i;
        }
    }
    
    result->count_at_10 = result->length_counts[10];
    result->count_at_100 = result->length_counts[100];
    
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
    printf("NPES_256_X_ChaCha - Base 10 Boundary Test\n");
    printf("======================================\n");
    printf("Threads: %d\n", NUM_THREADS);
    printf("Duration per flush rate: %d seconds\n", TEST_DURATION_SEC);
    printf("Flush rates: 11 to 111 (odd numbers only)\n\n");
    
    FlushResult results[51];
    int result_count = 0;
    
    ThreadData thread_data[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    volatile int control[2];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        chacha_init(&thread_data[i].rng_state, (uint64_t)time(NULL) + i * 0x123456789ABCDEFULL);
        thread_data[i].keep_running = (volatile int*)control;
        thread_data[i].seeds_generated = 0;
        thread_data[i].seeds_accepted = 0;
        memset((void*)thread_data[i].length_counts, 0, sizeof(thread_data[i].length_counts));
    }
    
    for (int flush_rate = 11; flush_rate <= 111; flush_rate += 2) {
        printf("\n=== FLUSH RATE: %d ===\n", flush_rate);
        
        for (int i = 0; i < NUM_THREADS; i++) {
            thread_data[i].seeds_generated = 0;
            thread_data[i].seeds_accepted = 0;
            memset((void*)thread_data[i].length_counts, 0, sizeof(thread_data[i].length_counts));
        }
        
        control[0] = 1;
        control[1] = flush_rate;
        
        time_t start_time = time(NULL);
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_create(&threads[i], NULL, thread_worker, &thread_data[i]);
        }
        
        time_t now;
        do {
            now = time(NULL);
        } while (difftime(now, start_time) < TEST_DURATION_SEC);
        
        control[0] = 0;
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        
        time_t end_time = time(NULL);
        double elapsed = difftime(end_time, start_time);
        
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
    
    printf("\n\nWriting JSON output...\n");
    FILE* fp = fopen("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "w");     //<----- output path
    printf("\nTEST COMPLETE!\n");
    if (!fp) {
        printf("ERROR: Could not open JSON file for writing!\n");
        return 1;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"test_name\": \"NPES_256_X_ChaCha\",\n");
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
    
    printf("JSON written to: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");     //<----- output path
    printf("\nTEST COMPLETE!\n");

    printf("\nTEST COMPLETE!\n");
    
    return 0;
}