#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <espnow.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <U8g2lib.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

// Replace with your network credentials
const char *default_ssid = "ceva";
const char *default_password = "ceva";
const char *serverUrl = "http://20.33.76.19:8080/temperature";

// Define the display and its connections
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define DHTPIN 3    // Pinul RX (GPIO3) la care este conectat DHT11
#define DHTTYPE DHT11 // Tipul senzorului DHT11
#define LEDPIN 1    // Pinul TX (GPIO1) la care este conectat LED-ul

DHT dht(DHTPIN, DHTTYPE);

enum Message_type
{
    INIT,
    DISCOVERY,
    DISCOVERY_REPLY,
    DATA,
    WIFI_CREDENTIALS
};

enum Mode
{
    WIFI_CONNECTED,
    PEER,
    SEARCHING,
    AP_MODE
};

typedef struct struct_message
{
    float temperature;
    float humidity;
    uint32_t chip_id;
    char ssid[32];
    char password[32];
    Message_type messageType;
} struct_message;

struct_message myData;
struct_message receivedData;

WiFiClient client;
ESP8266WebServer server(80);

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

unsigned long lastSendTime = 0;
const long sendInterval = 10000;

uint8_t peer_mac[6] = {0};

Mode mode = SEARCHING;

struct_message wait_Data;

int handle_attempts = 0;

void handleRoot()
{
    String html = "<html><head><style>";
    html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }";
    html += "form { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }";
    html += "h1 { margin-bottom: 20px; }";
    html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; }";
    html += "input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; }";
    html += "input[type='submit']:hover { background-color: #45a049; }";
    html += "</style></head><body>";
    html += "<form action=\"/connect\" method=\"POST\">";
    html += "<h1>Enter WiFi Credentials</h1>";
    html += "SSID: <input type=\"text\" name=\"ssid\"><br>";
    html += "Password: <input type=\"password\" name=\"password\"><br>";
    html += "<input type=\"submit\" value=\"Submit\">";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
}

void handleConnect()
{
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    handle_attempts = 0;

    if (ssid.length() > 0 && password.length() > 0)
    {
        WiFi.begin(ssid.c_str(), password.c_str());
        server.sendHeader("Location", "/loading", true);
        server.send(302, "text/html", "");
    }
    else
    {
        server.sendHeader("Location", "/invalid", true);
        server.send(302, "text/html", "");
    }
}

void handleLoading()
{
    String html = "<html><head><style>";
    html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }";
    html += ".loader { border: 16px solid #f3f3f3; border-top: 16px solid #3498db; border-radius: 50%; width: 120px; height: 120px; animation: spin 2s linear infinite; }";
    html += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
    html += "</style></head><body>";
    html += "<div class=\"loader\"></div>";
    html += "<meta http-equiv=\"refresh\" content=\"2; url=/retry\" />";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleRetry()
{
    handle_attempts++;
    if (WiFi.status() == WL_CONNECTED)
    {
        server.sendHeader("Location", "/success", true);
        server.send(302, "text/html", "");
        handle_attempts = 0;
    }
    else if (handle_attempts >= 4)
    {
        String html = "<html><head><style>";
        html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }";
        html += "</style></head><body>";
        html += "<h1>Connection failed! Please try again.</h1>";
        html += "<meta http-equiv=\"refresh\" content=\"5; url=/\" />";
        html += "</body></html>";
        server.send(200, "text/html", html);
        handle_attempts = 0;
    }
    else
    {
        server.sendHeader("Location", "/loading", true);
        server.send(302, "text/html", "");
    }
}

void handleSuccess()
{
    String html = "<html><head><style>";
    html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }";
    html += "</style></head><body>";
    html += "<h1>Connected!</h1>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleInvalid()
{
    String html = "<html><head><style>";
    html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }";
    html += "</style></head><body>";
    html += "<h1>Invalid input! Please try again.</h1>";
    html += "<meta http-equiv=\"refresh\" content=\"5; url=/\" />";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void setup()
{
    Wire.begin(0, 2);        // Inițializează I2C pe pinii GPIO0 (SCL) și GPIO2 (SDA)
    pinMode(LEDPIN, OUTPUT); // Setează pinul LED ca ieșire

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(default_ssid, default_password);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        retry++;
        if (retry > 5)
        {
            break;
        }
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Conexiune esuata");
        u8g2.sendBuffer();
    }
    else
    {
        u8g2.drawStr(0, 10, "Conectat la WiFi");
        u8g2.sendBuffer();
        mode = WIFI_CONNECTED;
    }

    delay(1000);

    wait_Data.humidity = 0;

    char* name = (char*)malloc(20);
    sprintf(name, "ESP8266_Config_%d", ESP.getChipId());
    WiFi.softAP(name);
    IPAddress IP = WiFi.softAPIP();
    server.on("/", handleRoot);
    server.on("/connect", HTTP_POST, handleConnect);
    server.on("/loading", handleLoading);
    server.on("/retry", handleRetry);
    server.on("/success", handleSuccess);
    server.on("/invalid", handleInvalid);
    server.begin();
    delay(2000);

    if (esp_now_init() != 0)
    {
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "ESP-NOW init failed");
        u8g2.sendBuffer();
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    esp_now_register_recv_cb(OnDataRecv);

    dht.begin();

    ArduinoOTA.onStart([]()
                       {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "OTA update");
    u8g2.sendBuffer(); });
    ArduinoOTA.onEnd([]()
                     {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "OTA update finalizat");
    u8g2.sendBuffer(); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "OTA update");
    u8g2.setCursor(0, 20);
    u8g2.print("Progres: ");
    u8g2.print(progress / (total / 100));
    u8g2.print("%");
    u8g2.sendBuffer(); });
    ArduinoOTA.onError([](ota_error_t error)
                       {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "OTA eroare");
    u8g2.setCursor(0, 20);
    u8g2.print("Cod eroare: ");
    u8g2.print(error);
    u8g2.sendBuffer(); });
    ArduinoOTA.begin();
}

