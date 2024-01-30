/*
 *  Maincore.ino
 */

#ifdef SUBCORE
#error "Core selection is wrong!!"
#endif

#include <Audio.h>
#include <FFT.h>
#include <float.h> // FLT_MAX, FLT_MIN

#define FFT_LEN 1024

// モノラル、1024サンプルでFFTを初期化
FFTClass<AS_CHANNEL_MONO, FFT_LEN> FFT;

AudioClass *theAudio = AudioClass::getInstance();

#include <MP.h>
#include <MPMutex.h>
MPMutex mutex(MP_MUTEX_ID0);
const int subcore = 1;

void avgFilter(float dst[FFT_LEN]) {
  static const int avg_filter_num = 8;  
  static float pAvg[avg_filter_num][FFT_LEN/8];
  static int g_counter = 0;
  if (g_counter == avg_filter_num) g_counter = 0;
  for (int i = 0; i < FFT_LEN/8; ++i) {
    pAvg[g_counter][i] = dst[i];
    float sum = 0;
    for (int j = 0; j < avg_filter_num; ++j) {
      sum += pAvg[j][i];
    }
    dst[i] = sum / avg_filter_num;
  }
  ++g_counter;
}


void setup() {
  Serial.begin(115200);
  
  // ハミング窓、モノラル、オーバーラップ50%
  FFT.begin(WindowHamming, 1, (FFT_LEN/4));

  // 入力をマイクに設定  
  Serial.println("Init Audio Recorder");
  theAudio->begin();
  theAudio->setRecorderMode(AS_SETRECDR_STS_INPUTDEVICE_MIC);
  // 録音設定：フォーマットはPCM (16ビットRAWデータ)、
  // DSPコーデックの場所の指定 (SDカード上のBINディレクトリ)、
  // サンプリグレート 48000Hz、モノラル入力  
  int err = theAudio->initRecorder(AS_CODECTYPE_PCM, "/mnt/sd0/BIN", AS_SAMPLINGRATE_48000 ,AS_CHANNEL_MONO);                             
  if (err != AUDIOLIB_ECODE_OK) {
    Serial.println("Recorder initialize error");
    while(1);
  }

  // 録音開始
  Serial.println("Start Recorder");
  theAudio->startRecorder();
 
  // サブコア起動
  Serial.println("Subcore start");
  MP.begin(subcore); 
}


void loop() {
  static const uint32_t buffering_time = FFT_LEN * 1000 / AS_SAMPLINGRATE_48000;
  static const uint32_t buffer_size = FFT_LEN * sizeof(int16_t);
  static char buff[buffer_size];
  static float pDst[FFT_LEN];
  uint32_t read_size;
    
  int ret = theAudio->readFrames(buff, buffer_size, &read_size);
  if (ret != AUDIOLIB_ECODE_OK && ret != AUDIOLIB_ECODE_INSUFFICIENT_BUFFER_AREA) {
    Serial.println("Error err = " + String(ret));
    theAudio->stopRecorder();
    exit(1);
  }
  
  if (read_size < buffer_size) {
    delay(buffering_time);
    return;
  } 

  // FFTを実行
  FFT.put((q15_t*)buff, FFT_LEN);
  // FFT演算結果を取得
  FFT.get(pDst, 0);

  // 過去のFFT演算結果で平滑化
  avgFilter(pDst);

  // 周波数スペクトルの最大値最小値を検出
  float fpmax = FLT_MIN;
  float fpmin = FLT_MAX;
  for (int i = 0; i < FFT_LEN/8; ++i) {
    if (pDst[i] < 0.0) pDst[i] = 0.0;  
    if (pDst[i] > fpmax) fpmax = pDst[i];
    if (pDst[i] < fpmin) fpmin = pDst[i];
  }

  ////////////////////////////////

  // 周波数のピーク値を上位4つまで取得
  float maxValue[4] = {0.0, 0.0, 0.0, 0.0};
  float peakFs[4] = {0.0, 0.0, 0.0, 0.0};
  int ret_peak = get_peak_frequency_list(pDst, peakFs, maxValue);

  // Subcoreに渡すデータ
  float dataForSubcore[8];
  for (int i = 0; i < 4; i++) {
    dataForSubcore[i * 2] = peakFs[i];
    dataForSubcore[i * 2 + 1] = maxValue[i];
  }

  ////////////////////////////////
  
  // サブコアへのデータ転送処理
  static const int8_t snd_ok = 100;
  static const int8_t snd_ng = 101;
  static float data[8];
  int8_t sndid = snd_ok;
  
  // サブコアが処理中の場合（retが負）データ転送しない
  ret = mutex.Trylock();
  if (ret != 0) return;

  // データをコピー
//  memcpy(data, pDst, disp_samples * sizeof(float));
  memcpy(data, dataForSubcore, 8 * sizeof(float));
  
  // Autoencoder の出力を見たい場合はこちらを有効にしてください
  //memcpy(data, result, disp_samples*sizeof(float));
  
  // サブコアにデータ転送
  ret = MP.Send(sndid, &data, subcore); 
  if (ret < 0) Serial.println("MP.Send Error");
  
  mutex.Unlock(); //サブコアにMutexを渡す
}


// 周波数のピーク値を上位4つまで取得
int get_peak_frequency_list(float *pData, float *peakFs, float *maxValue) {
  uint32_t idx;
  float delta, delta_spr;

  // 周波数分解能(delta)を算出
  const float delta_f = AS_SAMPLINGRATE_48000 / FFT_LEN;

  const int Max_Key_Num = 4;
  for (int i = 0; i < Max_Key_Num; i++) {
    if (i > 0) {
      pData[idx-1] = 0.0;
      pData[idx] = 0.0;
      pData[idx+1] = 0.0;
    }

    // 最大値と最大値のインデックスを取得
//    arm_max_f32(pData, FFT_LEN/2, maxValue, &idx);
    float maxValueTmp = 0.0;
    arm_max_f32(pData, FFT_LEN/2, &maxValueTmp, &idx);
    if (idx < 1) return i;

    float data_sub = pData[idx-1] - pData[idx+1];
    float data_add = pData[idx-1] + pData[idx+1];
    
    // 周波数のピークの近似値を算出
//    delta = 0.5*(pData[idx-1] - pData[idx+1]) / (pData[idx-1] + pData[idx+1] - (2.*pData[idx]));
    delta = 0.5 * data_sub / (data_add - (2.0 * pData[idx]));
    peakFs[i] = (idx + delta) * delta_f;
    
    // スペクトルの最大値の近似値を算出
    delta_spr = 0.125 * data_sub * data_sub / (2.0 * pData[idx] - data_add);
    maxValue[i] = maxValueTmp + delta_spr;
  }
  
  return Max_Key_Num;
}
