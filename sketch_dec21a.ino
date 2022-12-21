#include <Wire.h>
#include <Adafruit_BMP085.h>
#include "DHT.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "ESP32_MailClient.h"
#include "ArduinoJson.h"
#include <WiFiManager.h>
#include <iostream>


#define DHTPIN 33  // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11
Adafruit_BMP085 bmp;
SMTPData smtpData;
WiFiManager wifiManager;

const char* ssid = "";
const char* password = "";

// sender's email address
const char* sender = "rick.sanchez4044@gmail.com";
// sender's password
const char* passwordSend = "jhhcdrfpyirztoeo";
// receiver's email address
const char* receiver = "";

//Your Domain name with URL path or IP address with path
String statusPostLink = "https://weather-app-ec620-default-rtdb.europe-west1.firebasedatabase.app/status.json";
String settingsGetLink = "https://weather-app-ec620-default-rtdb.europe-west1.firebasedatabase.app/settings.json";

unsigned long lastTime = 0;
unsigned long timerDelay = 50000;

DHT dht(DHTPIN, DHTTYPE);
void setup() {
  Serial.begin(9600);
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP085/BMP180 sensor, check wiring!");
    while (1) {}
  }
  dht.begin();
  wifiConnect();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    float humidity = dht.readHumidity();
    float temperature = bmp.readTemperature();
    float pressure = bmp.readPressure();
    float alt = bmp.readAltitude();
    float pressure_sea_level = bmp.readSealevelPressure();
    float altitude = bmp.readAltitude(102000);
    print_readings(altitude, pressure_sea_level, alt, pressure, temperature, humidity);

    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      // get settings from firebase
      http.begin(settingsGetLink);
      int settingsResponseCode = http.GET();
      if (settingsResponseCode > 0) {
        String payload = http.getString();
        Serial.println(payload);
        // parse the json response
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        // get the settings from the json response
        String current_receiver = doc["email"];
        receiver = current_receiver.c_str();
        float altitudeThreshold = doc["altitudeThreshold"];
        float humidityThreshold = doc["humidityThreshold"];
        float pressureThreshold = doc["pressureThreshold"];
        float temperatureThreshold = doc["temperatureThreshold"];
        send_email(altitude, pressure, temperature, humidity, altitudeThreshold, pressureThreshold, temperatureThreshold, humidityThreshold);
      } else {
        Serial.print("Error code: ");
        Serial.println(settingsResponseCode);
      }
      http.end();


      http.begin(statusPostLink);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("x-http-method-override", "PUT");
      // Data to send with HTTP POST
      String httpRequestData = "{\"humidity\":";
      httpRequestData += humidity;
      httpRequestData += ",\"temperature\":";
      httpRequestData += temperature;
      httpRequestData += ",\"pressure\":";
      httpRequestData += pressure;
      httpRequestData += ",\"altitude\":";
      httpRequestData += altitude;
      httpRequestData += "}";
      Serial.println(httpRequestData);
      // Send HTTP POST request
      int httpResponseCode = http.POST(httpRequestData);
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();

    } else {
      Serial.println("WiFi Disconnected");
      WiFi.disconnect();
      wifiConnect();
    }
    lastTime = millis();
  }
}

bool sendEmailNotification(String emailMessage) {
  // Set email credentials
  smtpData.setLogin("smtp.gmail.com", 465, sender, passwordSend);
  smtpData.setSender("Weather Service", sender);
  smtpData.setPriority("High");
  // Set the subject
  smtpData.setSubject("Weather Alert");
  // Set the message with HTML format
  smtpData.setMessage(emailMessage, true);

  // Add recipients
  smtpData.addRecipient(receiver);
  smtpData.setSendCallback(sendCallback);

  if (!MailClient.sendMail(smtpData)) {
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());
    return false;
  }
  smtpData.empty();
  return true;
}

// Callback function to get the Email sending status
void sendCallback(SendStatus msg) {
  // Print the current status
  Serial.println(msg.info());

  // Do something when complete
  if (msg.success()) {
    Serial.println("----------------");
  }
}

void print_readings(float altitude, float pressure_sea_level, float alt, float pressure, float temperature, float humidity) {
  Serial.print("\nCurrent humidity = ");
  Serial.print(humidity);
  Serial.print("%  ");
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" *C");
  Serial.print("Pressure = ");
  Serial.print(pressure);
  Serial.println(" Pa");
  Serial.print("Altitude = ");
  Serial.print(alt);
  Serial.println(" meters");
  Serial.print("Pressure at sealevel (calculated) = ");
  Serial.print(pressure_sea_level);
  Serial.println(" Pa");
  Serial.print("Real altitude = ");
  Serial.print(altitude);
  Serial.println(" meters");
  Serial.println();
}

void send_email(float altitude, float pressure, float temperature, float humidity,
                float altitudeThres, float pressureThres, float tempThres, float humidityThres) {
  if (temperature > tempThres) {
    sendEmailNotification("The temperature is too high, currently at " + String(temperature) + " degrees");
  }
  if (humidity > humidityThres) {
    sendEmailNotification("The humidity is too high, currently at " + String(humidity) + "%");
  }
  if (pressure > pressureThres) {
    sendEmailNotification("The pressure is too high, currently at " + String(pressure) + " Pa");
  }
  if (altitude > altitudeThres) {
    sendEmailNotification("The altitude is too high, currently at " + String(altitude) + " meters");
  }
}


void wifiConnect() {
  bool res = wifiManager.autoConnect("AutoConnectAP", "password");
  Serial.print(wifiManager.getWiFiHostname());
  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void saveReadingsToCSV(float altitude, float pressure, float temperature, float humidity) {
  Serial.println(String(altitude) + "," + String(pressure) + "," + String(temperature) + "," + String(humidity));
}
