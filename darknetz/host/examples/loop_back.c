#include "darknet.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <tee_client_api.h>


#define EVICT_BYTES (16 * 1024 * 1024)   // L2/L3보다 크게 (보드에 맞게 8~64MB로 조정)
#define EVICT_STRIDE 64                  // 보통 캐시라인 크기

static uint8_t *evict_buf = NULL;

static inline void evict_caches(void) {
    // volatile로 최적화 제거 + 64B 간격으로 쓰기(더티화) → 확실히 캐시 축출
    volatile uint8_t *p = (volatile uint8_t *)evict_buf;
    for (size_t i = 0; i < EVICT_BYTES; i += EVICT_STRIDE) {
        p[i]++; // 내용은 중요치 않음
    }
}


float* make_random_float_array(int size) {
    float *arr = (float*)malloc(sizeof(float) * size);
    if (!arr) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    srand((unsigned int)time(NULL));
    for (int i = 0; i < size; i++) {
        float val;
        val = rand_normal();
        arr[i] = val;
    }

    return arr;
}

void print_float_array(float *arr, int size) {
    printf("(size=%d):\n", size);
    for (int i = 0; i < size; i++) {
        printf("%10.6f ", arr[i]);
        if ((i + 1) % 8 == 0) printf("\n");  // 보기 좋게 줄바꿈
    }
    printf("\n");
}

void ree_to_tee(float* arr, int size){
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_VALUE_INPUT,
                                     TEEC_VALUE_OUTPUT, TEEC_NONE);
    int params1 = 0;

    op.params[0].tmpref.buffer = arr;
    op.params[0].tmpref.size = sizeof(float) * size;
    op.params[1].value.a = params1;

    res = TEEC_InvokeCommand(&sess, REE2TEE, &op, &origin);
    if (res != TEEC_SUCCESS)
    errx(1, "TEEC_InvokeCommand(REE2TEE) failed 0x%x origin 0x%x", res, origin);

    uint32_t us = op.params[2].value.a;
    // printf("ree2tee: %d\n", us);
}

void tee_to_ree(float* arr, int size)
{
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
                                     TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);

    op.params[0].tmpref.buffer = arr;
    op.params[0].tmpref.size = sizeof(float) * size;

    res = TEEC_InvokeCommand(&sess, TEE2REE, &op, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand(TEE2REE) failed 0x%x origin 0x%x", res, origin);

    // memcpy(arr, op.params[0].tmpref.buffer, sizeof(float)*size);
    uint32_t us = op.params[1].value.a;
    // printf("tee2ree: %d\n", us);
}


double get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1.0e6 + (double)ts.tv_nsec / 1.0e3;
}





void loop_back(int argc, char **argv)
{
    if (argc < 4) {
        printf("Usage: %s <dummy> <repeat> <range>\n", argv[0]);
        printf("Example: %s x 10 100 -> 100개의 size를 10번씩 반복\n", argv[0]);
        return;
    }

    int repeat = atoi(argv[2]); // 각 size당 반복 횟수
    int range = atoi(argv[3]);  // 실험할 size의 범위 (1 ~ range)
    printf("repeat: %d, range: %d\n", repeat, range);

    // 캐시 제거용 버퍼 준비
    if (!evict_buf) {
        posix_memalign((void**)&evict_buf, 64, EVICT_BYTES);
        if (!evict_buf) { perror("evict_buf alloc"); return; }
        for (size_t i = 0; i < EVICT_BYTES; i += EVICT_STRIDE)
            evict_buf[i] = (uint8_t)i;
    }

    // CSV 파일 열기
    FILE *fp = fopen("loop_back_result1.csv", "w");
    if (!fp) { perror("CSV open"); return; }

    // CSV 헤더 작성
    fprintf(fp, "size,input(us),output(us)\n");
    int array[] = {2352, 4704, 1176, 294, 120, 84, 10, 1};
    // ---- 실험 루프 ----
    range = 8;
    for (int idx = 0; idx < range; idx++) {        // size: 1 ~ range
        for (int j = 0; j < repeat; j++) {    // 각 size를 여러 번 반복
            int s=array[idx];
            float* rand_array = make_random_float_array(s);
            float* output     = make_random_float_array(s);
ree_to_tee(rand_array, s);ree_to_tee(rand_array, s);

            evict_caches();
            double t_start_in = get_time_us();
            ree_to_tee(rand_array, s);
            double t_end_in = get_time_us();
            double input_time = t_end_in - t_start_in;

            double t_start_out = get_time_us();
            tee_to_ree(output, s);
            double t_end_out = get_time_us();
            double output_time = t_end_out - t_start_out;

            fprintf(fp, "%d,%.9f,%.9f\n", s, input_time, output_time);

            free(rand_array);
            free(output);
        }
    }

    fclose(fp);
    printf("✅ CSV 파일이 생성되었습니다: loop_back_result1.csv\n");
}
// #define NUM_THREADS 200   // 스레드 개수 (TEE의 CFG_NUM_THREADS 이하)

// typedef struct {
//     float *rand_array;
//     float *output;
//     int size;
// } ThreadArgs;

// // ───────────────────────────────
// // thread_func: 오직 ree_to_tee(), tee_to_ree()만 수행
// // ───────────────────────────────
// void* thread_func(void* arg)
// {
//     ThreadArgs *args = (ThreadArgs *)arg;

//     ree_to_tee(args->rand_array, args->size);
//     tee_to_ree(args->output, args->size);

//     return NULL;
// }

// // ───────────────────────────────
// // loop_back: 병렬 vs 순차 실행 비교
// // ───────────────────────────────
// void loop_back(int argc, char **argv)
// {
//     int size = atoi(argv[2]);
//     printf("size: %d\n", size);

//     pthread_t threads[NUM_THREADS];
//     ThreadArgs targs[NUM_THREADS];

//     // 각 스레드용 랜덤 배열 생성
//     for (int i = 0; i < NUM_THREADS; i++) {
//         targs[i].rand_array = make_random_float_array(size);
//         targs[i].output     = make_random_float_array(size);
//         targs[i].size       = size;
//     }

//     // 1️⃣ 병렬 실행
//     double start = get_time_us();
//     for (int i = 0; i < NUM_THREADS; i++)
//         pthread_create(&threads[i], NULL, thread_func, &targs[i]);

//     for (int i = 0; i < NUM_THREADS; i++)
//         pthread_join(threads[i], NULL);
//     double end = get_time_us();
//     printf("[Parallel] elapsed time: %.3f us\n", end - start);

//     // 2️⃣ 순차 실행
//     start = get_time_us();
//     for (int i = 0; i < NUM_THREADS; i++) {
//         pthread_create(&threads[i], NULL, thread_func, &targs[i]);
//         pthread_join(threads[i], NULL);
//     }
//     end = get_time_us();
//     printf("[Sequential] elapsed time: %.3f us\n", end - start);

//     printf("All threads finished.\n");

//     // 메모리 해제
//     for (int i = 0; i < NUM_THREADS; i++) {
//         free(targs[i].rand_array);
//         free(targs[i].output);
//     }
// }