#!/usr/bin/env bash
set -e

CMD="sudo -S host/optee_example_darknetp classifier predict \
  ../tz_datasets/cfg/mnist.dataset \
  ../tz_datasets/cfg/mnist_lenet.cfg \
  ../tz_datasets/models/mnist/mnist_lenet.weights \
  ../tz_datasets/data/mnist/images/t_00007_c3.png"

#nmcli radio wifi off
#sleep 60s
#nmcli radio wifi off
# sleep 20s

# echo "nvidia" | $CMD -num_exp 1

echo "nvidia" | $CMD -pp_start 5 -pp_end 5 -num_exp 100

# for start in {0..10}; do
#  for end in $(seq "$start" 10); do
#    echo "nvidia" | $CMD -pp_start "$start" -pp_end "$end" -num_exp 1
#    # echo "$start $end"
#  done
# done

# echo "nvidia" | sudo host/optee_example_darknetp loop_back 8 100 > ~/log.log

# poweroff

# nmcli radio wifi on
# sleep 10s

# densenet 실행 command
# sudo host/optee_example_darknetp classifier predict \
# ../tz_datasets/cfg/imagenet1k.data\
#  ../tz_datasets/cfg/densenet201.cfg \
#  ../tz_datasets/models/weights/densenet201.weights \
#  ../tz_datasets/data/dog.jpg