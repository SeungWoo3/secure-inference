#include "darknet.h"
#include "main.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <tee_client_api.h>
#include "experiment.h"

#include <sys/time.h>
#include <assert.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include "tcp_transfer.h"

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

void analysis_gemm(int argc, char **argv){
    num_exp = find_int_arg(argc, argv, "-num_exp", 10);
    int arr_size = find_int_arg(argc, argv, "-arr_size", 1);
    arr_size_glob = arr_size;

    init_csv_filename_gemm();

    char *data = argv[3];
    char *cfg = argv[4];
    char *weights = (argc > 5) ? argv[5] : 0;
    char *filename = (argc > 6) ? argv[6] : 0;
    int top = find_int_arg(argc, argv, "-t", 0);
    if(0==strcmp(argv[2], "predict")) {
        for (int i=0; i< num_exp; i++){
            state = 'p';
            predict_gemm_REE(data, cfg, weights, filename, top);
            predict_gemm_TEE(data, cfg, weights, filename, top);
        
            write_csv_results();
        }
    }
    
}

void predict_gemm_REE(const char *datacfg, const char *cfgfile, const char *weightfile, const char *filename, int top){       
    srand(0);
    char label[32];
    
    // --------------------------
    // 실험 파라미터 설정
    // --------------------------
    int channels = 1;
    int height   = arr_size_glob;
    int width    = arr_size_glob;
    int ksize    = 3;
    int stride   = 1;
    int pad      = 0;
    int num_exp  = 100;   // 반복 횟수

    // Conv output shape 계산
    int height_col   = (height + 2*pad - ksize) / stride + 1;
    int width_col    = (width  + 2*pad - ksize) / stride + 1;
    int channels_col = channels * ksize * ksize;
    int M = height_col * width_col;
    int N = 1; // output 채널 1개
    int K = channels_col;

    // --------------------------
    // 메모리 준비 (1회만)
    // --------------------------
    float* input   = make_random_float_array(channels * height * width);
    float* kernel  = make_random_float_array(ksize * ksize * channels);
    float* data_col = (float*)malloc(sizeof(float) * channels_col * height_col * width_col);
    float* output   = (float*)malloc(sizeof(float) * M * N);

    // --------------------------
    // 반복 측정
    // --------------------------
    double total_time = 0.0;
    
    // output 초기화
    for (int i = 0; i < M*N; i++) output[i] = 0.0f;

#ifdef TIME_PROFILE_GEMM
        double t_start_gemm, t_end_gemm;
        t_start_gemm = get_time_ms();
#endif

    // 1️⃣ im2col
    im2col_cpu(input, channels, height, width, ksize, stride, pad, data_col);

    // 2️⃣ GEMM
    gemm_cpu(0, 0, M, N, K, 1.0f,
                data_col, K,
                kernel, N,
                0.0f,
                output, N);

#ifdef TIME_PROFILE_GEMM
            t_end_gemm = get_time_ms();
            memset(label, 0, sizeof(label));  
            snprintf(label, sizeof(label), "REE[%d]", arr_size_glob);
            record_segment(label, t_end_gemm - t_start_gemm);
#endif
    
    // --------------------------
    // 메모리 해제
    // --------------------------
    free(input);
    free(kernel);
    free(data_col);
    free(output);
    return 0;
}

// void predict_gemm_TEE(const char *datacfg, const char *cfgfile, const char *weightfile, const char *filename, int top){       
//     TEEC_Result res;
//     TEEC_Operation op;
//     uint32_t origin;

//     char label[32];
    
//     memset(&op, 0, sizeof(op));
//     op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
//                                      TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);


//     op.params[0].value.a = arr_size_glob;
    
//     res = TEEC_InvokeCommand(&sess, PREGEMM_TEE, &op, &origin);
// #ifdef TIME_PROFILE_GEMM
//         double t_start_gemm_TA, t_end_gemm_TA;
//         t_start_gemm_TA = get_time_ms();
// #endif

//     res = TEEC_InvokeCommand(&sess, GEMM_TEE, NULL, &origin);
// #ifdef TIME_PROFILE_GEMM
//         t_end_gemm_TA = get_time_ms();
//         memset(label, 0, sizeof(label));  
//         snprintf(label, sizeof(label), "TEE[%d]", arr_size_glob);
//         record_segment(label, t_end_gemm_TA - t_start_gemm_TA);
// #endif
//     printf("done tee gemm\n");
// }

void predict_gemm_TEE(const char *datacfg, const char *cfgfile, const char *weightfile,
                      const char *filename, int top)
{
    TEEC_Result res;
    TEEC_Operation op;
    uint32_t origin;
    char label[32];

    int input_size = arr_size_glob * arr_size_glob;  // height * width (채널=1)
    float *input_buf = make_random_float_array(input_size);

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,   // input 전달
        TEEC_VALUE_INPUT,         // arr_size 값 전달
        TEEC_NONE,
        TEEC_NONE
    );

    op.params[0].tmpref.buffer = input_buf;
    op.params[0].tmpref.size   = sizeof(float) * input_size;
    op.params[1].value.a       = arr_size_glob;

    // TEE 내부 데이터 준비 (input 전달 + kernel 생성)
    res = TEEC_InvokeCommand(&sess, PREGEMM_TEE, &op, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "PREGEMM_TEE failed 0x%x origin 0x%x", res, origin);

#ifdef TIME_PROFILE_GEMM
    double t_start, t_end;
    t_start = get_time_ms();
#endif

    // 실제 GEMM 연산
    res = TEEC_InvokeCommand(&sess, GEMM_TEE, NULL, &origin);
    if (res != TEEC_SUCCESS)
        errx(1, "GEMM_TEE failed 0x%x origin 0x%x", res, origin);

#ifdef TIME_PROFILE_GEMM
    t_end = get_time_ms();
    memset(label, 0, sizeof(label));
    snprintf(label, sizeof(label), "TEE[%d]", arr_size_glob);
    record_segment(label, t_end - t_start);
#endif

    free(input_buf);
}