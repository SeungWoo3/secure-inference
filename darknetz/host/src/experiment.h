#ifndef EXPERIMENT_H
#define EXPERIMENT_H

#define TIME_PROFILE_TRANSFER
// #define TIME_PROFILE_INFERENCE
// #define TIME_PROFILE_LAYER
// #define TIME_PROFILE_TASK

#define MAX_SEGMENTS 100

typedef struct {
    char name[64];
    double value;
} Segment;

// 전역 변수 선언 (정의는 experiment.c에만 존재)
extern int num_exp;
extern char csv_output_path[256];
extern int segment_count;
extern Segment segments[MAX_SEGMENTS];

// 함수 프로토타입
double get_time_ms(void);
void record_segment(const char *name, double elapsed_ms);
void write_csv_results(void);
void init_csv_filename(void);

#endif
