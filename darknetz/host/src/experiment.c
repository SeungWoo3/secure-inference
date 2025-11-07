#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "experiment.h"

// 전역 변수 정의
int num_exp = 1;
char csv_output_path[256];
int segment_count = 0;
Segment segments[MAX_SEGMENTS];
int csv_header_written = 0;
int partition_point1;
int partition_point2;
int arr_size_glob;
// 시간 측정
double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

// dict처럼 값 추가/수정
void record_segment(const char *name, double elapsed_ms) {
    for (int i = 0; i < segment_count; i++) {
        if (strcmp(segments[i].name, name) == 0) {
            segments[i].value = elapsed_ms;
            return;
        }
    }
    if (segment_count < MAX_SEGMENTS) {
        snprintf(segments[segment_count].name,
                 sizeof(segments[segment_count].name), "%s", name);
        segments[segment_count].value = elapsed_ms;
        segment_count++;
    }
}

// 헤더 + 결과 한 줄 동시에 기록
void write_csv_results(void) {
    // 기존 파일 존재 여부 확인 (헤더 중복 방지)
    int file_exists = access(csv_output_path, F_OK) == 0;

    // 이미 존재하면 append, 없으면 새로 생성
    FILE *fp = fopen(csv_output_path, "a");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", csv_output_path);
        return;
    }

    // 파일이 없었던 경우 → 헤더 작성
    if (!file_exists) {
        for (int i = 0; i < segment_count; i++) {
            fprintf(fp, "%s", segments[i].name);
            if (i < segment_count - 1) fprintf(fp, ",");
        }
        fprintf(fp, "\n");
    }

    // 데이터 한 줄 작성
    for (int i = 0; i < segment_count; i++) {
        fprintf(fp, "%.6f", segments[i].value);
        if (i < segment_count - 1) fprintf(fp, ",");
    }
    fprintf(fp, "\n");

    fclose(fp);
    printf("[INFO] CSV line appended to: %s\n", csv_output_path);

    // 다음 회차 준비
    segment_count = 0;
}

// CSV 파일명 초기화
void init_csv_filename(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    snprintf(csv_output_path, sizeof(csv_output_path),
             "/home/avees/tee/output/output_pp%d-%d_%02d%02d_%02d%02d.csv",
             partition_point1 + 1, partition_point2,
             t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
}

void init_csv_filename_transfer(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    snprintf(csv_output_path, sizeof(csv_output_path),
             "/home/avees/tee/output/transfer_arr_size_%d.csv",
             arr_size_glob);
}

void init_csv_filename_gemm(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    snprintf(csv_output_path, sizeof(csv_output_path),
             "/home/avees/tee/output/gemm_arr_size_%d.csv",
             arr_size_glob);
}