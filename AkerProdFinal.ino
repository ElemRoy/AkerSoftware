#include <SPI.h>
#include <Arduino.h>
#include <Adafruit_GFX.h>  // Include core graphics library
#include <Adafruit_ST7735.h>  // Include Adafruit_ST7735 library to drive the display
#include <GRGB.h>
#include <FlickerFreePrint.h>
#include <DS3231.h>
#include <OneButton.h>

#include <Fonts/FreeSerif24pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>

#define TFT_CS     10
#define TFT_RST    8  // You can also connect this to the Arduino reset in which case, set this #define pin to -1!
#define TFT_DC     9

#define B_PIN 3
#define G_PIN 6
#define R_PIN 5

#define LEFT A3
#define RIGHT A2
#define UP A7
#define DOWN A1
#define CLICK 12

#define LED_PIN A0
 
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DS3231 rtc(SDA, SCL);

GRGB rgb(COMMON_CATHODE, R_PIN, G_PIN, B_PIN);

FlickerFreePrint<Adafruit_ST7735> Amperage(&tft, 0x0400, 0x0000);
FlickerFreePrint<Adafruit_ST7735> Voltage(&tft, 0xFFFF80, 0x0000);
FlickerFreePrint<Adafruit_ST7735> Watts(&tft, 0x0400, 0x0000);
FlickerFreePrint<Adafruit_ST7735> mWhHour(&tft, 0x867F, 0x0000);
FlickerFreePrint<Adafruit_ST7735> Charge(&tft, 0xFFFF, 0x0000);
FlickerFreePrint<Adafruit_ST7735> BacklightModes(&tft, 0xFFFF, 0x0000);
FlickerFreePrint<Adafruit_ST7735> DateTimeSettings(&tft, 0xFFFF, 0x0000);
FlickerFreePrint<Adafruit_ST7735> ExitArrow(&tft, 0xFFFF, 0x0000);
FlickerFreePrint<Adafruit_ST7735> Step(&tft, 0xFFFF, 0x0000);
FlickerFreePrint<Adafruit_ST7735> Brightness(&tft, 0xFFFF, 0x0000);

OneButton clickBtn = OneButton(
  CLICK,  // Input pin for the button
  false,       // Button is active high
  false        // Disable internal pull-up resistor
);

              // PARAMETERS VARIABLES

const float configR1 = 0.1f; // Om
const float configR3 = 20.0f; // kOm
const float configR5 = 1.0f; // kOm
const float configMinA = 0.1f; // A
const float configRatedVoltage = 3.7f; // V

unsigned int _adcRAW1[3];
unsigned int _adcRAW2[3];
unsigned int _adcTime[3];

float _adcA[3]; // Current
float _adcV[3]; // Voltage

float A[3];
float V[3];
float mWh[3];

float _adcRl[3];      // load resistance
float _adcMWh[3]; // milliwatt hour

bool ledIsOn = false;

          // DEVICE PARAMETERS

int wheelVal = 0;

unsigned char deviceParams[4];

bool flag = false;

short int menuIndex = 0;

int backlightMenuVertical = 0;
int backlightMenuHorizontal = 0;

int mPlexorPorts[3][2];

byte paramsIntervalCounter = 0;

int settingsIndex = 0;

bool chosen = false;

int displayTick = 0;
const int displayUpdateTickCount = 20;

              // INTERFACE FUNCTIONS

void drawText(char *text, int x, int y, uint16_t color, int font, int textSize){
  if(font == 24){
    tft.setFont(&FreeSerif24pt7b);
  }
  else if(font == 9){
    tft.setFont(&FreeSerif9pt7b);
  }
  else if(font == 0){
    tft.setFont();
  }
  
  tft.setCursor(x, y);
  tft.setTextSize(textSize);
  tft.setTextColor(color, ST7735_BLACK);
  tft.print(text);
}

void drawTextLine(char *text, int x, int y, uint16_t color, int font, int textSize){
  if(font == 24){
    tft.setFont(&FreeSerif24pt7b);
  }
  else if(font == 9){
    tft.setFont(&FreeSerif9pt7b);
  }
  else if(font == 0){
    tft.setFont();
  }
  
  tft.setCursor(x, y);
  tft.setTextSize(textSize);
  tft.setTextColor(color, ST7735_BLACK);
  tft.println(text);
}

