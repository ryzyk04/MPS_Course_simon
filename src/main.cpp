// Саймон: временный I2C-модуль 0x11; сид/сложность приходят через wires-шлюз.
//
// Распиновка (Arduino Uno):
//   TM1637  CLK -> D2, DIO -> D4, VCC -> 5V, GND -> GND       (сид)
//   RGB-светодиод Саймона (4 вывода): R -> D3, G -> D5, B -> D6 (каждый через резистор ~220 Ом),
//             общий вывод V -> 5V (общий анод) или GND (общий катод), см. COMMON_ANODE
//   Лента WS2812 на 2 светодиода: DIN -> D7, светодиод [0] = уровень сложности
//   Лента WS2812 на 4 светодиода: DIN -> D8, светодиоды [0..2] = этапы
//   (на DIN каждой ленты резистор ~330 Ом, питание 5V и GND)
//   Кнопки 1..5 -> A1, A2, A3, D9, D10, вторым выводом на GND
//   I2C: A4 SDA, A5 SCL, общий GND с другими Uno, подтяжки к 5V на шлюзе
//   A0 оставить свободным (источник случайности)

#include <Arduino.h>
#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "../../shared/puzzle_i2c.h"

const uint8_t PIN_TM_CLK = 2;
const uint8_t PIN_TM_DIO = 4;
const uint8_t PIN_RGB[3] = {3, 5, 6};  // R, G, B (PWM)
const uint8_t PIN_LEVEL_STRIP = 7;
const uint8_t PIN_STAGE_STRIP = 8;
const uint8_t PIN_BTN[5] = {A1, A2, A3, 9, 10};
PuzzleI2c bus;
bool gameActive = false, stagePending = false;
uint32_t stageReadyAt = 0;

// true: общий вывод V подключён к +5V, false: к GND
const bool COMMON_ANODE = true;

// Яркость RGB-светодиода Саймона: 0 (выкл) .. 255 (максимум)
const uint8_t SIMON_BRIGHTNESS = 30;

enum Color : uint8_t { WHITE, PINK, RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, PURPLE, COLOR_COUNT };
enum Level : uint8_t { EASY, MEDIUM, HARD };

const uint8_t RGB[COLOR_COUNT][3] = {
  {255, 255, 255},  // белый
  {255, 0, 51},     // розовый
  {255, 0, 0},      // красный
  {255, 51, 0},     // оранжевый
  {255, 180, 0},    // жёлтый
  {0, 255, 0},      // зелёный
  {0, 200, 255},    // голубой
  {0, 0, 255},      // синий
  {162, 0, 230},    // фиолетовый
};

// Лёгкий: цвета красный, жёлтый, зелёный, синий, белый; столбцы: последняя цифра чётная / нечётная.
const uint8_t EASY_COLORS[5] = {RED, YELLOW, GREEN, BLUE, WHITE};
const uint8_t EASY_TABLE[COLOR_COUNT][2] = {
  /* WHITE  */ {4, 3}, /* PINK */ {0, 0}, /* RED */ {3, 5}, /* ORANGE */ {0, 0}, /* YELLOW */ {1, 2},
  /* GREEN  */ {5, 4}, /* CYAN */ {0, 0}, /* BLUE */ {2, 1}, /* PURPLE */ {0, 0},
};

// Средний: столбцы A, B, C, D.
const uint8_t MEDIUM_TABLE[COLOR_COUNT][4] = {
  {2, 4, 1, 5}, {5, 1, 3, 2}, {1, 3, 5, 4}, {4, 2, 3, 1}, {3, 5, 2, 4},
  {4, 3, 1, 2}, {1, 5, 4, 3}, {3, 2, 5, 1}, {5, 1, 4, 3},
};

// Сложный: столбцы I..VI.
const uint8_t HARD_TABLE[COLOR_COUNT][6] = {
  {3, 5, 1, 4, 2, 3}, {1, 4, 3, 2, 5, 2}, {4, 2, 5, 1, 3, 4}, {2, 1, 5, 3, 4, 5}, {5, 3, 2, 4, 1, 1},
  {2, 4, 4, 5, 1, 3}, {4, 2, 3, 1, 5, 1}, {5, 5, 2, 3, 4, 2}, {1, 3, 4, 5, 2, 4},
};

const uint8_t SEQ_LEN[3][3] = {{3, 3, 4}, {3, 4, 5}, {4, 5, 6}};
const uint8_t MAX_SEQ = 6;

const uint16_t FLASH_ON_MS = 500;
const uint16_t FLASH_OFF_MS = 250;
const uint16_t SHOW_PAUSE_MS = 2000;
const uint16_t STAGE_DONE_MS = 800;
const uint8_t DEBOUNCE_MS = 30;

