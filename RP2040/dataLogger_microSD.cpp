/**
 * @file main_datalogger.cpp
 * @brief microSDデータロガープログラム (for RP2040)
 * @details
 * 電源ONのたびに新しいフライトログファイル (flight_log_XXX.csv) を作成し、
 * センサーデータを記録します。電源OFFをピン割り込みで検知し、安全にファイルを
 * 閉じることで、データの損失を防ぎますわ。
 * 定期的なフラッシュ処理により、メタデータの欠損リスクも低減しておりますの。
 *
 * @note 対象ボード: Raspberry Pi Pico (RP2040)
 * @note 動作確認環境: earlephilhowerコア
 *
 * @section pin_config ピン設定
 * - VCC: 3.3V
 * - GND: GND
 * - CS: GPIO 22
 * - SCK: GPIO 18
 * - RX (MISO): GPIO 16
 * - TX (MOSI): GPIO 19
 * - 電源監視ピン: GPIO 2 (任意。INPUT_PULLUPを想定)
 *
 * @section functionality 機能概要
 * - 電源ONごとのログファイル自動生成 (例: /flight_log_001.csv)
 * - 20 Hzでのデータサンプリングと記録 (周期は可変)
 * - 電源OFF時の割り込みによる安全なファイルクローズ処理
 * - 定期的なファイルフラッシュによるデータ保護
 */
#include <SPI.h>
#include <SD.h>

//================================================
//== 設定項目
//================================================
// SPIピン設定
#define PIN_SPI_CS   22
#define PIN_SPI_SCK  18
#define PIN_SPI_RX   16
#define PIN_SPI_TX   19

// 電源監視ピン設定 (例: GPIO 2)
// このピンの電圧が下がった(FALLING)ことを検知してシャットダウン処理を開始しますわ
#define PIN_POWER_SENSE 2

// データロギング設定
// サンプリング周波数 (Hz)
const float SAMPLING_FREQUENCY_HZ = 20.0;
// サンプリング周期 (ミリ秒)
const unsigned long SAMPLING_INTERVAL_MS = 1000 / SAMPLING_FREQUENCY_HZ;
// ファイルをフラッシュする周期 (ミリ秒)。不意の電源断によるデータ損失を防ぎますの
const unsigned long FLUSH_INTERVAL_MS = 1000;


//================================================
//== グローバル変数
//================================================
File logFile;
char logFileName[30]; // ログファイル名を格納するグローバル変数

// 割り込みサービスルーチン (ISR) で使用するため、volatileを付けますわ
volatile bool g_powerOffDetected = false;

unsigned long g_lastLogTime = 0;
unsigned long g_lastFlushTime = 0;


//================================================
//== 関数プロトタイプ
//================================================
void findNextLogFileName();
void powerOffISR();
void logData();


//================================================
//== セットアップ関数
//================================================
void setup() {
  Serial.begin(115200);
  // シリアルポートの準備ができるまで待ちますわ。デバッグに必要ですもの
  while (!Serial) {
    delay(10);
  }
  Serial.println("データロガーを起動しますわ。ごきげんよう。");

  // SDカードの初期化
  SPI.setRX(PIN_SPI_RX);
  SPI.setTX(PIN_SPI_TX);
  SPI.setSCK(PIN_SPI_SCK);
  if (!SD.begin(PIN_SPI_CS)) {
    Serial.println("SDカードの初期化に失敗しましたわ。残念ですが、ここで処理を停止します。");
    while (1); // 永久ループ
  }
  Serial.println("SDカードの初期化に成功しましたわ。");

  // 次のログファイル名を決定します
  findNextLogFileName();
  Serial.print("今回のログは '");
  Serial.print(logFileName);
  Serial.println("' に記録しますわ。");

  // ファイルを開き、ヘッダーを書き込みます
  logFile = SD.open(logFileName, FILE_WRITE);
  if (logFile) {
    // CSVヘッダー。記録するデータに合わせて変更してくださいませ
    logFile.println("timestamp_ms,dummy_sensor1,dummy_sensor2");
    logFile.flush(); // ヘッダーをすぐに書き込んでおきますの
    Serial.println("ヘッダーの書き込みに成功しましたわ。記録を開始します。");
  } else {
    Serial.println("ファイルを開けませんでしたわ…。処理を停止します。");
    while (1);
  }

  // 電圧がHIGHからLOWに変化した(FALLING)ら、powerOffISR関数を呼び出しますの
  attachInterrupt(digitalPinToInterrupt(PIN_POWER_SENSE), powerOffISR, FALLING);
  Serial.println("電源監視を開始しましたわ。いつでも電源をお切りになってよろしくてよ。");
}