int getPressCount(){
  int j = 0;
  
  if(digitalRead(LEFT)  == HIGH){ j++; }
  if(digitalRead(RIGHT) == HIGH){ j++; }
  if(clickBtn.isLongPressed()){ j++; }
  if(analogRead(DOWN)  > 500){ j++; }
  if(analogRead(UP)  > 500){ j++; }
  
  return j;
}

char* intToC(int i){
   char* cstr = new char[16];
   itoa(i, cstr, 10);
   return cstr;
}

char* floatToC(float i){
   char* cstr = new char[16];
   dtostrf(i, 4, 2, cstr);
   return cstr;
}

char* combine(char* str1, char* str2){
  char* buff = new char[strlen(str1) + strlen(str2) + 1];
  strcpy(buff, str1);
  strcat(buff, str2);
  return buff;
}

void changeMenu(short int index){
  menuIndex = index;
  tft.fillRect(0, 0, 160, 128, ST7735_BLACK);
  
  _adcA[0] = 0.0f;
  _adcV[0] = 0.0f;
  _adcMWh[0] = 0.0f;

  _adcA[1] = 0.0f;
  _adcV[1] = 0.0f;
  _adcMWh[1] = 0.0f;

  _adcA[2] = 0.0f;
  _adcV[2] = 0.0f;
  _adcMWh[2] = 0.0f;

  paramsIntervalCounter = 0;

  if(index == 0){ drawMainMenu(); drawMainMenuLoop(); }
  if(index == 1){ drawSettingsMenu(); drawSettingsMenuLoop(); }
  if(index == 2 || index == 3 || index == 4) { drawPortMenu(); drawPortMenuLoop(); }
  if(index == 5) { drawBacklightModes(); }
  if(index == 6) { drawDatetime(); }

  displayTick = 0;
}


                // RGB FUNCTIONS

void rgbFun(){
  wheelVal += deviceParams[0];

  if(wheelVal > 1530){
    wheelVal = wheelVal - 1530;
  }

  rgb.setWheel(wheelVal, deviceParams[1] * 10);
  clickBtn.tick();
  delay(20);
}



              // PARAMETERS FUNCTIONS

float getCurrentCharge(){
  unsigned int x = getPortData(6);
  float y = (x-522.0f)*(x-281.27f)*0.000346724;
  if(y > 100) { y = 100; }
  if(y < 0) { y = 0; }
  return floor(y);
}

int getPortData(int port){
  digitalWrite(2, bitRead(port,0));
  digitalWrite(7, bitRead(port,1));
  digitalWrite(4, bitRead(port,2));

  return analogRead(A6);
}

void updatePortParams(int port1, int port2, int index){
  float f;

  _adcRAW1[index] = getPortData(port1);
  f = (float) _adcRAW1[index];
  _adcA[index] += ((5.0f * f) / (1023.0f * configR1));
  _adcRAW2[index] = getPortData(port2);
  f = (float) _adcRAW2[index];
  _adcV[index] += ((4.7f * f) / 1023.0f) * ((configR3 + configR5) / configR5);
  
  paramsIntervalCounter++;
}

void calcPortParams(int port1, int port2, int index){
  _adcA[index] /= 30.0f;
  _adcV[index] /= 30.0f;

  _adcRl[index] = 0.0f;
  if (_adcA[index] > configMinA) {
    _adcTime[index]++;
    _adcRl[index] = _adcV[index] / _adcA[index] - configR1;
    // 60 * 60 / 1000 = 3.6
    _adcMWh[index] += (_adcA[index] * _adcA[index] * _adcRl[index]) / 3.6f;
  }

  A[index] = _adcA[index];
  V[index] = _adcV[index];
  mWh[index] = _adcMWh[index];
   
  _adcA[index] = 0.0f;
  _adcV[index] = 0.0f;
  _adcMWh[index] = 0.0f;

  paramsIntervalCounter = 0;
}

int getPortIndex(){
  if(menuIndex < 2 || menuIndex > 4){ return 0; }
  return menuIndex - 2;
}

int getMenuIndex(int index){
  if(index < 0) { index = 4; }
  if(index > 4) { index = 0; }
  return index;
}

              // BUTTON

static void clickLongClick(){
  if(menuIndex == 0) {
    if(ledIsOn == false) { analogWrite(LED_PIN, 255); ledIsOn = true; } 
    else { analogWrite(LED_PIN, 0); ledIsOn = false; }
  }
}
              

              // SETUP

