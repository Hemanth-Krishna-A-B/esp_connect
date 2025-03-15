#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WebServer.h>


#define DHTPIN 4          
#define DHTTYPE DHT11     

#define LEDC_TIMER_12_BIT 12
#define LEDC_BASE_FREQ 5000
#define LED_PIN 5

#define BOT_TOKEN "7854730138:AAEavJEJTx4y8G0tD4YGKsUkbOXLvx4dzwg"
#define CHAT_ID "7037375742"

#define MAX_TEMP 75


char ssid[32] = "";
char password[64] = "";
unsigned long lastCheckTime = 0;
const int checkInterval = 2000; 


int brightness = 0;

const char* wifiConfigPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 WiFi Setup</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            background-color: #f4f4f4;
            padding: 20px;
        }
        .container {
            background: white;
            max-width: 400px;
            margin: 0 auto;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        input, button {
            width: 100%;
            padding: 10px;
            margin: 10px 0;
            border: 1px solid #ccc;
            border-radius: 5px;
            font-size: 16px;
        }
        button {
            background: #007bff;
            color: white;
            border: none;
            cursor: pointer;
        }
        button:hover {
            background: #0056b3;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>WiFi Setup</h2>
        <form action="/save" method="POST">
            <input type="text" name="ssid" placeholder="Enter WiFi SSID" required>
            <input type="password" name="password" placeholder="Enter WiFi Password" required>
            <button type="submit">Save & Connect</button>
        </form>
    </div>
</body>
</html>
)rawliteral";


DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WebServer server(80);


void ledcAnalogWrite(uint8_t pin, uint32_t value, uint32_t valueMax = 255) {
  uint32_t duty = (4095 / valueMax) * min(value, valueMax);
  ledcWrite(pin, duty);
}

void saveWiFiCredentials(const char* newSSID, const char* newPassword) {
    EEPROM.writeString(0, newSSID);
    EEPROM.writeString(32, newPassword);
    EEPROM.commit();
}

void loadWiFiCredentials() {
    EEPROM.readString(0).toCharArray(ssid, 32);
    EEPROM.readString(32).toCharArray(password, 64);
}

void startAccessPoint() {
    Serial.println("\nStarting Access Point...");
    WiFi.softAP("ESP32_Setup", "12345678");
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.begin();
    Serial.println("AP Started. Connect to ESP32_Setup and open 192.168.4.1");
}

void handleSave() {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");
    saveWiFiCredentials(newSSID.c_str(), newPassword.c_str());
    server.send(200, "text/plain", "Credentials Saved! Restarting...");
    delay(2000);
    ESP.restart();
}

void handleRoot() {
    server.send(200, "text/html",wifiConfigPage);
}

void checkTelegramMessages() {
    int messageCount = bot.getUpdates(bot.last_message_received + 1);

    while (messageCount) {
        Serial.println("New Message Received!");

        for (int i = 0; i < messageCount; i++) {
            String chat_id = bot.messages[i].chat_id;
            String text = bot.messages[i].text;
            Serial.println("Message: " + text);

            if (chat_id == CHAT_ID) {
                if (text == "on") {
                    bot.sendMessage(CHAT_ID, "ON (100%)", "");
                    brightness = 255;
                } else if (text == "off") {
                    brightness = 0;
                    bot.sendMessage(CHAT_ID, "OFF!", "");
                } else if (text == "temp") {
                    sendTemperatureToTelegram();
                } else if (text.startsWith("brightness ")) {
                    brightness = text.substring(10).toInt();
                    if (brightness >= 0 && brightness <= 255) {
                        bot.sendMessage(CHAT_ID,"oky");
                        
                    } else {
                        bot.sendMessage(CHAT_ID, "⚠️ Invalid brightness! Use a value between 0-255.", "");
                    }
                } else {
                    bot.sendMessage(CHAT_ID, "❓ Unknown Command! Use /on, /off, /temp, or 'brightness 128'", "");
                }
            }
        }
        messageCount = bot.getUpdates(bot.last_message_received + 1);
    }
}


void sendTemperatureToTelegram() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity(); 

    if (isnan(temperature) || isnan(humidity)) {
        bot.sendMessage(CHAT_ID, "⚠️ Error reading temperature and humidity!", "");
        return;
    }

    String message = "🌡️ Temperature: " + String(temperature) + "°C\n";
    message += "💧 Humidity: " + String(humidity) + "%";

    bot.sendMessage(CHAT_ID, message, "");
}


void setup() {
    Serial.begin(115200);
    
    dht.begin();  
    ledcAttach(LED_PIN, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAnalogWrite(LED_PIN, 255);

    
    EEPROM.begin(96);
    loadWiFiCredentials();

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
        secured_client.setInsecure();
        Serial.println("Enter messages in Serial Monitor to send to Telegram:");
    } else {
        Serial.println("\nFailed to connect to WiFi! starting acces point");
        startAccessPoint();
    }
}

void loop() {
    if (millis() - lastCheckTime > checkInterval) {
        lastCheckTime = millis();
        checkTelegramMessages();
        int temperature = (int)dht.readTemperature();
        if(temperature <= MAX_TEMP && temperature <= brightness)
        {
          ledcAnalogWrite(LED_PIN, brightness); 
        }
        else
        {
          ledcAnalogWrite(LED_PIN, 0); 
        }
    }
}