TM1637Display display(PIN_TM_CLK, PIN_TM_DIO);
Adafruit_NeoPixel levelStrip(2, PIN_LEVEL_STRIP, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stageStrip(4, PIN_STAGE_STRIP, NEO_GRB + NEO_KHZ800);

uint8_t digits[4];
Level level;
uint8_t column;  // столбец таблицы текущего этапа

uint8_t seq[MAX_SEQ];  // мигающие цвета
uint8_t expected[MAX_SEQ];  // нужные кнопки (1..5)
uint8_t seqLen;
uint8_t stage;  // 0..2, 3 = решено
uint8_t inputPos;

// Состояние показа
enum ShowPhase : uint8_t { SHOW_ON, SHOW_OFF, SHOW_PAUSE };
ShowPhase showPhase;
uint8_t showIdx;
uint32_t showT;

// Антидребезг
bool btnState[5];
bool btnRaw[5];
uint32_t btnT[5];

const uint8_t BLACK[3] = {0, 0, 0};

void setSimon(const uint8_t* c) {
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t v = (uint16_t)c[i] * SIMON_BRIGHTNESS / 255;
    analogWrite(PIN_RGB[i], COMMON_ANODE ? 255 - v : v);
  }
}

bool isPrime(uint8_t d) { return d == 2 || d == 3 || d == 5 || d == 7; }

bool hasRepeats() {
  for (uint8_t i = 0; i < 4; i++)
    for (uint8_t j = i + 1; j < 4; j++)
      if (digits[i] == digits[j]) return true;
  return false;
}

bool hasZero() {
  for (uint8_t i = 0; i < 4; i++)
    if (digits[i] == 0) return true;
  return false;
}

uint8_t digitSum() { return digits[0] + digits[1] + digits[2] + digits[3]; }

bool hasDigit(uint8_t d) {
  for (uint8_t i = 0; i < 4; i++)
    if (digits[i] == d) return true;
  return false;
}

uint8_t oddCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < 4; i++) n += digits[i] % 2;
  return n;
}

bool hasPrime() {
  for (uint8_t i = 0; i < 4; i++)
    if (isPrime(digits[i])) return true;
  return false;
}

// Столбец таблицы для этапа st (0..2): EASY - 0 чётная / 1 нечётная; MEDIUM - A..D = 0..3; HARD - I..VI = 0..5
uint8_t pickColumn(uint8_t st) {
  uint8_t first = digits[0], second = digits[1], third = digits[2], last = digits[3];
  bool sumEven = digitSum() % 2 == 0;
  bool halfBigger = digits[0] + digits[1] > digits[2] + digits[3];
  switch (level) {
    case EASY:
      return last % 2;
    case MEDIUM:
      if (st == 0) return sumEven ? (first > last ? 0 : 1) : (first > last ? 2 : 3);
      if (st == 1) return second % 2 == 0 ? (halfBigger ? 3 : 0) : (halfBigger ? 1 : 2);
      if (third > second) return digitSum() > 18 ? 1 : 2;
      return digitSum() > 18 ? 3 : 0;
    default:
      if (st == 0) {
        if (hasRepeats()) return 3;
        if (digitSum() > 20) return 5;
        if (hasZero()) return 1;
        if (halfBigger) return 4;
        if (isPrime(last)) return 0;
        return 2;
      }
      if (st == 1) {
        if (first > last && sumEven) return 2;
        if (oddCount() == 2) return 0;
        if (isPrime(second)) return 4;
        if (digitSum() < 15) return 5;
        if (hasDigit(9)) return 1;
        return 3;
      }
      if (third > second && last % 2 == 1) return 5;
      if (second + third > 10) return 3;
      if (!hasPrime()) return 0;
      if (first % 2 == 0) return 1;
      if (hasDigit(1)) return 2;
      return 4;
  }
}

uint8_t buttonFor(uint8_t color) {
  switch (level) {
    case EASY: return EASY_TABLE[color][column];
    case MEDIUM: return MEDIUM_TABLE[color][column];
    default: return HARD_TABLE[color][column];
  }
}

const uint16_t ERROR_MS = 500;
const uint16_t STAGE_BLINK_MS = 400;
bool errorActive = false;
uint32_t errorT;
uint32_t lastCurrent = 0xFFFFFFFF;  // последний показанный цвет текущего этапа

uint32_t currentStageColor() {
  if (errorActive) return stageStrip.Color(255, 0, 0);
  bool on = (millis() / STAGE_BLINK_MS) % 2 == 0;
  return on ? stageStrip.Color(255, 180, 0) : 0;
}

void drawProgress() {
  lastCurrent = currentStageColor();
  for (uint8_t i = 0; i < 3; i++) {
    uint32_t c = i < stage ? stageStrip.Color(0, 255, 0) : (i == stage ? lastCurrent : 0);
    stageStrip.setPixelColor(i, c);
  }
  stageStrip.show();
}

void updateStageLed() {
  if (stage >= 3) return;
  if (errorActive && millis() - errorT >= ERROR_MS) errorActive = false;
  if (currentStageColor() != lastCurrent) drawProgress();
}

void startShow() {
  showIdx = 0;
  showPhase = SHOW_ON;
  showT = millis();
  setSimon(RGB[seq[0]]);
}