void setup() {
  pinMode(LEFT, OUTPUT);  
  pinMode(RIGHT, OUTPUT);
  pinMode(UP, INPUT);
  pinMode(DOWN, OUTPUT);
  pinMode(CLICK, OUTPUT);

  pinMode(A6, INPUT);

  pinMode(2,  OUTPUT);  // CD4051 pin 11 (A)
  pinMode(7,  OUTPUT);  // CD4051 pin 10 (B)
  pinMode(4, OUTPUT);  // CD4051 pin 9  (C)

  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  rtc.begin();

  tft.initR(INITR_BLACKTAB);

  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(-45);

  _adcTime[0] = 0;
  _adcTime[1] = 0;
  _adcTime[2] = 0;

  _adcMWh[0] = 0;
  _adcMWh[1] = 0;
  _adcMWh[2] = 0;

  deviceParams[0] = 0;
  deviceParams[1] = 0;
  deviceParams[2] = 0;
  deviceParams[3] = 0;

  backlightMenuVertical = 0; 
  backlightMenuHorizontal = 0; 

  mPlexorPorts[0][0] = 3;
  mPlexorPorts[0][1] = 0;

  mPlexorPorts[1][0] = 1;
  mPlexorPorts[1][1] = 2;

  mPlexorPorts[2][0] = 5;
  mPlexorPorts[2][1] = 4;

  clickBtn.attachLongPressStart(clickLongClick);
  clickBtn.setPressTicks(1000);
  
  drawMainMenu();
}



                // LOOP

void loop() {
  if(flag == true && getPressCount() == 0) { flag = false; }

  /*
  if(menuIndex == 0) 
  { 
    if(displayTick > displayUpdateTickCount) { drawMainMenuLoop(); displayTick = 0; }
    else { drawMainMenuTick(); } 
  }
  
  if(menuIndex == 1) 
  { 
    if(displayTick > displayUpdateTickCount) { drawSettingsMenuLoop(); displayTick = 0; }
    else { drawSettingsMenuTick(); } 
  }
  
  if(menuIndex == 2 || menuIndex == 3 || menuIndex == 4) 
  { 
    if(displayTick > displayUpdateTickCount) { drawPortMenuLoop(); displayTick = 0; }
    else { drawPortMenuTick(); } 
  }
  
  if(menuIndex == 5) 
  { 
    if(displayTick > displayUpdateTickCount) { drawBacklightModes(); displayTick = 0; } 
    else { drawBacklightModesTick(); } 
  }

  if(menuIndex == 6) 
  { 
    drawDatetimeTick(); 
    displayTick = 0;
  }
  */

  // more compact version of the commented code below
  switch(menuIndex){
    case 0: {
      if(displayTick > displayUpdateTickCount) { drawMainMenuLoop(); displayTick = 0; }
      else { drawMainMenuTick(); } 
      break;
    }
    case 1: {
      if(displayTick > displayUpdateTickCount) { drawSettingsMenuLoop(); displayTick = 0; }
      else { drawSettingsMenuTick(); } 
      break;
    }
    case 2: case 3: case 4: {
      if(displayTick > displayUpdateTickCount) { drawPortMenuLoop(); displayTick = 0; }
      else { drawPortMenuTick(); } 
      break;
    }
    case 5: {
      if(displayTick > displayUpdateTickCount) { drawBacklightModes(); displayTick = 0; } 
      else { drawBacklightModesTick(); } 
      break;
    }
    case 6: {
      drawDatetimeTick(); 
      displayTick = 0;
      break;
    }
  }

  displayTick++;
}



                // MAIN MENU

void drawMainMenu(){
  for(int i = 0; i < 2; i++){
    tft.fillRoundRect(20, 61 + 32 * i, 120, 2, 3, 0x07FF);
  }

  for(int i = 0; i < 2; i++){
    tft.fillRect(156 * i, 0, 4, 128, 0x07FF);
  }
  
  drawTextLine(rtc.getTimeStr(1), 20, 18, 0xFFFF80, 0, 4);
  
  drawText(rtc.getDOWStr(), 17, 74, 0x7B35, 0, 1);
  drawText(rtc.getDateStr(), 84, 74, 0x7B35, 0, 1);

  tft.fillRect(12, 100, 136, 30, ST7735_BLACK);

  drawText("Capacity:", 31, 114, 0xF800, 9, 1);
  
  char* number = intToC(getCurrentCharge());
  char* res = combine(number, "%");
  tft.setCursor(105, 114);
  Charge.print(res);
  delete[] number;
  delete[] res;
}

