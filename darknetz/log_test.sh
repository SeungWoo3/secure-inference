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

# echo "nvidia" | $CMD -num_exp 1000

echo "nvidia" | $CMD -pp_start 2 -pp_end 10 -num_exp 1

# for start in {0..10}; do
#   for end in $(seq "$start" 10); do
#     echo "nvidia" | $CMD -pp_start "$start" -pp_end "$end" -num_exp 1000
#     # echo "$start $end"
#   done
# done

# poweroff


# nmcli radio wifi on
# sleep 10s
