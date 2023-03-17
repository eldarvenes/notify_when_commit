#include "config2.h"
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
const uint32_t connectTimeoutMs = 5000;

const char endpoint[] = ENDPOINT;
const String token = TOKEN;

String saved_sha = "";

WiFiClientSecure client;
HTTPClient httpsClient;

StaticJsonDocument<16> filter_sha;
StaticJsonDocument<64> doc;

const uint8_t BUZZER = 4;
const uint8_t WIFI_LED = 5;
const uint8_t COMMIT_LED = 12;

// wifi status
int status = WL_IDLE_STATUS;

void setup()
{
    Serial.begin(9600);

    // turn off build in led
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    
    pinMode(BUZZER, OUTPUT);
    pinMode(WIFI_LED, OUTPUT);
    pinMode(COMMIT_LED, OUTPUT);

    WiFi.mode(WIFI_STA);

    wifiMulti.addAP(wifi_ssid, wifi_password);
    filter_sha["sha"] = true;
    client.setInsecure();
}

void loop()
{ 
if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
    Serial.print("WiFi connected: ");
    Serial.print(WiFi.SSID());
    Serial.print(" ");
    Serial.println(WiFi.localIP());
    digitalWrite(WIFI_LED, HIGH);
 
    handleAction(whatNamespace(checkRepo()));

    } else {
    Serial.println("WiFi not connected!");
  }
    delay(5000);
}

String checkRepo() {
  httpsClient.useHTTP10(true);
  httpsClient.begin(client, endpoint);
    httpsClient.addHeader("Authorization", "token " + token);
    int httpCode = httpsClient.GET();
    Serial.println(httpCode);
    //String payload* = httpsClient.getString();
    //Serial.println(payload);
    DynamicJsonDocument doc(16*1024);
  DeserializationError error = deserializeJson(doc, httpsClient.getStream());

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return "<error>";
  }

  String sha = doc["sha"].as<String>();
  String filename = doc["files"][0]["filename"].as<String>();
  httpsClient.end();

  if (checkForCommits(sha)) {
    return filename;
  } else {
    return "no commit";
  }
}

void handleAction(Namespace nameSpace) {
  if (nameSpace == TEST) {
    playCommitSound();
    flashLed(COMMIT_LED);
  }
  if (nameSpace == PROD) {
    playCommitSound();
    Serial.println("PROD");
    flashLed(COMMIT_LED);
  }
  if (nameSpace == SYSTEST) {
    playCommitSound();
    flashLed(COMMIT_LED);
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
void playCommitSound() {
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

boolean checkForCommits(String sha) {
  if (saved_sha.equals("") && sha.length() > 5) {
    Serial.println("Storing sha: " + sha);
    saved_sha = sha;
  } else {

    Serial.println("Saved_sha is: " + saved_sha);
    if (sha.length() > 5 && sha != saved_sha) {
      Serial.println("New sha detected: " + sha);
      saved_sha = sha;
      return true;
    }
    return false;
  }
}

void playBuzzer() {
  tone(BUZZER, 1000);
  delay(750);
  noTone(BUZZER);
}

void flashLed() {
  for (int i=0;i<10;i++) {
    digitalWrite(COMMIT_LED, HIGH);
    delay(150);
    digitalWrite(COMMIT_LED, LOW);
    delay(150);
    }
}

Namespace whatNamespace(String filename) {
  if (filename.indexOf("test") >= 0) {
    return TEST;
  }
  if (filename.indexOf("prod") >= 0) {
    return PROD;
  }
  if (filename.indexOf("systest") >= 0) {
    return SYSTEST;
  }
  if (filename.indexOf("no commit") >= 0) {
    return NONE;
  }
}
