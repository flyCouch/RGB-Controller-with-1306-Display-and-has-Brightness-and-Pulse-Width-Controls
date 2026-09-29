/*
 * Project Name: Arduino Nano RGB Controller
 * Compile Date: 2026-09-29
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Mappings as specified
const int redIN     = A0;
const int greenIN   = A1;
const int blueIN    = A2;
const int brightness= A3;
const int pulseWidth= A6;

const int redOUT    = 6;
const int greenOUT  = 5;
const int blueOUT   = 3;

// Timing variables for non-blocking execution
unsigned long previousMicros = 0;
unsigned long previousDisplayMillis = 0;
const unsigned long displayInterval = 100; // Refresh OLED every 100ms
bool pulseState = false;

// Increased deadband factor to 20 for stronger jitter cancellation at endpoints
int readPot(int pin, int outMin, int outMax, int deadband = 20) {
  int raw = analogRead(pin);
  // Apply bottom deadband so turning it all the way down hits absolute zero cleanly
  if (raw <= deadband) return outMin;
  // Apply top deadband so turning it all the way up hits absolute max cleanly
  if (raw >= (1023 - deadband)) return outMax;
  
  // Map the active middle range cleanly with the expanded buffer
  return map(raw, deadband, 1023 - deadband, outMin, outMax);
}

void setup() {
  Serial.begin(115200); 

  // --- START INTERNAL ID SNIPPET ---
  Serial.println(F("\n========================================"));
  Serial.print(F("PROJECT:  Arduino Nano RGB Controller\n"));
  Serial.print(F("FILE:     ")); Serial.println(F(__FILE__));
  Serial.print(F("COMPILED: ")); Serial.print(F(__DATE__));
  Serial.print(F(" | "));      Serial.println(F(__TIME__));
  Serial.println(F("========================================\n"));
  // --- END INTERNAL ID SNIPPET ---

  pinMode(redOUT, OUTPUT);
  pinMode(greenOUT, OUTPUT);
  pinMode(blueOUT, OUTPUT);

  // Initialize SSD1306 display over I2C (Address 0x3C)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.display();
}

void loop() {
  // Read and clean analog inputs using the strengthened deadband helper function
  int rOut       = readPot(redIN, 0, 255, 20);
  int gOut       = readPot(greenIN, 0, 255, 20);
  int bOut       = readPot(blueIN, 0, 255, 20);
  int briPercent = readPot(brightness, 0, 100, 20);
  
  // Raw read for pulse width with matching expanded buffer protection
  int pwValRaw   = analogRead(pulseWidth);
  if (pwValRaw <= 20) pwValRaw = 0;
  if (pwValRaw >= 1003) pwValRaw = 1023;

  float briMultiplier = briPercent / 100.0;

  // Apply Brightness scaling to final color outputs
  int finalRed = rOut * briMultiplier;
  int finalGreen = gOut * briMultiplier;
  int finalBlue = bOut * briMultiplier;

  // Pulse Width Processing
  // 0 to 20 = full steady on
  // 21 to 1023 = log scaled from 0.001s to 1.0s
  bool isPulsing = (pwValRaw > 20);
  float pwSeconds = 0.0;
  
  if (isPulsing) {
    float normalizedPW = (float)(pwValRaw - 21) / 1002.0; // 0.0 to 1.0
    pwSeconds = 0.001 * pow(1000.0, normalizedPW);

    // Calculate symmetrical half-period in microseconds for high speed accuracy
    unsigned long halfPeriodMicros = (unsigned long)(pwSeconds * 500000.0); 
    unsigned long currentMicros = micros();
    
    if (currentMicros - previousMicros >= halfPeriodMicros) {
      previousMicros = currentMicros;
      pulseState = !pulseState;
    }

    if (!pulseState) {
      finalRed = 0;
      finalGreen = 0;
      finalBlue = 0;
    }
  }

  // Write PWM outputs to pins instantly
  analogWrite(redOUT, finalRed);
  analogWrite(greenOUT, finalGreen);
  analogWrite(blueOUT, finalBlue);

  // --- SEPARATE NON-BLOCKING OLED UPDATE ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousDisplayMillis >= displayInterval) {
    previousDisplayMillis = currentMillis;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    // Row 1: R value & BRI value (Y = 0)
    display.setCursor(0, 0);
    display.print(F("R: "));
    display.print(rOut);

    display.setCursor(64, 0);
    display.print(F("BRI: "));
    display.print(briPercent);
    display.print(F("%"));

    // Row 2: G value & PW value (Y = 16)
    display.setCursor(0, 16);
    display.print(F("G: "));
    display.print(gOut);

    display.setCursor(64, 16);
    display.print(F("PW: "));
    if (!isPulsing) {
      display.print(F("Full"));
    } else {
      if (pwSeconds < 0.01) {
        display.print(pwSeconds, 3);
      } else if (pwSeconds < 0.1) {
        display.print(pwSeconds, 3);
      } else {
        display.print(pwSeconds, 2);
      }
    }

    // Row 3: B value (Y = 32)
    display.setCursor(0, 32);
    display.print(F("B: "));
    display.print(bOut);

    // Row 4: LYTTLE reSearch at the bottom (Y = 48)
    display.setCursor(18, 48);
    display.print(F("LYTTLE reSearch"));

    display.display();
  }
}