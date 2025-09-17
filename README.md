# ESP8266 Slack Prayer Reminder Bot

This project uses an **ESP8266** microcontroller to fetch daily prayer times from the [MuslimSalat API](https://muslimsalat.com/) and send reminders to a **Slack channel**.
The device connects to Wi-Fi, fetches the prayer schedule once per day, and posts updates and reminders throughout the day.

## Features

* Fetches daily prayer times for a configured city
* Posts a summary of times to Slack each morning
* Sends reminders at each prayer time
* Daily refresh of times at noon
* Jummah (Friday) reminder

## Requirements

* ESP8266 board (NodeMCU, Wemos D1 Mini, etc.)
* Arduino IDE with ESP8266 core installed
* Slack Bot Token with `chat:write` permissions
* MuslimSalat API key

## Setup

1. Clone this repo and plug in your secrets
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* slackToken = "YOUR_SLACK_BOT_TOKEN";
   const char* slackChannel = "#your-channel";
   const char* apiKey = "YOUR_MUSLIMSALAT_API_KEY";
   ```
2. Upload the code to your ESP8266 board.
3. Reset the board and check the Serial Monitor for logs.

## Notes

* Time is synced via NTP. Ensure the `gmtOffsetSec` matches your timezone.

---

