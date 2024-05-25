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
const char *ssid = "Tenda_49C4C01";
const char *password = "Shiro123";
const char *serverUrl = "http://192.168.1.195:6000/temperature";

// Define the display and its connections
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define DHTPIN 3	  // Pinul RX (GPIO3) la care este conectat DHT11
#define DHTTYPE DHT11 // Tipul senzorului DHT11
#define LEDPIN 1	  // Pinul TX (GPIO1) la care este conectat LED-ul

DHT dht(DHTPIN, DHTTYPE);

enum Message_type
{
	INIT,
	DISCOVERY,
	DISCOVERY_REPLY,
	DATA
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
	Message_type messageType;
} struct_message;

struct_message myData;
struct_message receivedData;

WiFiClient client;
ESP8266WebServer server(80);

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

unsigned long lastSendTime = 0;
const long sendInterval = 5000;

uint8_t peer_mac[6] = {0};

Mode mode = SEARCHING;

void handleRoot()
{
	String html = "<html><body><h1>Enter WiFi Credentials</h1>";
	html += "<form action=\"/connect\" method=\"POST\">";
	html += "SSID: <input type=\"text\" name=\"ssid\"><br>";
	html += "Password: <input type=\"text\" name=\"password\"><br>";
	html += "<input type=\"submit\" value=\"Submit\">";
	html += "</form></body></html>";
	server.send(200, "text/html", html);
}

void handleConnect()
{
	String ssid = server.arg("ssid");
	String password = server.arg("password");

	if (ssid.length() > 0 && password.length() > 0)
	{
		WiFi.begin(ssid.c_str(), password.c_str());
		delay(2000);
		if (WiFi.status() == WL_CONNECTED)
		{
			server.send(200, "text/html", "Connected! Please reboot the device.");
		}
		else
		{
			server.send(200, "text/html", "Connection failed! Please try again.");
		}
	}
	else
	{
		server.send(200, "text/html", "Invalid input! Please try again.");
	}
}

void setup()
{
	Wire.begin(0, 2);		 // Inițializează I2C pe pinii GPIO0 (SCL) și GPIO2 (SDA)
	pinMode(LEDPIN, OUTPUT); // Setează pinul LED ca ieșire

	u8g2.begin();
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_ncenB08_tr);

	WiFi.mode(WIFI_AP_STA);
	WiFi.begin(ssid, password);
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

	WiFi.softAP("ESP8266_Config");
	IPAddress IP = WiFi.softAPIP();
	server.on("/", handleRoot);
	server.on("/connect", HTTP_POST, handleConnect);
	server.begin();
	u8g2.clearBuffer();
	u8g2.setCursor(0, 10);
	u8g2.print("AP IP address: ");
	u8g2.setCursor(0, 20);
	u8g2.print(IP);
	u8g2.sendBuffer();
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

	unsigned long currentMillis = millis();
	if (currentMillis - lastSendTime >= sendInterval)
	{
		lastSendTime = currentMillis;

		switch (mode)
		{
		case WIFI_CONNECTED:
			wifi_mode();
			break;

		case PEER:
			peer_mode();
			break;

		case SEARCHING:
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
		return;
	}

	send_data_to_server(t, h);
}

void send_data_to_server(float temperature, float humidity)
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
	doc["chipId"] = ESP.getChipId();

	char buffer[200];
	serializeJson(doc, buffer);

	HTTPClient http;
	http.begin(client, serverUrl);
	http.addHeader("Content-Type", "application/json");
	int httpResponseCode = http.POST(buffer);

	u8gs.clearBuffer();
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
	if (len != sizeof(receivedData))
	{
		return;
	}

	memcpy(&receivedData, incomingData, sizeof(receivedData));
	if (receivedData.messageType == DISCOVERY)
	{
		uint8_t my_mac[6];
		WiFi.macAddress(my_mac);

		esp_now_send(mac, my_mac, 6);
		return;
	}

	if (receivedData.messageType == DATA)
	{
		u8g2.clearBuffer();
		u8g2.setCursor(0, 10);
		u8g2.print("Date primite:");
		u8g2.setCursor(0, 20);
		u8g2.print("Temp: ");
		u8g2.print(receivedData.temperature);
		u8g2.print(" C");
		u8g2.setCursor(0, 30);
		u8g2.print("Umid: ");
		u8g2.print(receivedData.humidity);
		u8g2.print(" %");
		u8g2.setCursor(0, 40);

		u8g2.sendBuffer();
		// send_data_to_server(receivedData.temperature, receivedData.humidity);

		return;
	}

	if (receivedData.messageType == DISCOVERY_REPLY)
	{
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
}
