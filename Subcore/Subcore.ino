/*
 *  Subcore.ino
 */

#ifndef SUBCORE
#error "Core selection is wrong!!"
#endif

#include <MP.h>
#include <MPMutex.h>
MPMutex mutex(MP_MUTEX_ID0);

void setup() {
  // 液晶ディスプレイの設定
  setupLcd();

  // メインコアに起動を通知
  MP.begin();
}


// 各キー（数値）を判定（範囲：C5 ~ C7）
const int Freq_Array[] = {
  508, 538, 570, 604, 640, 678, 718, 761, 806, 854, 905, 959, 
  1016, 1077, 1141, 1209, 1280, 1357, 1437, 1523, 1613, 1709, 1811, 1919, 
  2033, 2154
};

// 音楽コード（3音）
const int ChordList_3_Array[][3] = {
  {0, 4, 7},
  {0, 3, 7},
  {0, 3, 8},
  {0, 4, 8},
  {0, 3, 6},
  {0, 5, 7},
};

// 音楽コード（4音）
const int ChordList_4_Array[][4] = {
  {0, 4, 7, 9},
  {0, 4, 7, 10},
  {0, 4, 6, 10},
  {0, 4, 8, 10},
  {0, 4, 7, 14},
  {0, 4, 7, 11},
  {0, 4, 6, 11},
  {0, 4, 8, 11},
  {0, 3, 7, 9},
  {0, 3, 7, 14},
  {0, 3, 7, 10},
  {0, 3, 7, 11},
  {0, 3, 6, 10},
  {0, 3, 8, 10},
  {0, 5, 7, 10},
  {0, 5, 7, 11},
};


void loop() {
  int ret;
  int8_t msgid;
  float *buff; // メインコアから渡されたデータへのポインター
  bool bNG = false;

  // データを受信
  ret = MP.Recv(&msgid, &buff);
  // データがない場合は何もしない
  if (ret < 0)  return;

  do {
    // MPMutexを取得
    ret = mutex.Trylock();
  } while (ret != 0);  // メインコアの処理が終わるまで待つ

  float maxValue[4];
  float peakFs[4];
  for (int i = 0; i < 4; i++) {
    peakFs[i] = buff[i * 2];
    maxValue[i] = buff[i * 2 + 1];
  }

  int iFreqArrayNum = sizeof(Freq_Array) / sizeof(int);
  
  int iKeyIndex[4] = {-1, -1, -1, -1};
  int iKeyCnt = 0;

  ////
  // 状況に応じて調整
  float fThreshold = 0.2;
  ////
  for (int i = 0; i < 4; i++) {
      // 音量が小さい、または、対象の周波数の範囲外　は除く
      if (maxValue[i] > fThreshold && peakFs[i] > Freq_Array[0] && peakFs[i] < Freq_Array[iFreqArrayNum - 1]) {

        iKeyIndex[i] = 0;
        for (int j = 0; j < iFreqArrayNum - 1; j++) {
          if ((peakFs[i] >= Freq_Array[j]) && (peakFs[i] < Freq_Array[j + 1])) {
            // 各キー（数値）を判定
            iKeyIndex[i] = j;
            break;
          }
        }

        iKeyCnt ++;
      }
  }

  int iRootKeyIndex = -1;
  int iChordIndex = -1;

  // コード表示の対象かを確認
  if (iKeyCnt >= 3) {
    // 小さい順に並び替え
    int tmp;
    for (int i = 0; i < 4; ++i) {
      for (int j = i+1; j < 4; ++j) {
        if (iKeyIndex[i] > iKeyIndex[j]) {
          tmp =  iKeyIndex[i];
          iKeyIndex[i] = iKeyIndex[j];
          iKeyIndex[j] = tmp;
        }
      }
    }
    
    if (iKeyCnt == 3) {
      // 1つ前にずらす
      for (int i = 0; i < 3; i++) {
        iKeyIndex[i] = iKeyIndex[i + 1];
      }
    }
    
    // ルートキーを判定
    iRootKeyIndex = iKeyIndex[0];

    // ルートを0にした配列に変換
    for (int i = 0; i < iKeyCnt; i++) {
      iKeyIndex[i] -= iRootKeyIndex;
    }

    // コードを判別
    if (iKeyCnt == 3) {
      // 3音の場合
//      for (int i = 0; i < sizeof(ChordList_3_Array) / (sizeof(int) * 3); i++) {
      for (int i = 0; i < 6; i++) {
        if (iKeyIndex[0] == ChordList_3_Array[i][0] && iKeyIndex[1] == ChordList_3_Array[i][1] && iKeyIndex[2] == ChordList_3_Array[i][2]) {
          iChordIndex = i;
          break;
        }
      }
      
    } else {
      // 4音の場合
//      for (int i = 0; i < sizeof(ChordList_4_Array) / (sizeof(int) * 4); i++) {
      for (int i = 0; i < 16; i++) {
        if (iKeyIndex[0] == ChordList_4_Array[i][0] && iKeyIndex[1] == ChordList_4_Array[i][1] && iKeyIndex[2] == ChordList_4_Array[i][2] && iKeyIndex[3] == ChordList_4_Array[i][3]) {
          iChordIndex = i;
          break;
        }
      }
    }
  }
  
  // 結果をLCDに表示
  showResultText(peakFs, maxValue, iRootKeyIndex, iKeyIndex, iKeyCnt, iChordIndex);

  // MPMutexをメインコアに渡す
  mutex.Unlock();
}
