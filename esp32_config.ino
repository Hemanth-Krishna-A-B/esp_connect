#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <WebServer.h>
#include <EEPROM.h>

// Define built-in LED for ESP32
#define LED_BUILTIN 14

// WiFi Credentials (Defaults, will be overwritten if configured)
char ssid[32] = "";
char password[64] = "";

// Telegram Bot Credentials
#define BOT_TOKEN "   "
#define CHAT_ID "  "

// WiFi Client & Telegram Bot
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WebServer server(80);

unsigned long lastCheckTime = 0;
const int checkInterval = 2000; // Check Telegram every 2 sec

bool wifiConnected = false;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    // Initialize EEPROM (store WiFi credentials)
    EEPROM.begin(96);
    loadWiFiCredentials();

    // Try connecting to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi!");
        wifiConnected = true;
        secured_client.setInsecure();  
        Serial.println("Enter messages in Serial Monitor to send to Telegram:");
    } else {
        Serial.println("\nFailed to connect to WiFi. Starting AP mode...");
        startWiFiSetup();
    }
}

void loop() {
    if (!wifiConnected) {
        server.handleClient();
    } else {
        if (millis() - lastCheckTime > checkInterval) {
            lastCheckTime = millis();
            checkTelegramMessages();
        }
        sendSerialToTelegram();
    }
}

// ---------------------------
// WiFi Setup via Access Point
// ---------------------------
void startWiFiSetup() {
    WiFi.softAP("ESP32-Setup", "12345678");
    Serial.println("WiFi AP Started. Connect to 'ESP32-Setup' and visit 192.168.4.1");

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", R"rawliteral(
            <html>
            <head>
            <title>WiFi Setup</title>
            </head>
            <body>
            <h2>Enter WiFi Credentials</h2>
            <form action="/save" method="POST">
                SSID: <input type="text" name="ssid"><br>
                Password: <input type="password" name="password"><br>
                <input type="submit" value="Save & Restart">
            </form>
            </body>
            </html>)rawliteral");
    });

    server.on("/save", HTTP_POST, []() {
        String newSSID = server.arg("ssid");
        String newPassword = server.arg("password");

        if (newSSID.length() > 0 && newPassword.length() > 0) {
            saveWiFiCredentials(newSSID.c_str(), newPassword.c_str());
            server.send(200, "text/plain", "WiFi credentials saved! Restarting...");
            delay(1000);
            ESP.restart();
        } else {
            server.send(400, "text/plain", "Invalid input. Try again.");
        }
    });

    server.begin();
}

// ---------------------------
// EEPROM: Store & Load WiFi Credentials
// ---------------------------
void saveWiFiCredentials(const char* newSSID, const char* newPassword) {
    EEPROM.writeString(0, newSSID);
    EEPROM.writeString(32, newPassword);
    EEPROM.commit();
}

void loadWiFiCredentials() {
    EEPROM.readString(0).toCharArray(ssid, 32);
    EEPROM.readString(32).toCharArray(password, 64);
}

// ---------------------------
// Telegram Bot Functions
// ---------------------------
void checkTelegramMessages() {
    int messageCount = bot.getUpdates(bot.last_message_received + 1);
    while (messageCount) {
        Serial.println("New Message Received!");

        for (int i = 0; i < messageCount; i++) {
            String chat_id = bot.messages[i].chat_id;
            String text = bot.messages[i].text;
            Serial.println("Message: " + text);

            if (chat_id == CHAT_ID) {
                if (text == "/on") {
                    digitalWrite(LED_BUILTIN, HIGH);
                    bot.sendMessage(CHAT_ID, "✅ LED is ON!", "");
                } else if (text == "/off") {
                    digitalWrite(LED_BUILTIN, LOW);
                    bot.sendMessage(CHAT_ID, "❌ LED is OFF!", "");
                } else {
                    bot.sendMessage(CHAT_ID, "❓ Unknown Command! Use /on or /off", "");
                }
            }
        }
        messageCount = bot.getUpdates(bot.last_message_received + 1);
    }
}

void sendSerialToTelegram() {
    if (Serial.available()) {
        String message = Serial.readStringUntil('\n');
        message.trim();
        if (message.length() > 0) {
            Serial.println("Sending message to Telegram...");
            bot.sendMessage(CHAT_ID, "📩 " + message, "");
            Serial.println("Message sent!");
        }
    }
}
