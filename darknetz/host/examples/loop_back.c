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

    float *params0 = malloc(sizeof(float)*size);
    for(int z=0; z<size; z++){
        params0[z] = arr[z];
    }
    int params1 = 0;

    op.params[0].tmpref.buffer = params0;
    op.params[0].tmpref.size = sizeof(float) * size;
    op.params[1].value.a = params1;

    res = TEEC_InvokeCommand(&sess, REE2TEE,
                             &op, &origin);
    if (res != TEEC_SUCCESS)
    errx(1, "TEEC_InvokeCommand(REE2TEE) failed 0x%x origin 0x%x",
         res, origin);

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
    float *params0 = malloc(sizeof(float) * size);
    if (!params0)
        errx(1, "malloc failed in tee_to_ree()");

    op.params[0].tmpref.buffer = params0;
    op.params[0].tmpref.size = sizeof(float) * size;

    // TEE 내부의 전역 배열(loop_back_buffer)을 REE로 복사해오는 명령 호출
    res = TEEC_InvokeCommand(&sess, TEE2REE,
                             &op, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand(TEE2REE) failed 0x%x origin 0x%x",
             res, origin);

    // 받은 결과를 arr에 복사
    for (int z = 0; z < size; z++) {
        arr[z] = params0[z];
    }

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

    float* rand_array = make_random_float_array(size);
    float* output = make_random_float_array(size);
    // print_float_array(rand_array, size);

        double t_start_in = get_time_us();
ree_to_tee(rand_array, size);
double t_end_in = get_time_us();
printf("Input: %.3f us\n", t_end_in - t_start_in);

double t_start_out = get_time_us();
tee_to_ree(output, size);
double t_end_out = get_time_us();
printf("Output: %.3f us\n", t_end_out - t_start_out);

    // print_float_array(output, size);
}