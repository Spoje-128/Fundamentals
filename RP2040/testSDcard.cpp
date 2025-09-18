

/**
 * @file main.cpp
 * @brief RP2040 (Raspberry Pi Pico) 用 microSDカード読み書きテストプログラム
 * @details earlephilhowerコアで動作確認済み。SPIインターフェースを使用してmicroSDカードとの
 *          通信を行い、初期化、ディレクトリ表示、ファイル操作のテストを実行する。
 *          参考: https://karakuri-musha.com/inside-technology/arduino-raspberrypi-picow-tips-microsd-readwrite01/
 * 
 * @note 対象ボード: Raspberry Pi Pico (RP2040)
 * @note 動作確認環境: earlephilhowerコア
 * @note 必要ライブラリ: SPI.h, SD.h (デフォで入っている)
 * 
 * @section pin_config ピン設定 SPI0と電源線をSDカードモジュールに接続
 * - VCC: 3.3V
 * - GND: GND
 * - CS (Chip Select): GPIO 22
 * - SCK (Serial Clock): GPIO 18  
 * - RX (MISO): GPIO 16
 * - TX (MOSI): GPIO 19
 * 
 * @section functionality 機能概要
 * - microSDカードの自動初期化
 * - カードタイプの識別・表示
 * - ディレクトリ構造の再帰的表示
 * - テストファイル(mountdata.txt)への書き込み
 * - 5秒間隔での定期実行
 */
#include <SPI.h>
#include <SD.h>

#define PIN_SPI_CS 22
#define PIN_SPI_SCK 18
#define PIN_SPI_RX 16
#define PIN_SPI_TX 19

File file;

bool sdInitialized = false;

/**
 * @brief microSDカードを初期化する
 * @details SPIピンを設定してSDカードの初期化を行う。既に初期化済みの場合は処理をスキップする。
 */
void initializeMicroSD() {
  if (sdInitialized) {
    return; // 既に初期化済み
  }
  
  Serial.println("SDカードを初期化中...");
  
  SPI.setRX(PIN_SPI_RX);
  SPI.setTX(PIN_SPI_TX);
  SPI.setSCK(PIN_SPI_SCK);
  
  if (SD.begin(PIN_SPI_CS)) {
    sdInitialized = true;
    Serial.println("SDカードの初期化に成功しました");
  } else {
    Serial.println("SDカードの初期化に失敗しました");
  }
}

/**
 * @brief ディレクトリの内容を再帰的に表示する
 * @param dir 表示対象のディレクトリファイル
 * @param numTabs インデント用のタブ数（デフォルト: 0）
 */
void printDirectory(File dir, int numTabs = 0) {
  File entry;
  while ((entry = dir.openNextFile())) {
    // インデントを表示
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    
    Serial.print(entry.name());
    
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1); // 再帰的にサブディレクトリを処理
    } else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

/**
 * @brief microSDカードの情報を表示し、テストファイルを操作する
 * @details カードタイプの表示、ディレクトリ一覧の表示、テストファイルへの書き込みを行う
 */
void viewMicroSDInfo() {
  if (!sdInitialized) {
    Serial.println("エラー: SDカードが初期化されていません");
    return;
  }

  // カードタイプを表示
  Serial.print("カードタイプ: ");
  uint8_t cardType = SD.type();
  const char* cardTypes[] = {"SD1", "SD2", "不明", "SDHC/SDXC"};
  Serial.println(cardTypes[cardType < 4 ? cardType : 2]);

  // ディレクトリの内容を一覧表示
  File root = SD.open("/");
  if (root) {
    printDirectory(root);
    root.close();
  } else {
    Serial.println("エラー: ルートディレクトリを開けません");
    return;
  }

  // テストファイルの処理
  const char* testFile = "/mountdata.txt";
  if (SD.exists(testFile)) {
    Serial.println("mountdata.txt が存在します - データを追記中");
    file = SD.open(testFile, FILE_WRITE);
  } else {
    Serial.println("mountdata.txt が存在しません - 新規作成します");
    file = SD.open(testFile, FILE_WRITE);
  }

  if (file) {
    file.println("Hello microSD card!");
    file.close();
    Serial.println("データの書き込みに成功しました");
  } else {
    Serial.println("エラー: ファイルを書き込み用に開けません");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // シリアルポート接続を待つ
  }
  
  initializeMicroSD();
}

void loop() {
  static unsigned long lastViewTime = 0;
  if (sdInitialized) {
    if (millis() - lastViewTime > 5000) {
      viewMicroSDInfo();
      lastViewTime = millis();
    }
    } else {
    Serial.println("SDカードが初期化されていません");
    }
  delay(10);
}
