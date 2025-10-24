#include "darknet.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <tee_client_api.h>

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
                                     TEEC_NONE, TEEC_NONE);

    // float *params0 = malloc(sizeof(float)*size);
    // for(int z=0; z<size; z++){
    //     params0[z] = arr[z];
    // }
    int params1 = 0;

    // op.params[0].tmpref.buffer = params0;
    op.params[0].tmpref.buffer = arr;
    op.params[0].tmpref.size = sizeof(float) * size;
    op.params[1].value.a = params1;

    res = TEEC_InvokeCommand(&sess, REE2TEE, &op, &origin);
    if (res != TEEC_SUCCESS)
    errx(1, "TEEC_InvokeCommand(REE2TEE) failed 0x%x origin 0x%x", res, origin);
}

void tee_to_ree(float* arr, int size)
{
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
                                     TEEC_NONE, TEEC_NONE, TEEC_NONE);

    // REE 쪽에서 데이터를 받을 버퍼
    // float *params0 = malloc(sizeof(float) * size);
    // if (!params0)
    //     errx(1, "malloc failed in tee_to_ree()");

    op.params[0].tmpref.buffer = malloc(sizeof(float) * size);
    op.params[0].tmpref.size = sizeof(float) * size;

    // TEE 내부의 전역 배열(loop_back_buffer)을 REE로 복사해오는 명령 호출
    res = TEEC_InvokeCommand(&sess, TEE2REE, &op, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand(TEE2REE) failed 0x%x origin 0x%x", res, origin);

    // 받은 결과를 arr에 복사
    memcpy(arr, op.params[0].tmpref.buffer, sizeof(float)*size);
    // for (int z = 0; z < size; z++) {
    //     arr[z] = params0[z];
    // }
    // free(params0);
}


double get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1.0e6 + (double)ts.tv_nsec / 1.0e3;
}

void loop_back(int argc, char **argv)
{
    int size = atoi(argv[2]);
    printf("size: %d\n", size);

    for (int i=0; i<10; i++){
    float* rand_array = make_random_float_array(size);
    float* output = make_random_float_array(size);
    // warm-up
    ree_to_tee(rand_array, size);
    ree_to_tee(rand_array, size);
    // print_float_array(rand_array, size);

                    double t_start_in = get_time_us();
    ree_to_tee(rand_array, size);
                    double t_end_in = get_time_us();
                    printf("Input: %.9f us\n", t_end_in - t_start_in);

                    double t_start_out = get_time_us();
    tee_to_ree(output, size);
                    double t_end_out = get_time_us();
                    printf("Output: %.9f us\n", t_end_out - t_start_out);
    }
    // print_float_array(output, size);
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