//================================================
//== メインループ関数
//================================================
void loop() {
  // --- シャットダウン処理 ---
  if (g_powerOffDetected) {
    if (logFile) {
      logFile.close(); // これが一番大事ですわ！
      Serial.println("電源OFFを検知！ ファイルを安全に閉じましたわ。お疲れ様でした。");
    }
    // 割り込みを無効にして、意図しない動作を防ぎます
    detachInterrupt(digitalPinToInterrupt(PIN_POWER_SENSE));
    // 全ての処理を停止
    while (1) {
      delay(100);
    }
  }

  unsigned long currentTime = millis();

  // --- データロギング処理 ---
  if (currentTime - g_lastLogTime >= SAMPLING_INTERVAL_MS) {
    g_lastLogTime = currentTime;
    logData();
  }

  // --- 定期的なフラッシュ処理 ---
  if (currentTime - g_lastFlushTime >= FLUSH_INTERVAL_MS) {
    // loop()関数内のフラッシュ処理部分を差し替え

  // --- 定期的なクローズ・再オープン処理 ---
  if (currentTime - g_lastFlushTime >= FLUSH_INTERVAL_MS) {
    g_lastFlushTime = currentTime;
    if (logFile) {
      // 一度ファイルを閉じて、メタデータを確実に書き込みます
      logFile.close();
      // すぐに追記モードで同じファイルを開き直します
      logFile = SD.open(logFileName, FILE_WRITE);
      if (!logFile) {
        // 再オープンに失敗した場合の処理
        Serial.println("ファイルの再オープンに失敗しましたわ！");
        // ここでエラー処理（例：LEDを点滅させるなど）をすることも考えられます
      }
    }
  }
  }
}


//================================================
//== 各種処理関数
//================================================

/**
 * @brief 次に使用するログファイル名を検索・生成しますわ
 * @details
 * SDカードルートにある "flight_log_XXX.csv" という形式のファイルを検索し、
 * 存在しない最も若い番号で新しいファイル名を生成しますの。
 */
void findNextLogFileName() {
  int fileNumber = 1;
  while (true) {
    // ファイル名を生成 (例: /flight_log_001.csv)
    sprintf(logFileName, "/flight_log_%03d.csv", fileNumber);
    if (!SD.exists(logFileName)) {
      break; // このファイル名はまだ使われていないので、これで決定ですわ
    }
    fileNumber++;
    if (fileNumber > 999) { // 念のため、ファイル数の上限を設けておきますわ
      Serial.println("ログファイルが999を超えましたわ！");
      // 本来はエラー処理をすべきですが、今回は最初のファイル名を使います
      sprintf(logFileName, "/flight_log_001.csv");
      break;
    }
  }
}

/**
 * @brief 電源OFF検知時の割り込みサービスルーチン (ISR) ですの
 * @details
 * ISR内では重い処理は禁物ですわ。フラグを立てるだけにして、
 * 実際の処理はloop()に任せるのがエレガントな作法ですのよ。
 */
void powerOffISR() {
  g_powerOffDetected = true;
}

/**
 * @brief データを生成し、ファイルに記録しますわ
 * @details
 * 将来的には、ここで各種センサーからの値を読み取って記録しますのよ。
 * 今はダミーデータを書き込んでいますわ。
 */
void logData() {
  if (logFile) {
    // --- ↓↓↓ ここにセンサー読み取り処理を実装しますの ↓↓↓ ---
    unsigned long timestamp = millis();
    int dummyValue1 = random(0, 1024); // 例: 10bit ADCの値
    float dummyValue2 = random(0, 1000) / 10.0; // 例: 温度センサーの値
    // --- ↑↑↑ ここまで ---

    // データをCSV形式で書き込みます
    logFile.print(timestamp);
    logFile.print(",");
    logFile.print(dummyValue1);
    logFile.print(",");
    logFile.println(dummyValue2);
  }
}
