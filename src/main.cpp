#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <LiquidCrystal_I2C.h>
#include "secrets.h" // from GitHub Actions

WiFiClientSecure secured_client;
UniversalTelegramBot bot(TG_BOT_TOKEN, secured_client);
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

unsigned long lastCheck = 0;
const long checkInterval = 3000; // 3 seconds
bool updateAvailable = true; // temporarily always true

String manifestUrl = "https://raw.githubusercontent.com/" + String(GH_REPO) + "/main/out/manifest.json";

void lcdMessage(const String &msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg.substring(0, LCD_COLS));
  if (msg.length() > LCD_COLS) {
    lcd.setCursor(0, 1);
    lcd.print(msg.substring(LCD_COLS, LCD_COLS * 2));
  }
}

void sendTelegramWithButton(String text) {
  String keyboard = "[[{\"text\":\"Update\",\"callback_data\":\"update\"}]]";
  bot.sendMessageWithInlineKeyboard(TG_CHAT_ID, text, "", keyboard);
}

void performOTA() {
  lcdMessage("OTA Loading...");
  HTTPClient http;
  http.begin(manifestUrl);
  int httpCode = http.GET();
  if (httpCode != 200) {
    lcdMessage("Manifest Error");
    bot.sendMessage(TG_CHAT_ID, "Error fetching manifest.json");
    return;
  }
  String json = http.getString();
  http.end();

  DynamicJsonDocument doc(512);
  deserializeJson(doc, json);
  String fwUrl = doc["url"].as<String>();

  lcdMessage("Downloading firmware");
  bot.sendMessage(TG_CHAT_ID, "Starting update...");

  http.begin(fwUrl);
  int resp = http.GET();
  if (resp == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (Update.begin(contentLength)) {
      WiFiClient *client = http.getStreamPtr();
      Update.writeStream(*client);
      if (Update.end()) {
        bot.sendMessage(TG_CHAT_ID, "Update completed!");
        lcdMessage("Done, restarting");
        delay(2000);
        ESP.restart();
      } else {
        bot.sendMessage(TG_CHAT_ID, "OTA Error: " + String(Update.getError()));
        lcdMessage("OTA Error");
      }
    }
  } else {
    bot.sendMessage(TG_CHAT_ID, "Firmware download error");
    lcdMessage("Download error");
  }
  http.end();
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    if (text == "/start") {
      sendTelegramWithButton("Firmware update available.");
    }
    if (text == "/update") {
      performOTA();
    }
  }
}
//asdasd
void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcdMessage("Starting WiFi...");

  WiFiManager wm;
  if (!wm.autoConnect(WM_AP_NAME, WM_AP_PASS)) {
    lcdMessage("WiFi Error");
    ESP.restart();
  }

  secured_client.setInsecure();
  lcdMessage("WiFi connected");

  sendTelegramWithButton("Device started. Firmware update available.");
}

void loop() {
  if (millis() - lastCheck > checkInterval) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastCheck = millis();
  }
}
