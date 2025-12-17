#include "BitmapPlusPlus.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>

// 全域原子變數，用來確保多個執行緒不會拿到同一個檔案編號
std::atomic<int> g_currentFileIndex(0);

inline uint8_t boundPixel(double val) {
  if (val < 0)
    return 0;
  if (val > 255)
    return 255;
  return static_cast<uint8_t>(round(val));
}

// 優化後的 filtingRow：移除了 new/delete，改用 stack 陣列
void filtingRow(bmp::Bitmap &des, const bmp::Bitmap &src, const int row,
                const double W[], const int kSize) {
  // 由於 kSize 固定為 3x3，這裡 len 固定為 9
  const int offset = kSize >> 1;
  const int len = kSize * kSize;

  // 優化：使用固定大小陣列代替 new/delete，避免昂貴的 Heap Allocation
  std::vector<bmp::Pixel>::const_iterator A[9];

  int width = src.width();

  // 設置 3x3 區域的 9 個指標的起點
  for (int i = 0, r = -offset; r <= offset; ++r) {
    // A[i++] 指向中心行 (row + r) 的第一個像素
    A[i++] = src.cbegin() + (row + r) * width;
    // 設置該行的其他兩個像素指標
    for (int j = -offset + 1; j <= offset; ++j, ++i)
      A[i] = A[i - 1] + 1;
  }

  std::vector<bmp::Pixel>::iterator tar =
      des.begin() + row * des.width() + offset;

  // 滑動視窗卷積運算
  for (int i = kSize - 1; i < des.width(); ++i) {
    double r = 0, g = 0, b = 0;
    for (int j = 0; j < len; ++j) {
      // 卷積：顏色值 x 權重
      r += W[j] * A[j]->r;
      g += W[j] * A[j]->g;
      b += W[j] * A[j]->b;
      ++A[j]; // 優化：將所有 9 個指標向右移動一格 (Sliding Window)
    }

    *tar = bmp::Pixel(boundPixel(r), boundPixel(g), boundPixel(b));
    ++tar;
  }
}

// 執行緒的工作函式：負責讀取、處理、儲存單個檔案
void workerThread(char *inputPath, char *outputPath, int amountOfFile) {
  const double W[] = {1, 0, -1, 2, 0, -2, 1, 0, -1};
  char infilename[128], outfilename[128];

  while (true) {
    // 原子操作：取得目前要處理的檔案編號，並將計數器 +1
    int filecount = g_currentFileIndex.fetch_add(1);

    // 如果編號超過總檔案數，則結束執行緒
    if (filecount >= amountOfFile) {
      break;
    }

    // 構建輸入和輸出檔名
    std::snprintf(infilename, sizeof(infilename), "%s%d.bmp", inputPath,
                  filecount);
    std::snprintf(outfilename, sizeof(outfilename), "%s%d.bmp", outputPath,
                  filecount);

    try {
      // 1. 讀檔 (I/O)
      bmp::Bitmap srcImage;
      srcImage.load(infilename);

      if (srcImage.width() == 0 || srcImage.height() == 0)
        continue;

      bmp::Bitmap desImage(srcImage.width(), srcImage.height());

      // 2. 計算 (CPU)
      // 執行 Sobel 濾鏡，跳過邊緣一行
      for (int R = 1; R < srcImage.height() - 1; ++R) {
        filtingRow(desImage, srcImage, R, W, 3);
      }

      // 3. 存檔 (I/O)
      desImage.save(outfilename);
    } catch (const std::exception &e) {
      std::cerr << "Error processing file " << filecount << ": " << e.what()
                << std::endl;
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout << "Usage: " << argv[0]
              << " <SOURCE_BMP> <TARGET_BMP> <AMOUNT_OF_FILE> " << std::endl;
    return 0;
  }

  try {
    int amountoffile = atoi(argv[3]);
    auto beginTime = std::chrono::steady_clock::now();

    // 決定執行緒數量：
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4; // Fallback if hardware_concurrency returns 0

    // 避免執行緒數量超過任務總數
    if (numThreads > (unsigned int)amountoffile) {
      numThreads = amountoffile;
    }

    std::vector<std::thread> threads;

    // 啟動執行緒池：讓多個核心同時進行檔案處理
    for (unsigned int i = 0; i < numThreads; ++i) {
      threads.emplace_back(workerThread, argv[1], argv[2], amountoffile);
    }

    // 等待所有執行緒完成
    for (auto &t : threads) {
      if (t.joinable()) {
        t.join();
      }
    }

    auto endTime = std::chrono::steady_clock::now();
    double totaltime =
        std::chrono::duration<double>(endTime - beginTime).count();
    std::cout << "Takes " << totaltime << "secs" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Main Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
