#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Encoder.h>
#include <SD.h>
#include <SPI.h>
#include <C:\Users\ppacz\Documents\Arduino\libraries\RingBuffer\src\RingBuf.h>
#include <TimerOne.h>

// STEPPER DRIVER PINS
#define DIR_PIN 2
#define STEP_PIN 3
#define EN_PIN 4

// ENCODER PINS
#define ENCODER_A_PIN 15
#define ENCODER_B_PIN 16
#define ENCODER_INDEX_PIN 17

// RESET BUTTON
#define BUTTON_PIN 9

// SD CARD
#define SD_SPI_CS_PIN 10

Encoder encoder(ENCODER_A_PIN, ENCODER_B_PIN);

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8;

struct log_record_t {
  long unsigned millis;
  long steps;
  long position;
};

RingBuf<log_record_t, 2048> ringBuf;

char buf[16];
char sdStatus[16];

volatile long steps = 0;
volatile int dir = 0;
File logFile = NULL;
long lastFlushMillis = 0;
long lastScreenMillis = 0;

void step() {
  steps += dir;
}

void dirChanged() {
  dir = digitalReadFast(EN_PIN) ? 0 : (digitalReadFast(DIR_PIN) ? -1 : 1);
}

void buttonPressed() {
  steps = 0;
  encoder.readAndReset();
}

void log() {
  ringBuf.push({millis(),steps,encoder.read()});
}

void setup(void) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  dirChanged();
  attachInterrupt(STEP_PIN, step, RISING);
  attachInterrupt(DIR_PIN, dirChanged, CHANGE);
  attachInterrupt(BUTTON_PIN, buttonPressed, FALLING);

  if (SD.begin(SD_SPI_CS_PIN)) {
    logFile = SD.open("log.txt", FILE_WRITE);
    if (logFile) {
      snprintf(sdStatus, sizeof(sdStatus), "SD OK");
    } else {
      snprintf(sdStatus, sizeof(sdStatus), "SD FERR");
    }
  } else {
    snprintf(sdStatus, sizeof(sdStatus), "SD ERR");
  }

  Timer1.initialize(1000); //1000us
  Timer1.attachInterrupt(log);

  u8x8.begin();
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);    
}

void loop(void) {
  long currentMillis = millis();
  long currentPosition = encoder.read();
  long currentSteps = steps;

  if (currentMillis > lastScreenMillis + 20) {
    snprintf(buf, sizeof(buf), "STP: %08.3fmm", currentSteps/259.0);
    u8x8.draw1x2String(0, 0, buf);
    snprintf(buf, sizeof(buf), "Raw: %010ld", currentSteps);
    u8x8.drawString(0, 2, buf);
    snprintf(buf, sizeof(buf), "ENC: %08.3fmm", 12.08*3.1415*currentPosition/5120/4);
    u8x8.draw1x2String(0, 4, buf);
    snprintf(buf, sizeof(buf), "Raw: %010ld", currentPosition);
    u8x8.drawString(0, 6, buf);
    snprintf(buf, sizeof(buf), "%s %.1fs", sdStatus, currentMillis/1000.0);
    u8x8.drawString(0, 7, buf);
    lastScreenMillis = currentMillis;
  }

  if (logFile) {
    log_record_t rec;
    while (ringBuf.lockedPop(rec)) {
      logFile.print(rec.millis);
      logFile.print(",");
      logFile.print(rec.steps);
      logFile.print(",");
      logFile.print(rec.position);
      logFile.write("\n");
    }
    if (currentMillis > lastFlushMillis + 1000) {
      logFile.flush();
      lastFlushMillis = currentMillis;
    }
  }
} 