void loop()
{
    ArduinoOTA.handle();

    server.handleClient();

    if (WiFi.status() == WL_CONNECTED && mode != WIFI_CONNECTED)
    {
        mode = WIFI_CONNECTED;
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Conectat la WiFi");
        u8g2.sendBuffer();
        delay(2000);
    }

    if (wait_Data.humidity > 0 && mode == WIFI_CONNECTED)
    {
        send_data_to_server(wait_Data.temperature, wait_Data.humidity, wait_Data.chip_id);
        wait_Data.humidity = -1;
    }

    unsigned long currentMillis = millis();
    if (currentMillis - lastSendTime >= sendInterval)
    {
        lastSendTime = currentMillis;

        switch (mode)
        {
        case WIFI_CONNECTED:
            // write to display
            u8g2.clearBuffer();
            u8g2.drawStr(0, 10, "trimitere la WiFi");
            u8g2.sendBuffer();
            wifi_mode();
            break;

        case PEER:
            u8g2.clearBuffer();
            u8g2.drawStr(0, 10, "trimitere la peer");
            u8g2.sendBuffer();
            peer_mode();
            break;

        case SEARCHING:
            u8g2.clearBuffer();
            u8g2.drawStr(0, 10, "cautare");
            u8g2.sendBuffer();
            searching_mode();
            break;
        }
    }

    delay(100);
}

void peer_mode()
{
    if (peer_mac[0] == 0)
    {
        mode = SEARCHING;
        return;
    }

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t))
    {
        return;
    }

    myData.temperature = t;
    myData.humidity = h;
    myData.messageType = DATA;
    myData.chip_id = ESP.getChipId();

    esp_now_send(peer_mac, (uint8_t *)&myData, sizeof(myData));
}

void searching_mode()
{
    myData.temperature = 0;
    myData.humidity = 0;
    myData.messageType = DISCOVERY;
    esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
}

void wifi_mode()
{
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t))
    {   
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Eroare citire senzor");
        u8g2.sendBuffer();
        delay(1000);
        return;
    }

    send_data_to_server(t, h, ESP.getChipId());
}

void send_data_to_server(float temperature, float humidity, uint32_t chip_id)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Conexiune pierduta");
        u8g2.sendBuffer();
        return;
    }
    StaticJsonDocument<200> doc;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["chipId"] = chip_id;

    char buffer[200];
    serializeJson(doc, buffer);

    HTTPClient http;
    http.begin(client, serverUrl);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(buffer);

    u8g2.clearBuffer();
    if (httpResponseCode > 0)
    {
        String response = http.getString();
        u8g2.drawStr(0, 10, "Date trimise:");
        u8g2.setCursor(0, 20);
        u8g2.print("Temp: ");
        u8g2.print(temperature);
        u8g2.print(" C");
        u8g2.setCursor(0, 30);
        u8g2.print("Umid: ");
        u8g2.print(humidity);
        u8g2.print(" %");
    }
    else
    {
        u8g2.drawStr(0, 10, "Eroare trimitere date");

        u8g2.setCursor(0, 20);
        u8g2.print("Cod eroare: ");
        u8g2.print(httpResponseCode);
    }

    u8g2.sendBuffer();
    http.end();
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len)
{
    if (len != sizeof(receivedData) && len != 6)
    {
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Eroare dimensiune");
        u8g2.sendBuffer();

        delay(1000);
        return;
    }

    if (len == 6) {
        if (mode != SEARCHING)
        {
            return;
        }

        for (int i = 0; i < 6; i++)
        {
            peer_mac[i] = mac[i];
        }
        mode = PEER;
        return;
    }

    memcpy(&receivedData, incomingData, sizeof(receivedData));
    if (receivedData.messageType == DISCOVERY)
    {
        uint8_t my_mac[6];
        WiFi.macAddress(my_mac);

        esp_now_send(mac, my_mac, 6);

        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Raspuns la descoperire");
        u8g2.sendBuffer();
        return;
    }

    if (receivedData.messageType == WIFI_CREDENTIALS)
    {
        u8g2.clearBuffer();
        u8g2.setCursor(0, 10);
        u8g2.print("Cred. WiFi primite:");
        u8g2.sendBuffer();

        WiFi.begin(receivedData.ssid, receivedData.password);

        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 10)
        {
            delay(500);
            retry++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            mode = WIFI_CONNECTED;
            u8g2.clearBuffer();
            u8g2.drawStr(0, 10, "Conectat la WiFi");
            u8g2.sendBuffer();
        }
        else
        {
            mode = SEARCHING;
            u8g2.clearBuffer();
            u8g2.drawStr(0, 10, "Conexiune esuata");
            u8g2.sendBuffer();
        }
        return;
    }

    if (receivedData.messageType == DATA)
    {
        u8g2.clearBuffer();
        u8g2.setCursor(0, 10);
        u8g2.print("Date primite:");
        u8g2.sendBuffer();

        wait_Data.humidity = receivedData.humidity;
        wait_Data.temperature = receivedData.temperature;
        wait_Data.chip_id = receivedData.chip_id;

        if (mode == WIFI_CONNECTED)
        {
            send_data_to_server(receivedData.temperature, receivedData.humidity, receivedData.chip_id);
        }
        else
        {
            esp_now_send(broadcastAddress, (uint8_t *)&receivedData, sizeof(receivedData));
        }

        return;
    }

    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Raspuns la ANACONDA");
    u8g2.sendBuffer();
}