void drawMainMenuTick(){
   updatePortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex());
   if(paramsIntervalCounter == 30) { calcPortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex()); }
   rgbFun();
   
   if(flag == false) 
   {
      if(digitalRead(LEFT) == HIGH){ flag = true; int index = menuIndex - 1; changeMenu(getMenuIndex(index)); }
      if(digitalRead(RIGHT) == HIGH){ flag = true; int index = menuIndex + 1; changeMenu(getMenuIndex(index)); }
   }
}

void drawMainMenuLoop(){
  drawTextLine(rtc.getTimeStr(1), 20, 18, 0xFFFF80, 0, 4);
  
  drawText(rtc.getDOWStr(), 17, 74, 0x7B35, 0, 1);
  drawText(rtc.getDateStr(), 84, 74, 0x7B35, 0, 1);

  drawText("Capacity:", 31, 114, 0xF800, 9, 1);
  
  char* number = intToC(getCurrentCharge());
  char* res = combine(number, "%");
  tft.setCursor(105, 114);
  Charge.print(res);
  delete[] number;
  delete[] res;
}



                  // PORT MENU

void drawPortMenu(){
  tft.fillRoundRect(0, 18, 168, 2, 3, 0x07FF);

  char* number = intToC(menuIndex - 1);
  char* res = combine("Port ", number);
  drawText(res, 57, 40, 0xFFFFFF, 9, 1);
  delete[] number;
  delete[] res;

  drawTextLine(rtc.getTimeStr(1), 20, 6, 0xFFFF80, 0, 1);
  
  drawTextLine("Capacity:", 60, 6, 0xF800, 0, 1);

  char* charge = intToC(getCurrentCharge());
  char* cres = combine(charge, "%");
  tft.setCursor(118, 6);
  Charge.print(cres);
  delete[] number;
  delete[] res;

  
  tft.setFont(&FreeSerif9pt7b);
  tft.setTextSize(0);
  
  char* VValue = floatToC(V[getPortIndex()]);
  char* VRes = combine(VValue, " V");
  tft.setCursor(10, 61);
  Voltage.print(VRes);
  delete[] VValue;
  delete[] VRes;

  char* AValue = floatToC(A[getPortIndex()]);
  char* ARes = combine(AValue, " A");
  tft.setCursor(10, 89);
  Amperage.print(ARes);
  delete[] AValue;
  delete[] ARes;

  char* WValue = floatToC(A[getPortIndex()] * V[getPortIndex()]);
  char* WRes = combine(WValue, " W");
  tft.setCursor(80, 63);
  Watts.print(WRes);
  delete[] WValue;
  delete[] WRes;

  char* mWhValue = floatToC(mWh[getPortIndex()]);
  char* mWhRes = combine(mWhValue, " mWh");
  tft.setCursor(80, 89);
  mWhHour.print(mWhRes);
  delete[] mWhValue;
  delete[] mWhRes;
  
  tft.fillRect(71, 54, 2, 40, 0x07FF);
}

void drawPortMenuTick(){
   updatePortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex());
   if(paramsIntervalCounter == 30) { calcPortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex()); }
   rgbFun();
   
   if(flag == false){
     if(digitalRead(LEFT) == HIGH){ flag = true; int index = menuIndex - 1; changeMenu(getMenuIndex(index)); }
     if(digitalRead(RIGHT) == HIGH){ flag = true; int index = menuIndex + 1; changeMenu(getMenuIndex(index)); }
   }
}

void drawPortMenuLoop(){
  drawTextLine(rtc.getTimeStr(1), 20, 6, 0xFFFF80, 0, 1);
  
  drawTextLine("Capacity:", 60, 6, 0xF800, 0, 1);

  char* number = intToC(getCurrentCharge());
  char* res = combine(number, "%");
  tft.setCursor(118, 6);
  Charge.print(res);
  delete[] number;
  delete[] res;


  tft.setFont(&FreeSerif9pt7b);
  tft.setTextSize(0);
  
  char* VValue = floatToC(V[getPortIndex()]);
  char* VRes = combine(VValue, " V");
  tft.setCursor(10, 61);
  Voltage.print(VRes);
  delete[] VValue;
  delete[] VRes;

  char* AValue = floatToC(A[getPortIndex()]);
  char* ARes = combine(AValue, " A");
  tft.setCursor(10, 89);
  Amperage.print(ARes);
  delete[] AValue;
  delete[] ARes;

  char* WValue = floatToC(A[getPortIndex()] * V[getPortIndex()]);
  char* WRes = combine(WValue, " W");
  tft.setCursor(80, 63);
  Watts.print(WRes);
  delete[] WValue;
  delete[] WRes;

  char* mWhValue = floatToC(mWh[getPortIndex()]);
  char* mWhRes = combine(mWhValue, " mWh");
  tft.setCursor(80, 89);
  mWhHour.print(mWhRes);
  delete[] mWhValue;
  delete[] mWhRes;
  
  tft.fillRect(71, 54, 2, 40, 0x07FF);
}



                // SETTINGS MENU

