/*
 *  displayUtil.ino
 */

#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#define TFT_DC  9
#define TFT_CS  10

Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC);

// テキストの定義とテキストの表示位置
#define TX 35
#define TY 210

// 画面の幅と高さ
#define GRAPH_WIDTH  (240) 
#define GRAPH_HEIGHT (320) 

void setupLcd() {
  display.begin();

  // ディスプレイ向き　横(3)に設定
  display.setRotation(3);
  // 塗りつぶし
  display.fillRect(0, 0, GRAPH_HEIGHT, GRAPH_WIDTH, ILI9341_GREEN);

  // テキスト描画開始位置
  display.setCursor(TX, TY);
  // テキストの色
  display.setTextColor(ILI9341_BLACK);
  // テキストの大きさ
  display.setTextSize(2);
}


// 音楽のキー
const String Key_Array[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
//const String Key_Array[] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
// 音楽コード
const String Chord_3_Array[] = {"", "m", "m(#5)", "aug", "dim", "sus4"};
const String Chord_4_Array[] = {"6", "7", "7(b5)", "7(#5)", "add9", "M7", "M7(b5)", "M7(#5)", "m6", "madd9", "m7", "mM7", "m7(b5)", "m7(#5)", "7sus4", "M7sus4"};


// 結果をLCDに表示
void showResultText(float *peakFs, float *maxValue, int iRootKeyIndex, int *iKeyIndex, int iKeyCnt, int iChordIndex) {
  if (iKeyCnt == 0) {
    // キーが見つからない
    yield();
    // 塗りつぶし
    display.fillRect(0, 0, GRAPH_HEIGHT, GRAPH_WIDTH, ILI9341_GREEN);
    yield();
    return;
  }

  // テキストの大きさ
  display.setTextSize(2);

  int16_t iPosY = 20;
  ////
  display.fillRect(TX, iPosY, GRAPH_HEIGHT - TX, 40 * 4, ILI9341_GREEN);
  ////
  // ピークの周波数と値
  for (int i = 0; i < iKeyCnt; i++) {
    int16_t iPosY = 20 + 30 * i;
    display.setCursor(TX, iPosY);
    display.println(String(peakFs[i]));
    display.setCursor(TX + 100, iPosY);
    display.println(String(maxValue[i]));
  }

  ////
  display.fillRect(TX, 140, GRAPH_HEIGHT - TX, 40, ILI9341_GREEN);
  ////
  // 各キーのindex
  for (int i = 0; i < iKeyCnt; i++) {
    display.setCursor(TX + 100 + 40 * i, 140);
    if (iKeyIndex[i] >= 0) {
      display.println(Key_Array[iKeyIndex[i] % 12]);
    } else {
      display.println(String(iKeyIndex[i]));
    }
  }

  // Root, Chordのindex
  display.setCursor(TX, 140);
  display.println("(" + String(iRootKeyIndex) + " " + String(iChordIndex) + ")");

  String strKey = "?";
  if (iRootKeyIndex >= 0) {
    strKey = Key_Array[iRootKeyIndex % 12];
  }

  String strChord = "?";
  if (iChordIndex >= 0) {
    if (iKeyCnt == 3) {
      strChord = Chord_3_Array[iChordIndex];
    } else if (iKeyCnt == 4) {
      strChord = Chord_4_Array[iChordIndex];
    }
  }
  
  // テキストの大きさ
  display.setTextSize(4);
  
  ////
  display.fillRect(TX, 180, GRAPH_HEIGHT - TX, 40, ILI9341_GREEN);
  ////
  display.setCursor(TX, 180);
  display.println(strKey + " " + strChord);
}
