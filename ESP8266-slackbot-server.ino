#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

const char* ssid = "YOUR_WIFI_SSID";          // your WiFi SSID
const char* password = "YOUR_WIFI_PASSWORD";  // your WiFi password

const char* slackToken = "xoxb-XXXXXXXXXXXX-XXXXXXXXXXXX-XXXXXXXXXXXXXXXXXXXX"; // your Slack token
const char* slackChannel = "#your-channel";   // your Slack channel

const char* apiKey = "YOUR_MUSLIMSALAT_API_KEY"; // MuslimSalat API key
const char* city = "islamabad";               // city
const int methodCalc = 3;  // calculation method
//  1 = Egyptian General Authority of Survey
//  2 = University Of Islamic Sciences, Karachi (Shafi)
//  3 = University Of Islamic Sciences, Karachi (Hanafi)
//  4 = Islamic Circle of North America
//  5 = Muslim World League
//  6 = Umm Al-Qura
//  7 = Fixed Isha

const long gmtOffsetSec = 5 * 3600;
const long daylightOffsetSec = 0;

struct PrayerTimes {
  String fajr;
  String dhuhr;
  String asr;
  String maghrib;
  String isha;
  String date_for;
};

PrayerTimes todayTimes;

time_t lastDailyFetch = 0;
time_t lastSync = 0;

//Utility to convert “h:mm am/pm” → hour24, minute
void parseTimeString(const String &sOrig, int &hour24, int &minute) {
  String s = sOrig;
  bool pm = (s.indexOf("pm") >= 0 || s.indexOf("PM") >= 0);
  s.replace("am",""); s.replace("pm",""); s.replace("AM",""); s.replace("PM",""); s.trim();
  int colon = s.indexOf(':');
  if (colon > 0) {
    hour24 = s.substring(0, colon).toInt();
    minute = s.substring(colon + 1).toInt();
    if (pm && hour24 != 12) hour24 += 12;
    if (!pm && hour24 == 12) hour24 = 0;
  } else {
    hour24 = 0;
    minute = 0;
  }
}

void sendSlackMessage(const String &text) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://slack.com/api/chat.postMessage";
  if (!http.begin(client, url)) {
    Serial.println("Slack http.begin failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + slackToken);

  String payload = "{\"channel\":\"" + String(slackChannel) + "\",\"text\":\"" + text + "\"}";
  int code = http.POST(payload);
  Serial.print("Slack HTTP code: "); Serial.println(code);
  if (code > 0) {
    String resp = http.getString();
    Serial.println("Slack resp: " + resp);
  } else {
    Serial.println("Error in Slack POST");
  }
  http.end();
}

bool fetchPrayerTimes(PrayerTimes &outTimes) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://muslimsalat.com/") + city + "/daily.json?key=" + apiKey + "&method=" + String(methodCalc);
  if (!http.begin(client, url)) {
    Serial.println("API begin failed");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.print("API GET failed, code: "); Serial.println(code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<2048> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonObject item0 = doc["items"][0];
  outTimes.fajr = item0["fajr"].as<String>();
  outTimes.dhuhr = item0["dhuhr"].as<String>();
  outTimes.asr = item0["asr"].as<String>();
  outTimes.maghrib = item0["maghrib"].as<String>();
  outTimes.isha = item0["isha"].as<String>();
  outTimes.date_for = item0["date_for"].as<String>();

  return true;
}

void syncTime() {
  configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
  delay(2000);
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  Serial.printf("Time sync: %02d:%02d:%02d, wday=%d\n",
                ti.tm_hour, ti.tm_min, ti.tm_sec, ti.tm_wday);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");

  syncTime();

  if (fetchPrayerTimes(todayTimes)) {
    lastDailyFetch = time(nullptr);
    sendSlackMessage("🤖 Slack bot is up! Today's prayer times for " + String(city) + " (" + todayTimes.date_for + "):\n"
                     + "Fajr: " + todayTimes.fajr + "\n"
                     + "Dhuhr: " + todayTimes.dhuhr + "\n"
                     + "Asr: " + todayTimes.asr + "\n"
                     + "Maghrib: " + todayTimes.maghrib + "\n"
                     + "Isha: " + todayTimes.isha);
  } else {
    sendSlackMessage("🤖 Slack bot is up! Failed to fetch today’s prayer times.");
  }
}

void loop() {
  time_t now = time(nullptr);

  if (now - lastSync >= 2 * 3600) {
    syncTime();
    lastSync = now;
  }

  struct tm *t = localtime(&now);

  //get prayer times daily at 12:01
  if (t->tm_hour == 12 && t->tm_min == 1 && t->tm_sec == 0) {
    if (fetchPrayerTimes(todayTimes)) {
      lastDailyFetch = now;
      sendSlackMessage("✅ Updated prayer times for today (" + String(city) + " " + todayTimes.date_for + ")");
    } else {
      sendSlackMessage("⚠️ Failed to refresh prayer times today.");
    }
    delay(60 * 1000); // avoid multiple triggers within the same minute
  }

  // 12:05 reminder of today's times
  if (t->tm_hour == 12 && t->tm_min == 5 && t->tm_sec == 0) {
    sendSlackMessage("🌟 Prayer schedule for today (" + String(city) + " " + todayTimes.date_for + "):\n"
                     + "Fajr: " + todayTimes.fajr + "\n"
                     + "Dhuhr: " + todayTimes.dhuhr + "\n"
                     + "Asr: " + todayTimes.asr + "\n"
                     + "Maghrib: " + todayTimes.maghrib + "\n"
                     + "Isha: " + todayTimes.isha);
    delay(60 * 1000);
  }
//
//  // 9AM summary
//  if (t->tm_hour == 9 && t->tm_min == 0 && t->tm_sec == 0) {
//    sendSlackMessage("🌅 Today’s Salah schedule for " + String(city) + " (" + todayTimes.date_for + "):\n"
//                     + "Fajr: " + todayTimes.fajr + "\n"
//                     + "Dhuhr: " + todayTimes.dhuhr + "\n"
//                     + "Asr: " + todayTimes.asr + "\n"
//                     + "Maghrib: " + todayTimes.maghrib + "\n"
//                     + "Isha: " + todayTimes.isha);
//    delay(60 * 1000);
//  }

  // Reminders at each prayer time
  auto checkAndSend = [&](const String &prayerName, const String &prayerTimeStr) {
    int prH, prM;
    parseTimeString(prayerTimeStr, prH, prM);
    if (t->tm_hour == prH && t->tm_min == prM && t->tm_sec == 0) {
      sendSlackMessage("🕌 Reminder: It is time for " + prayerName + " prayer (" + prayerTimeStr + ")");
      delay(60 * 1000);
    }
  };

  checkAndSend("Fajr", todayTimes.fajr);
  checkAndSend("Dhuhr", todayTimes.dhuhr);
  checkAndSend("Asr", todayTimes.asr);
  checkAndSend("Maghrib", todayTimes.maghrib);
  checkAndSend("Isha", todayTimes.isha);

  // Jummah reminder (Friday morning)
  if (t->tm_wday == 5 && t->tm_hour == 9 && t->tm_min == 0 && t->tm_sec == 0) {
    sendSlackMessage("📢 Jummah Reminder: Don’t forget Friday prayers + recite Surah Al-Kahf today!");
    delay(60 * 1000);
  }

  delay(1000);
}
