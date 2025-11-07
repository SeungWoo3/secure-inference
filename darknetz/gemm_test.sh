#!/usr/bin/env bash
# set -e

CORE_ID=4

# 실행 명령어 정의
CMD="sudo taskset -c ${CORE_ID} host/optee_example_darknetp gemm predict"

# Wi-Fi 비활성화 (선택)
# nmcli radio wifi off
sleep 20s

# 테스트할 배열 크기 목록
NUM_LIST=(1 2 4 8 16 32 64 128 256)
for i in $(seq 1 500); do
  NUM_LIST+=($i)
done
FAIL_COUNT=0

# 실험 루프
TOTAL_RUNS=1

for ((run=1; run<=TOTAL_RUNS; run++)); do
  echo "=========================="
  echo "▶ 전체 반복: $run / $TOTAL_RUNS"
  echo "=========================="

  for NUM in "${NUM_LIST[@]}"; do
    echo "==== 실행: NUM=$NUM ===="
    
    echo "nvidia" | $CMD -arr_size "$NUM" -num_exp 1000
    ret=$?

    if [ $ret -ne 0 ]; then
      echo "⚠️ 실패: NUM=$NUM (exit code=$ret)"
      FAIL_COUNT=$((FAIL_COUNT + 1))
      echo "$(date): NUM=$NUM 실패 (run=$run)" >> ~/fail_log.txt
    fi

    # 프로세스 안정화를 위한 짧은 대기 (선택)
    sleep 1s
  done

  echo "✅ 반복 $run 완료"
  echo
done

echo "✅ 모든 실행 완료 (실패: $FAIL_COUNT회)"
# echo "nvidia" | sudo nmcli radio wifi on
sleep 5s
poweroff
