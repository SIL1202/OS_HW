#include "BitmapPlusPlus.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>

// 全域原子計數器，負責分配檔案編號
std::atomic<int> g_fileCounter(0);

// 快速數值限制函式 (取代原本慢速的 round + if check)
inline uint8_t clamp_pixel(int val) {
  if (val < 0)
    return 0;
  if (val > 255)
    return 255;
  return static_cast<uint8_t>(val);
}

/**
 * 極速版 Sobel 濾鏡行處理
 * 優化點：
 * 1. 移除 double 浮點數運算，全部改用 int 整數運算 (Sobel 權重都是整數)。
 * 2. 移除內層 convolution 迴圈，直接展開 (Loop Unrolling)。
 * 3. 移除不必要的陣列 new/delete。
 * 4. 使用指標直接存取像素，減少函式呼叫開銷。
 * * 核心權重 (3x3):
 * 1  0 -1
 * 2  0 -2
 * 1  0 -1
 */
void filtingRowOptimized(bmp::Bitmap &des, const bmp::Bitmap &src, int row) {
  int w = src.width();

  // 預先取得三行的指標 (上、中、下)
  // 使用 const_iterator 或直接指標操作
  auto row_up = src.cbegin() + (row - 1) * w;
  auto row_mid = src.cbegin() + row * w;
  auto row_down = src.cbegin() + (row + 1) * w;

  // 目標行的寫入起點 (從第 1 個 pixel 開始，跳過邊緣)
  auto target = des.begin() + row * w + 1;

  // 針對該行，從左到右掃描 (跳過最左和最右邊緣)
  for (int x = 1; x < w - 1; ++x) {
    // 根據 Sobel 權重展開公式：
    // Val = (左上 - 右上) + 2*(左中 - 右中) + (左下 - 右下)
    // 這裡權重:
    // 上: [x-1]*1, [x]*0, [x+1]*-1
    // 中: [x-1]*2, [x]*0, [x+1]*-2
    // 下: [x-1]*1, [x]*0, [x+1]*-1

    // 預先讀取需要的像素值，減少重複 access
    // R Channel
    int r = (row_up[x - 1].r - row_up[x + 1].r) +
            ((row_mid[x - 1].r - row_mid[x + 1].r) << 1) + // *2 改用位元左移
            (row_down[x - 1].r - row_down[x + 1].r);

    // G Channel
    int g = (row_up[x - 1].g - row_up[x + 1].g) +
            ((row_mid[x - 1].g - row_mid[x + 1].g) << 1) +
            (row_down[x - 1].g - row_down[x + 1].g);

    // B Channel
    int b = (row_up[x - 1].b - row_up[x + 1].b) +
            ((row_mid[x - 1].b - row_mid[x + 1].b) << 1) +
            (row_down[x - 1].b - row_down[x + 1].b);

    // 寫入結果
    target->r = clamp_pixel(r);
    target->g = clamp_pixel(g);
    target->b = clamp_pixel(b);

    // 移動目標指標
    ++target;
  }
}

void worker(char *inputPath, char *outputPath, int totalFiles) {
  char infilename[128];
  char outfilename[128];

  // 在迴圈外宣告 Bitmap 物件，重複利用記憶體，減少 malloc/free 次數
  bmp::Bitmap srcImage;

  while (true) {
    // 搶奪下一個任務編號
    int fileId = g_fileCounter.fetch_add(1);
    if (fileId >= totalFiles) {
      break;
    }

    std::snprintf(infilename, sizeof(infilename), "%s%d.bmp", inputPath,
                  fileId);
    std::snprintf(outfilename, sizeof(outfilename), "%s%d.bmp", outputPath,
                  fileId);

    try {
      srcImage.load(infilename);

      // 如果讀取失敗或圖片為空
      if (srcImage.width() == 0)
        continue;

      bmp::Bitmap desImage(srcImage.width(), srcImage.height());

      // 進行影像處理 (略過上下邊緣)
      for (int r = 1; r < srcImage.height() - 1; ++r) {
        filtingRowOptimized(desImage, srcImage, r);
      }

      desImage.save(outfilename);
    } catch (...) {
      // 忽略錯誤以確保其他執行緒繼續執行
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout << "Usage: " << argv[0]
              << " <SOURCE_BMP> <TARGET_BMP> <AMOUNT_OF_FILE>\n";
    return 0;
  }

  char *inputPath = argv[1];
  char *outputPath = argv[2];
  int amountOfFile = atoi(argv[3]);

  auto beginTime = std::chrono::steady_clock::now();

  // 1. 決定執行緒數量
  // 使用硬體支援的最大併發數，若無法偵測則預設為 4
  unsigned int numThreads = std::thread::hardware_concurrency();
  if (numThreads == 0)
    numThreads = 4;

  // 如果檔案少於核心數，不需要開那麼多執行緒
  if (numThreads > (unsigned int)amountOfFile) {
    numThreads = amountOfFile;
  }

  // 2. 建立並啟動執行緒
  std::vector<std::thread> threads;
  for (unsigned int i = 0; i < numThreads; ++i) {
    threads.emplace_back(worker, inputPath, outputPath, amountOfFile);
  }

  // 3. 等待所有執行緒完成
  for (auto &t : threads) {
    if (t.joinable())
      t.join();
  }

  auto endTime = std::chrono::steady_clock::now();
  double totalTime = std::chrono::duration<double>(endTime - beginTime).count();

  std::cout << "Takes " << totalTime << "secs" << std::endl;

  return 0;
}
