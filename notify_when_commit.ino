#include "config3.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFiMulti.h>
#include "pitches.h"
#include "Namespace.h"

// Add config in config.h
const char wifi_ssid[] = WIFI_SSID;
const char wifi_password[] = WIFI_PASSWORD;

ESP8266WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 10000;

const char endpoint[] = ENDPOINT;
const String token = TOKEN;

String saved_sha = "";

WiFiClientSecure client;
HTTPClient httpsClient;

StaticJsonDocument<16> filter_sha;

const uint8_t BUZZER = 4;
const uint8_t BUTTON = 14;
const uint8_t WIFI_LED = 5;
const uint8_t COMMIT_LED_PROD = 15;
const uint8_t COMMIT_LED_TEST = 13;
const uint8_t COMMIT_LED_SYSTEST = 12;

bool isMuted = false;
int buttonState;
int lastButtonState = LOW;

long lastDebounceTime = 0;
long debounceDelay = 50;


// wifi status
int status = WL_IDLE_STATUS;

int clockPin = 3;
int latchPin = 0;
int dataPin = 16;
int num[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67};

int commitCount = 0;

void setup()
{
  Serial.begin(9600);

  // turn off build in led
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT);
  pinMode(WIFI_LED, OUTPUT);
  pinMode(COMMIT_LED_PROD, OUTPUT);
  pinMode(COMMIT_LED_TEST, OUTPUT);
  pinMode(COMMIT_LED_SYSTEST, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  setSevenSegment(0);

  WiFi.mode(WIFI_STA);

  wifiMulti.addAP(wifi_ssid, wifi_password);
  filter_sha["sha"] = true;
  client.setInsecure();
}

unsigned long previousMillis = 0;
const long interval = 5000;

void loop() {
  unsigned long currentMillis = millis();

  checkButtonState();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (WiFi.status() != WL_CONNECTED) {
      if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
        Serial.print("WiFi connected: ");
        Serial.print(WiFi.SSID());
        Serial.print(" ");
        Serial.println(WiFi.localIP());
        digitalWrite(WIFI_LED, HIGH);
      } else {
        Serial.println("WiFi not connected!");
      }
    }

    handleAction(whatNamespace(checkRepo()));
  }
}

void setSevenSegment(int number) {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, num[number]);
  digitalWrite(latchPin, HIGH);
}

void checkButtonState() {
  int reading = digitalRead(BUTTON);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == HIGH) {
        playPRODSound();
      }
    }
  }
  lastButtonState = reading;
}

String checkRepo() {
  blinkWifi();
  httpsClient.useHTTP10(true);
  httpsClient.begin(client, endpoint);
  httpsClient.addHeader("Authorization", "token " + token);
  int httpCode = httpsClient.GET();
  StaticJsonDocument<200> filter;
  filter["sha"] = true;
  filter["files"] = true;
  DynamicJsonDocument doc(6144);
  DeserializationError error = deserializeJson(doc, httpsClient.getStream(), DeserializationOption::Filter(filter));

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    slowFlashLed(COMMIT_LED_TEST);
    return "<error>";
  }

  String sha = doc["sha"].as<String>();
  String filename = doc["files"][0]["filename"].as<String>();
  httpsClient.end();

  if (checkForCommits(sha)) {
    Serial.println(filename);
    return filename;
  } else {
    return "no commit";
  }
}

void handleAction(Namespace nameSpace) {
  if (nameSpace == TEST) {
    playTESTSound();
    flashLed(COMMIT_LED_TEST);
  }
  if (nameSpace == PROD) {
    playPRODSound();
    flashLed(COMMIT_LED_PROD);
  }
  if (nameSpace == SYSTEST) {
    playSYSTESTSound();
    flashLed(COMMIT_LED_SYSTEST);
  }
  if (nameSpace == NONE) {
    Serial.println("Do nothing!");
  }
}
void flashLed(int led) {
  for (int i = 0; i < 10; i++) {
    digitalWrite(led, HIGH);
    delay(150);
    digitalWrite(led, LOW);
    delay(150);
  }
}

void slowFlashLed(int led) {
  for (int i = 0; i < 3; i++) {
    digitalWrite(led, HIGH);
    delay(1000);
    digitalWrite(led, LOW);
    delay(1000);
  }
}

void blinkWifi() {
  digitalWrite(WIFI_LED, LOW);
  delay(150);
  digitalWrite(WIFI_LED, HIGH);
  delay(150);
}


void playTESTSound() {
  tone(BUZZER, NOTE_E4);
  delay(150);
  tone(BUZZER, NOTE_G4);
  delay(150);
  tone(BUZZER, NOTE_E5);
  delay(150);
  tone(BUZZER, NOTE_C5);
  delay(150);
  tone(BUZZER, NOTE_D5);
  delay(150);
  tone(BUZZER, NOTE_G5);
  delay(150);
  noTone(BUZZER);
}

void playSYSTESTSound() {
  tone(BUZZER, NOTE_E4);
  delay(150);
  tone(BUZZER, NOTE_G4);
  delay(150);
  tone(BUZZER, NOTE_E5);
  delay(150);
  noTone(BUZZER);
}