void startStage() {
  seqLen = SEQ_LEN[level][stage];
  column = pickColumn(stage);
  for (uint8_t i = 0; i < seqLen; i++) {
    seq[i] = (level == EASY) ? EASY_COLORS[random(5)] : random(COLOR_COUNT);
    expected[i] = buttonFor(seq[i]);
  }
  inputPos = 0;
  drawProgress();
  startShow();

  Serial.print(F("Stage "));
  Serial.print(stage + 1);
  Serial.print(F(" col "));
  Serial.print(column);
  Serial.print(F(": "));
  for (uint8_t i = 0; i < seqLen; i++) {
    Serial.print(expected[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void updateShow() {
  uint32_t dt = millis() - showT;
  switch (showPhase) {
    case SHOW_ON:
      if (dt >= FLASH_ON_MS) {
        setSimon(BLACK);
        showPhase = SHOW_OFF;
        showT = millis();
      }
      break;
    case SHOW_OFF:
      if (dt >= FLASH_OFF_MS) {
        if (++showIdx >= seqLen) {
          showPhase = SHOW_PAUSE;
        } else {
          setSimon(RGB[seq[showIdx]]);
          showPhase = SHOW_ON;
        }
        showT = millis();
      }
      break;
    case SHOW_PAUSE:
      if (dt >= SHOW_PAUSE_MS) startShow();
      break;
  }
}

void onPress(uint8_t button) {  // button 1..5
  if (!gameActive || stagePending || stage >= 3) return;
  if (button != expected[inputPos]) {
    bus.smallError();
    inputPos = 0;  // малая ошибка: этап заново
    errorActive = true;
    errorT = millis();
    drawProgress();
    Serial.println(F("Wrong"));
    return;
  }
  if (++inputPos < seqLen) return;

  // Этап пройден
  stage++;
  errorActive = false;
  drawProgress();
  setSimon(BLACK);
  if (stage >= 3) {
    gameActive = false;
    bus.solved();
    Serial.println(F("SOLVED"));
    return;
  }
  stagePending = true;
  stageReadyAt = millis() + STAGE_DONE_MS;
}

void readButtons() {
  for (uint8_t i = 0; i < 5; i++) {
    bool pressed = digitalRead(PIN_BTN[i]) == LOW;
    if (pressed != btnRaw[i]) {
      btnRaw[i] = pressed;
      btnT[i] = millis();
    }
    if (btnRaw[i] == btnState[i] || millis() - btnT[i] < DEBOUNCE_MS) continue;
    btnState[i] = btnRaw[i];
    if (btnState[i]) onPress(i + 1);
  }
}

void beginGame() {
  uint16_t seed = bus.status.seed;
  level = static_cast<Level>(bus.status.difficulty);
  randomSeed(uint32_t(seed) + 1);
  for (int8_t i = 3; i >= 0; --i) { digits[i] = seed % 10; seed /= 10; }
  for (uint8_t i = 0; i < 5; ++i) {
    btnRaw[i] = btnState[i] = digitalRead(PIN_BTN[i]) == LOW;
    btnT[i] = millis();
  }
  display.showNumberDec(digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3], true);
  setSimon(BLACK);
  stageStrip.clear(); levelStrip.clear();
  const uint32_t levelColor[3] = {levelStrip.Color(0, 255, 0), levelStrip.Color(255, 180, 0),
                                  levelStrip.Color(255, 0, 0)};
  levelStrip.setPixelColor(0, levelColor[level]);
  levelStrip.show();

  Serial.print(F("Seed: "));
  for (uint8_t i = 0; i < 4; i++) Serial.print(digits[i]);
  Serial.println();
  Serial.print(F("Level: "));
  Serial.println(level == EASY ? F("easy") : level == MEDIUM ? F("medium") : F("hard"));

  stage = 0;
  errorActive = false; stagePending = false; gameActive = true;
  startStage();
}

void haltGame() {
  gameActive = stagePending = false;
  setSimon(BLACK);
  display.clear();
  stageStrip.clear(); stageStrip.show();
  levelStrip.clear(); levelStrip.show();
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 5; ++i) pinMode(PIN_BTN[i], INPUT_PULLUP);
  for (uint8_t i = 0; i < 3; ++i) pinMode(PIN_RGB[i], OUTPUT);
  display.setBrightness(5);
  stageStrip.begin(); stageStrip.setBrightness(60);
  levelStrip.begin(); levelStrip.setBrightness(60);
  haltGame();
  bus.begin(ModuleBus::SIMON, ModuleBus::SIMON_ADDRESS);
  Serial.println(F("WAITING_FOR_COMMAND: I2C 0x11, SDA=A4 SCL=A5"));
}

void loop() {
  PuzzleI2c::Action action = bus.update();
  if (action == PuzzleI2c::NEW_GAME) beginGame();
  else if (action == PuzzleI2c::HALT) haltGame();
  if (!gameActive) return;
  readButtons();
  if (!gameActive) return;
  if (stagePending) {
    if (int32_t(millis() - stageReadyAt) >= 0) { stagePending = false; startStage(); }
    return;
  }
  if (stage < 3) {
    updateShow();
    updateStageLed();
  }
}
