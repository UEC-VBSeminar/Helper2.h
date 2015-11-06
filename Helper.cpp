#include <Wire.h> // I2C用
#include <ADXL345.h> // 加速度センサ用
#include <TimerOne.h> //timer1 //timer0はdelay、timer2はtoneで使われてる(´・ω・｀)
// #include "dtmtdatas.h"

#include "Helper.h"

// val の値を min と max の値に収まるようにする
// val < min         :=> min
// min <= val <= max :=> val
// max < val         :=> max
int clamp(int val, int min, int max) {
  if (val < min) { return min; }
  if (max < val) { return max; }

  return val;
}

/****************
 * 基板の向きと,ドトマトの向きと,センサの向きがバラバラなので
 * うまく整合させていきたい
 */
// ドットマトリクス
//完成版基板用
#define COL1A 14
#define COL2A 2
#define COL3A 3
#define COL4A 9
#define COL5A 5
#define COL6A 10
#define COL7A 16
#define COL8A 17
#define ROW1K 8
#define ROW2K 15
#define ROW3K 7
#define ROW4K 11
#define ROW5K 1
#define ROW6K 6
#define ROW7K 0
#define ROW8K 4
#define BUZZER 12

// ドットマトリクスの配線
const int rowpin[8] = { ROW1K, ROW2K, ROW3K, ROW4K, ROW5K, ROW6K, ROW7K, ROW8K };
const int colpin[8] = { COL1A, COL2A, COL3A, COL4A, COL5A, COL6A, COL7A, COL8A };

// I2C
#define ADXL345_ID 0x1D
#define APDS9960_ID 0x39

// 加速度センサ
ADXL345 adxl;

void initialize() {
  // printf("init\n");

  // 加速度センサ初期化
  sendi2c(ADXL345_ID, 0x2C, B00001100);//3200Hz書き出し
  sendi2c(ADXL345_ID, 0x31, B00001000);//fullresmode
  initializeAccelerometer();

  // ピンの出力設定
  for(int i=0; i<18; i++) {//digital 0-13,analog 0-3
    pinMode(i, OUTPUT);
  }

  // 出力の初期値すべて消灯
  for(int i=0; i<8; i++) {
    digitalWrite(rowpin[i], LOW);
    digitalWrite(colpin[i], HIGH);
  }

  // タイマー1
  //割り込み周期[usec]//887@16MH動作//8Mhz動作なら単純に考えて倍
  Timer1.initialize(6000);
  // showdtmt の処理にかかる時間以上(約890us)を設定しよう
  // すごいおそい
  Timer1.attachInterrupt(interrupt); //割り込みする関数

  // debug用
  // Serial.begin(9600);
  // Serial.println("started");

  delay(1000);//初期状態確認用
}

void updateData() {
  int count = 20, sumx = 0, sumy = 0, sumz = 0;
  int rawx = 0, rawy = 0, rawz = 0;
  // 水準器
  for(int i=0; i<count; i++){
    adxl.readAccel(&rawx, &rawy, &rawz);
    sumx += rawx;
    sumy += rawy;
    sumz += rawz;
  }

  // sum(x, y, z) は -10240 - 10240 をとる?
  // 1G で x, y: 4900, z: 4500 ぐらいの値をとる => (x, y は 610, z は 560 で割って clamp して) -8 - 8 に縮める
  // by kyontan
  // NOTE: 割る値 は適宜調整してください
  int8_t x = clamp(-sumx / 610, -8, 8);
  int8_t y = clamp(-sumy / 610, -8, 8);
  int8_t z = clamp(-sumz / 560, -8, 8);

  Accel &a = Accel::getInstance();
  // a.x = x;
  // a.y = y;
  // a.z = z;
  a.updateAccel(&x, &y, &z);
};

// timer1割り込みで走る関数
void interrupt() {
  DotMatrix::show();
};

// I2C通信
void sendi2c(byte id, byte reg, byte data) {
  Wire.beginTransmission(id); // Adxl345 のアドレス: 0x1D
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
};

void initializeAccelerometer() {
  adxl.powerOn();
  adxl.setRangeSetting(2); //測定範囲
  // 動作した or してないの閾値を設定 (0-255)
  adxl.setActivityThreshold(75);    //値:*62.5[mg]
  adxl.setInactivityThreshold(75);  //値:*62.5[mg]
  adxl.setTimeInactivity(10);       //非動作の判定までに要する時間//値:*5[ms]

  // 動作したかを監視する軸の設定 (1 == on; 0 == off)
  //各軸の判定の論理和
  adxl.setActivityX(1);
  adxl.setActivityY(1);
  adxl.setActivityZ(1);

  // 動作してないを監視する軸の設定 (1 == on; 0 == off)
  //各軸の判定の論理積
  adxl.setInactivityX(1);
  adxl.setInactivityY(1);
  adxl.setInactivityZ(1);

  // タップされたことを検視する軸の設定 (1 == on; 0 == off)
  adxl.setTapDetectionOnX(0);
  adxl.setTapDetectionOnY(0);
  adxl.setTapDetectionOnZ(0);

  // タップ,ダブルタップに関数る閾値の設定 (0-255)
  adxl.setTapThreshold(80);         //値:*62.5[mg]
  adxl.setTapDuration(15);          //値:*625[μs]
  adxl.setDoubleTapLatency(80);     //値:*1.25[ms]
  adxl.setDoubleTapWindow(200);     //値:*1.25[ms]

  // 自由落下に関する閾値の設定 (0-255)
  //閾値と時間の論理積
  adxl.setFreeFallThreshold(0x09);  //閾値//(0x05 - 0x09) 推薦 - 値:*62.5[mg]
  adxl.setFreeFallDuration(0x0A);   //時間//(0x14 - 0.46) 推薦 - 値:*5[ms]

  //setting all interupts to take place on int pin 1
  //I had issues with int pin 2, was unable to reset it
  adxl.setInterruptMapping( ADXL345_INT_SINGLE_TAP_BIT,   ADXL345_INT1_PIN );
  adxl.setInterruptMapping( ADXL345_INT_DOUBLE_TAP_BIT,   ADXL345_INT1_PIN );
  adxl.setInterruptMapping( ADXL345_INT_FREE_FALL_BIT,    ADXL345_INT1_PIN );
  adxl.setInterruptMapping( ADXL345_INT_ACTIVITY_BIT,     ADXL345_INT1_PIN );
  adxl.setInterruptMapping( ADXL345_INT_INACTIVITY_BIT,   ADXL345_INT1_PIN );

  //register interupt actions - 1 == on; 0 == off
  adxl.setInterrupt( ADXL345_INT_SINGLE_TAP_BIT, 1);
  adxl.setInterrupt( ADXL345_INT_DOUBLE_TAP_BIT, 1);
  adxl.setInterrupt( ADXL345_INT_FREE_FALL_BIT,  1);
  adxl.setInterrupt( ADXL345_INT_ACTIVITY_BIT,   1);
  adxl.setInterrupt( ADXL345_INT_INACTIVITY_BIT, 1);
}
