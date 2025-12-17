#!/bin/bash

INPUT_PATH="./assets/in/sample" OUTPUT_PATH="./assets/out/out" AMOUNT=20

X_TEACHER=14.7829
Y_TEACHER=4.0808

mkdir -p ./assets/out/
rm -f ${OUTPUT_PATH}_single*.bmp ${OUTPUT_PATH}_multi*.bmp

echo "----------------------------------------"
echo "Compiling..."
g++ -O3 -std=c++17 ./src/single_thread.cpp -o ./src/single_thread
g++ -O3 -std=c++17 -pthread ./src/multi_thread.cpp -o ./src/multi_thread
echo "Done."

echo "----------------------------------------"
echo "Running Single Thread (Baseline) on $AMOUNT files..."

output_a=$(./src/single_thread $INPUT_PATH ${OUTPUT_PATH}_single $AMOUNT)
echo "$output_a"
a=$(echo "$output_a" | grep "Takes" | awk '{print $2}' | sed 's/secs//')

echo "----------------------------------------"
echo "Running Multi Thread (Optimized) on $AMOUNT files..."

output_b=$(./src/multi_thread $INPUT_PATH ${OUTPUT_PATH}_multi $AMOUNT)
echo "$output_b"
b=$(echo "$output_b" | grep "Takes" | awk '{print $2}' | sed 's/secs//')

echo "----------------------------------------"
echo "Calculating Estimated Score..."

# 計算老師的加速倍率 (X/Y)
teacher_speedup=$(echo "scale=4; $X_TEACHER / $Y_TEACHER" | bc)
echo "Teacher Speedup (X/Y): $teacher_speedup X"

# 假設本地測試環境的「優化標竿時間 Y_LOCAL」
# 我們用您的單執行緒時間 a，除以老師的加速比，來模擬老師的優化水準
Y_LOCAL=$(echo "scale=4; $a / $teacher_speedup" | bc)

echo "Your Time (a): ${a}s (Local X)"
echo "Local Optimization Target (Y_LOCAL): ${Y_LOCAL}s"

# 檢查分數條件 (Your Time: b)
score=""

# 情況 1: 您的時間 > X_LOCAL (b >= a)
if (($(echo "$b >= $a" | bc -l))); then
  score=40
  grade_case="Case 1 (b >= a)"

# 情況 3: 您的時間 <= Y_LOCAL (b <= Y_LOCAL)
elif (($(echo "$b <= $Y_LOCAL" | bc -l))); then
  # Score = Y_LOCAL - b + 100
  score=$(echo "scale=2; $Y_LOCAL - $b + 100" | bc)
  grade_case="Case 3 (b <= Y_LOCAL)"

# 情況 2: Y_LOCAL < 您的時間 < X_LOCAL (介於中間)
else
  # Score = ((X - b) / (X - Y)) * 60 + 40
  # X_LOCAL = a, Y_LOCAL = Y_LOCAL
  # denominator = a - Y_LOCAL
  denominator=$(echo "scale=4; $a - $Y_LOCAL" | bc)
  numerator=$(echo "scale=4; $a - $b" | bc)

  # 避免分母為零
  if (($(echo "$denominator == 0" | bc -l))); then
    score=40
  else
    score=$(echo "scale=2; ($numerator / $denominator) * 60 + 40" | bc)
  fi
  grade_case="Case 2 (Y_LOCAL < b < a)"
fi

echo ""
echo ">>> Score Calculation Results <<<"
echo "Your Time (a): ${a}s"
echo "Your Multi-Thread Time (b): ${b}s"
echo "Teacher Speedup (X/Y): $teacher_speedup X"
echo "Optimization Target (Y_LOCAL): ${Y_LOCAL}s"
echo "----------------------------------------"
echo "APPLYING FORMULA: $grade_case"
echo "ESTIMATED SCORE: $score / 120 (Based on 20 files benchmark)"
echo "----------------------------------------"
