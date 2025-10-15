#!/usr/bin/env bash
set -e

CMD="sudo host/optee_example_darknetp classifier predict \
  ../tz_datasets/cfg/mnist.dataset \
  ../tz_datasets/cfg/mnist_lenet.cfg \
  ../tz_datasets/models/mnist/mnist_lenet.weights \
  ../tz_datasets/data/mnist/images/t_00007_c3.png"
 
$CMD
for start in {0..10}; do
  for end in $(seq "$start" 10); do   # end >= start
    echo "[pp_start=${start} pp_end=${end}]"
    $CMD -pp_start "$start" -pp_end "$end" -num_exp 1000
  done
done