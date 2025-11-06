#include "darknet.h"
#include "main.h"
#include "parser.h"
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

void transfer(int argc, char **argv)
{
        if(argc < 4) {
                fprintf(stderr, "usage: %s %s [train/test/valid] [cfg] [weights (optional)]\n", argv[0], argv[1]);
                return;
        }
        num_exp = find_int_arg(argc, argv, "-num_exp", 10);
        printf("[INFO] num_exp = %d\n", num_exp);

        int arr_size = find_int_arg(argc, argv, "-arr_size", 1);
        arr_size_glob = arr_size;
        
        // partition point of DNN
        int pp_start = find_int_arg(argc, argv, "-pp_start", 5);
        partition_point1 = pp_start - 1;
        int pp_end = find_int_arg(argc, argv, "-pp_end", 5);
        partition_point2 = pp_end;

        int dp = find_int_arg(argc, argv, "-dp", -1);
        global_dp = dp;

        sepa_save_bool = find_int_arg(argc, argv, "-ss", 0);
        // 0 no separate save and load
        // 1 separate save and load
        // 2 separate save but load together

        int cam_index = find_int_arg(argc, argv, "-c", 0);
        int top = find_int_arg(argc, argv, "-t", 0);
        int clear = find_arg(argc, argv, "-clear");
        char *data = argv[3];
        char *cfg = argv[4];
        char *weights = (argc > 5) ? argv[5] : 0;
        char *filename = (argc > 6) ? argv[6] : 0;
        char *layer_s = (argc > 7) ? argv[7] : 0;
        int layer = layer_s ? atoi(layer_s) : -1;

        init_csv_filename_transfer();
        printf("[INFO] CSV output file: %s\n", csv_output_path);

        if(0==strcmp(argv[2], "predict")) {
                state = 'p';
                predict_transfer(data, cfg, weights, filename, top);
        }
}

void predict_transfer(const char *datacfg, const char *cfgfile,
                        const char *weightfile, const char *filename, int top)
{       
        getMemory();
        network *net = load_network(cfgfile, weightfile, 0);
        if (!net) { fprintf(stderr, "Failed to load network\n"); return; }
        set_batch_network(net, 1);

        list *options = read_data_cfg(datacfg);
        char *names_path = option_find_str(options, "names", 0);
        if (!names_path) names_path = option_find_str(options, "labels", "data/labels.list");
        char **names = get_labels(names_path);
        if (top <= 0) top = option_find_int(options, "top", 1);

        int *indexes = calloc(top, sizeof(int));
        if (!indexes) { perror("calloc"); return; }
        
        for (int i = 0; i < num_exp; i++) {
        // warm-up //
        for (int arr_size = 1; arr_size < 5; arr_size++){
                char input_path[256];
                if (filename) {
                        strncpy(input_path, filename, sizeof(input_path));
                        input_path[sizeof(input_path) - 1] = '\0';
                } else {
                        printf("Enter Image Path: "); fflush(stdout);
                        if (!fgets(input_path, sizeof(input_path), stdin)) break;
                        input_path[strcspn(input_path, "\n")] = '\0';
                }

                image im = load_image_color(input_path, 0, 0);
                if (!im.data) { fprintf(stderr, "Failed to load image\n"); continue; }
                image r = letterbox_image(im, net->w, net->h);
        
#ifdef TIME_PROFILE_INFERENCE
                double t_start_inference = get_time_ms();
#endif
                float *predictions = network_predict(net, r.data);

#ifdef TIME_PROFILE_INFERENCE
                double t_end_inference = get_time_ms();
                record_segment("classifier", t_end_inference - t_start_inference);
#endif
                if (net->hierarchy)
                        hierarchy_predictions(predictions, net->outputs, net->hierarchy, 1, 1);
                top_k(predictions, net->outputs, top, indexes);
                for (int i = 0; i < top; ++i) {
                int idx = indexes[i];
                if (idx < 0 || idx >= net->outputs) continue;
                printf("%5.2f%% : %s\n", predictions[idx]*100, names[idx]);
                }
                free_image(im);
                free_image(r);
        }
        // warm-up //
                char input_path[256];
                if (filename) {
                        strncpy(input_path, filename, sizeof(input_path));
                        input_path[sizeof(input_path) - 1] = '\0';
                } else {
                        printf("Enter Image Path: "); fflush(stdout);
                        if (!fgets(input_path, sizeof(input_path), stdin)) break;
                        input_path[strcspn(input_path, "\n")] = '\0';
                }

                image im = load_image_color(input_path, 0, 0);
                if (!im.data) { fprintf(stderr, "Failed to load image\n"); continue; }
                image r = letterbox_image(im, net->w, net->h);
        
#ifdef TIME_PROFILE_INFERENCE
                double t_start_inference = get_time_ms();
#endif
                float *predictions = network_predict_transfer(net, r.data);

#ifdef TIME_PROFILE_INFERENCE
                double t_end_inference = get_time_ms();
                record_segment("classifier", t_end_inference - t_start_inference);
#endif
                if (net->hierarchy)
                        hierarchy_predictions(predictions, net->outputs, net->hierarchy, 1, 1);
                top_k(predictions, net->outputs, top, indexes);
                for (int i = 0; i < top; ++i) {
                int idx = indexes[i];
                if (idx < 0 || idx >= net->outputs) continue;
                printf("%5.2f%% : %s\n", predictions[idx]*100, names[idx]);
                }
                free_image(im);
                free_image(r);
                write_csv_results();
                usleep(100000);
        }
        free(indexes);
        free_network(net);
}