boolean checkForCommits(String sha) {
  if (saved_sha.equals("") && sha.length() > 5) {
    Serial.println("Storing sha: " + sha);
    saved_sha = sha;
  } else {

    Serial.println("Saved_sha is: " + saved_sha);
    if (sha.length() > 5 && sha != saved_sha) {
      Serial.println("New sha detected: " + sha);
      saved_sha = sha;

      if (commitCount > 9) {
        commitCount = 0;
      }
      commitCount++;
      setSevenSegment(commitCount);

      return true;
    }
    return false;
  }
}

void toggleMute() {
  isMuted = !isMuted;
}

Namespace whatNamespace(String filename) {
  if (filename.indexOf("minid/test") >= 0) {
    return TEST;
  }
  if (filename.indexOf("minid/prod") >= 0) {
    return PROD;
  }
  if (filename.indexOf("minid/systest") >= 0) {
    return SYSTEST;
  }
  if (filename.indexOf("no commit") >= 0) {
    return NONE;
  }
}

int tempo = 80;
int melody[] = {

  NOTE_D4, -8, NOTE_G4, 16, NOTE_C5, -4,
  NOTE_B4, 8, NOTE_G4, -16, NOTE_E4, -16, NOTE_A4, -16,
  NOTE_D5, 2,

};

int notes = sizeof(melody) / sizeof(melody[0]) / 2;
int wholenote = (60000 * 4) / tempo;
int divider = 0, noteDuration = 0;

void playPRODSound() {
  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {

    // calculates the duration of each note
    divider = melody[thisNote + 1];
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }

    tone(BUZZER, melody[thisNote], noteDuration * 0.9);
    delay(noteDuration);
    noTone(BUZZER);
  }
}

void playBUZZERSound() {
  int melody[] = {


    NOTE_E5, 4,  NOTE_B4, 8,  NOTE_C5, 8,  NOTE_D5, 4,  NOTE_C5, 8,  NOTE_B4, 8,
    NOTE_A4, 4,  NOTE_A4, 8,  NOTE_C5, 8,  NOTE_E5, 4,  NOTE_D5, 8,  NOTE_C5, 8,
    NOTE_B4, -4,  NOTE_C5, 8,  NOTE_D5, 4,  NOTE_E5, 4,
    NOTE_C5, 4,  NOTE_A4, 4,  NOTE_A4, 8,  NOTE_A4, 4,  NOTE_B4, 8,  NOTE_C5, 8,

    NOTE_D5, -4,  NOTE_F5, 8,  NOTE_A5, 4,  NOTE_G5, 8,  NOTE_F5, 8,
    NOTE_E5, -4,  NOTE_C5, 8,  NOTE_E5, 4,  NOTE_D5, 8,  NOTE_C5, 8,
    NOTE_B4, 4,  NOTE_B4, 8,  NOTE_C5, 8,  NOTE_D5, 4,  NOTE_E5, 4,
    NOTE_C5, 4,  NOTE_A4, 4,  NOTE_A4, 4, REST, 4,

    NOTE_E5, 4,  NOTE_B4, 8,  NOTE_C5, 8,  NOTE_D5, 4,  NOTE_C5, 8,  NOTE_B4, 8,
    NOTE_A4, 4,  NOTE_A4, 8,  NOTE_C5, 8,  NOTE_E5, 4,  NOTE_D5, 8,  NOTE_C5, 8,
    NOTE_B4, -4,  NOTE_C5, 8,  NOTE_D5, 4,  NOTE_E5, 4,
    NOTE_C5, 4,  NOTE_A4, 4,  NOTE_A4, 8,  NOTE_A4, 4,  NOTE_B4, 8,  NOTE_C5, 8,

    NOTE_D5, -4,  NOTE_F5, 8,  NOTE_A5, 4,  NOTE_G5, 8,  NOTE_F5, 8,
    NOTE_E5, -4,  NOTE_C5, 8,  NOTE_E5, 4,  NOTE_D5, 8,  NOTE_C5, 8,
    NOTE_B4, 4,  NOTE_B4, 8,  NOTE_C5, 8,  NOTE_D5, 4,  NOTE_E5, 4,
    NOTE_C5, 4,  NOTE_A4, 4,  NOTE_A4, 4, REST, 4,


    NOTE_E5, 2,  NOTE_C5, 2,
    NOTE_D5, 2,   NOTE_B4, 2,
    NOTE_C5, 2,   NOTE_A4, 2,
    NOTE_GS4, 2,  NOTE_B4, 4,  REST, 8,
    NOTE_E5, 2,   NOTE_C5, 2,
    NOTE_D5, 2,   NOTE_B4, 2,
    NOTE_C5, 4,   NOTE_E5, 4,  NOTE_A5, 2,
    NOTE_GS5, 2,

  };
  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {

    // calculates the duration of each note
    divider = melody[thisNote + 1];
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }


    tone(BUZZER, melody[thisNote], noteDuration * 0.9);
    delay(noteDuration);
    noTone(BUZZER);

  }
}