void drawSettingsMenu(){
  tft.fillRoundRect(0, 18, 168, 2, 3, 0x07FF);

  drawText("Settings", 49, 40, 0xFFFFFF, 9, 1);

  tft.fillRect(0, 0, 160, 18, ST7735_BLACK);

  drawTextLine(rtc.getTimeStr(1), 20, 6, 0xFFFF80, 0, 1);

  drawText("Capacity:", 60, 6, 0xF800, 0, 1);

  char* number = intToC(getCurrentCharge());
  char* res = combine(number, "%");
  tft.setCursor(118, 6);
  Charge.print(res);
  delete[] number;
  delete[] res;

  tft.setCursor(15, 62);
  tft.setFont(&FreeSerif9pt7b);
  tft.setTextSize(0);
  if(settingsIndex == 0) { BacklightModes.setTextColor(0xF800, ST7735_BLACK); }
  else { BacklightModes.setTextColor(0xFFFF, ST7735_BLACK); }
  BacklightModes.print("Backlight modes");

  tft.setCursor(15, 92);
  if(settingsIndex == 1) { DateTimeSettings.setTextColor(0xF800, ST7735_BLACK); }
  else { DateTimeSettings.setTextColor(0xFFFF, ST7735_BLACK); }
  DateTimeSettings.print("Set date, time");
}

void drawSettingsMenuTick(){
   updatePortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex());
   if(paramsIntervalCounter == 30) { calcPortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex()); }
   rgbFun();
   
   if(flag == false){
     if(digitalRead(LEFT) == HIGH){ flag = true;  int index = menuIndex - 1; changeMenu(getMenuIndex(index)); }
     if(digitalRead(RIGHT) == HIGH){ flag = true; int index = menuIndex + 1; changeMenu(getMenuIndex(index)); } 
     if(analogRead(UP)  > 500) 
     { 
        flag = true; 
        settingsIndex -= 1;
        if(settingsIndex < 0) { settingsIndex = 1;}
        displayTick = 40;
      }
     if(analogRead(DOWN) > 500) 
     { 
        flag = true; 
        settingsIndex = (settingsIndex + 1) % 2; 
        displayTick = 40;
     }
     if(clickBtn.isLongPressed()) 
     { 
        flag = true; 
        changeMenu(settingsIndex + 5);
     }
     
   }
}

void drawSettingsMenuLoop(){
  drawTextLine(rtc.getTimeStr(1), 20, 6, 0xFFFF80, 0, 1);

  drawText("Capacity:", 60, 6, 0xF800, 0, 1);

  char* number = intToC(getCurrentCharge());
  char* res = combine(number, "%");
  tft.setCursor(118, 6);
  Charge.print(res);
  delete[] number;
  delete[] res;

  tft.setCursor(15, 62);
  tft.setFont(&FreeSerif9pt7b);
  tft.setTextSize(0);
  if(settingsIndex == 0) { BacklightModes.setTextColor(0xF800, ST7735_BLACK); }
  else { BacklightModes.setTextColor(0xFFFF, ST7735_BLACK); }
  BacklightModes.print("Backlight modes");

  tft.setCursor(15, 92);
  if(settingsIndex == 1) { DateTimeSettings.setTextColor(0xF800, ST7735_BLACK); }
  else { DateTimeSettings.setTextColor(0xFFFF, ST7735_BLACK); }
  DateTimeSettings.print("Set date, time");
}



                  // BACKLIGHT MODES

