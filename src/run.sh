#!/bin/bash

# 設定測試參數
INPUT_PATH="./assets/in/sample"
OUTPUT_PATH="./assets/out/out"
AMOUNT=20
RUNS=10 # 設定跑 10 次，跟你的截圖一樣

# 老師的基準數據 (Benchmark)
X_TEACHER=14.7829
Y_TEACHER=4.0808

# 定義顏色代碼 (使用 ANSI-C Quoting 以確保兼容性)
GREEN=$'\e[1;32m'
RED=$'\e[1;31m'
YELLOW=$'\e[1;33m'
RESET=$'\e[0m'

# 建立輸出目錄
mkdir -p ./assets/out/
rm -f ${OUTPUT_PATH}_single*.bmp ${OUTPUT_PATH}_multi*.bmp

# ==========================================
# 1. 編譯 (Compile)
# ==========================================
echo "Compiling..."
g++ -O3 -std=c++17 ./src/single_thread.cpp -o ./src/single_thread
g++ -O3 -std=c++17 -pthread ./src/multi_thread.cpp -o ./src/multi_thread

if [ ! -f ./src/single_thread ] || [ ! -f ./src/multi_thread ]; then
  echo "Error: Compilation failed."
  exit 1
fi
echo "Done."
echo ""

# ==========================================
# 2. 顯示表頭 (Header)
# ==========================================
echo "=========================================================================================="
echo "   老師基準 X (Single): $X_TEACHER 秒"
echo "   老師基準 Y (Multi) : $Y_TEACHER 秒"
echo "=========================================================================================="
printf "| %-5s | %-10s | %-10s | %-10s | %-12s | %-17s |\n" "Run" "Single(a)" "Multi(b)" "Speedup" "Est(Y_est)" "Grade"
echo "|-------|------------|------------|------------|--------------|-------------------|"

# ==========================================
# 3. 執行測試迴圈 (Test Loop)
# ==========================================
for ((i = 1; i <= RUNS; i++)); do
  # --- 執行 Single Thread ---
  out_a=$(./src/single_thread $INPUT_PATH ${OUTPUT_PATH}_single $AMOUNT)
  a=$(echo "$out_a" | grep "Takes" | awk '{print $2}' | sed 's/secs//')

  # --- 執行 Multi Thread ---
  out_b=$(./src/multi_thread $INPUT_PATH ${OUTPUT_PATH}_multi $AMOUNT)
  b=$(echo "$out_b" | grep "Takes" | awk '{print $2}' | sed 's/secs//')

  # --- 計算數據 (Calculations) ---
  # 1. Speedup (加速比) = a / b
  speedup=$(echo "scale=4; $a / $b" | bc)

  # 2. Est(Y_est) (推算老師環境下的 Multi 時間) = Teacher_X / Speedup
  y_est=$(echo "scale=4; $X_TEACHER / $speedup" | bc)

  # 3. Grade (成績計算)
  score=0

  # 檢查是否比 Single 還慢 (Case 1)
  if (($(echo "$b >= $a" | bc -l))); then
    score=40
  # 檢查是否比老師的優化目標還快 (Case 3)
  elif (($(echo "$y_est <= $Y_TEACHER" | bc -l))); then
    # 預估分數修正 (放大 5 倍差異)
    diff=$(echo "scale=4; $Y_TEACHER - $y_est" | bc)
    score=$(echo "scale=2; ($diff * 5) + 100" | bc)
  else
    # 介於中間 (Case 2)
    numerator=$(echo "scale=4; $X_TEACHER - $y_est" | bc)
    denominator=$(echo "scale=4; $X_TEACHER - $Y_TEACHER" | bc)
    ratio=$(echo "scale=4; $numerator / $denominator" | bc)
    score=$(echo "scale=2; $ratio * 60 + 40" | bc)
  fi

  # --- 顏色格式化 (修正版) ---
  if (($(echo "$score >= 90" | bc -l))); then
    # 綠色
    printf "| Run %-2d| %-10.4f | %-10.4f | %-10.4f | %-12.4f | ${GREEN}%-17s${RESET} |\n" "$i" "$a" "$b" "$speedup" "$y_est" "$score"
  elif (($(echo "$score < 60" | bc -l))); then
    # 紅色
    printf "| Run %-2d| %-10.4f | %-10.4f | %-10.4f | %-12.4f | ${RED}%-17s${RESET} |\n" "$i" "$a" "$b" "$speedup" "$y_est" "$score"
  else
    # 黃色
    printf "| Run %-2d| %-10.4f | %-10.4f | %-10.4f | %-12.4f | ${YELLOW}%-17s${RESET} |\n" "$i" "$a" "$b" "$speedup" "$y_est" "$score"
  fi

  average_score=$(grep "Run " | awk -F'|' '{sum+=$7} END {print sum/NR}' | sed 's/\x1b\[[0-9;]*m//g')
  echo "平均預估分 (Avg Score) : ${YELLOW}${average_score}${RESET}"

done

echo "=========================================================================================="
echo "註: Est(Y_est) = Teacher_X / Speedup (數值越小越好，目標 < 4.0808)"
echo "註: Grade 為預估值，若在 100 files 測試中保持此加速比，將獲得此分數。"
echo ""