void drawBacklightModesTick(){
   updatePortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex());
   if(paramsIntervalCounter == 30) { calcPortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex()); }
   rgbFun();
   
   if(flag == false)
   { 
     if(digitalRead(LEFT) == HIGH)
     {
        flag = true;  
        if(chosen == false){
         backlightMenuHorizontal -= 1;
         if(backlightMenuHorizontal == 0){ backlightMenuVertical = 0; }
         else{
            backlightMenuHorizontal = 1;
         }
        }
        displayTick = 40; 
        
      }
     if(digitalRead(RIGHT) == HIGH)
     { 
        flag = true; 
        if(chosen == false){
         backlightMenuHorizontal += 1;
         if(backlightMenuHorizontal > 1){ backlightMenuHorizontal = 0; backlightMenuVertical = 0; }
        }
        displayTick = 40;
        
      } 
     if(analogRead(UP)  > 500) 
     { 
        flag = true; 
        displayTick = 40;
        
        if(chosen == false){
          if(backlightMenuVertical == 0 && backlightMenuHorizontal == 0) {  }
          else 
          { 
            backlightMenuVertical -= 1;
            if(backlightMenuVertical < 0) { backlightMenuVertical = 1;}
          }
        }
        else{
          deviceParams[backlightMenuVertical]++;
          if(deviceParams[backlightMenuVertical] > 10) { deviceParams[backlightMenuVertical] = 0; }
        }
        
     }
     if(analogRead(DOWN) > 500) 
     { 
        flag = true; 
        displayTick = 40;
        
        if(chosen == false){
          if(backlightMenuVertical == 0 && backlightMenuHorizontal == 0) {  }
          else { backlightMenuVertical = (backlightMenuVertical + 1) % 2; }
        }
        else{
          if(deviceParams[backlightMenuVertical] - 1 < 0) { deviceParams[backlightMenuVertical] = 10; }
          else { deviceParams[backlightMenuVertical]--; } 
        }
        
     }
     if(clickBtn.isLongPressed()) 
     { 
        if(backlightMenuVertical == 0 && backlightMenuHorizontal == 0){
            flag = true; 
            changeMenu(1);
        }
        else{
          flag = true; 
          chosen = !chosen;
          displayTick = 40;
        }
     }
  }
}

void drawBacklightModes(){
  char a[4][8];

  for(int i = 0; i < 2; i++){
    itoa(deviceParams[i], a[i], 10);
  }
  
  tft.setCursor(15, 30);
  tft.setFont(&FreeSerif9pt7b);
  tft.setTextSize(0);
  ExitArrow.setTextColor(0xFFFF, ST7735_BLACK);;
  if(backlightMenuVertical == 0 && backlightMenuHorizontal == 0) { ExitArrow.setTextColor(0xF800, ST7735_BLACK); }
  ExitArrow.print("<");

  char* stepRes = combine("Step: ", a[0]);
  tft.setCursor(39, 32);
  Step.setTextColor(0xFFFF, ST7735_BLACK);;
  if(backlightMenuVertical == 0 && backlightMenuHorizontal == 1) { Step.setTextColor(0xF800, ST7735_BLACK); }
  if(backlightMenuVertical == 0 && backlightMenuHorizontal == 1 && chosen == true) { Step.setTextColor(0x0400, ST7735_BLACK); }
  Step.print(stepRes);
  delete[] stepRes;
  
  char* brightnessRes = combine("Brightness: ", a[1]);
  tft.setCursor(39, 62);
  Brightness.setTextColor(0xFFFF, ST7735_BLACK);;
  if(backlightMenuVertical == 1 && backlightMenuHorizontal == 1) { Brightness.setTextColor(0xF800, ST7735_BLACK); }
  if(backlightMenuVertical == 1 && backlightMenuHorizontal == 1 && chosen == true) { Brightness.setTextColor(0x0400, ST7735_BLACK); }
  Brightness.print(brightnessRes);
  delete[] brightnessRes;
}


void drawDatetimeTick(){
   updatePortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex());
   if(paramsIntervalCounter == 30) { calcPortParams(mPlexorPorts[getPortIndex()][0], mPlexorPorts[getPortIndex()][1], getPortIndex()); }
   rgbFun();
   
   if(flag == false)
   { 
     if(clickBtn.isLongPressed()) 
     { 
        flag = true; 
        changeMenu(1);
     }
  }
}

void drawDatetime(){
  tft.setCursor(15, 30);
  tft.setFont(&FreeSerif9pt7b);
  tft.setTextSize(0);
  ExitArrow.setTextColor(0xF800, ST7735_BLACK);
  ExitArrow.print("<");

  drawText("Currently", 49, 30, 0xFFFFFF, 9, 1);
  drawText("unavailable", 49, 50, 0xFFFFFF, 9, 1);
}
