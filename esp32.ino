#include "EmonLib.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TimeLib.h>
#include <WiFi.h>
#include <WebServer.h>

#define VOLT_CAL 74.6
#define CURRENT_CAL 14.4

const int relayPin = 5;
const int voltagePin = 13;
const int currentPin = 14;
const char *ssid = "Targov";
const char *password = "Alex1997";
const char *serverUrl = "http://192.168.43.205:3200";
const char *createMeasurmentRoute = "/measurement";
const String createMeasurmentServerUrl = String(serverUrl) + String(createMeasurmentRoute);
const String deviceId = String("dee11d4e-63c6-4d90-983c-5c9f1e79e96c");

EnergyMonitor emon1;
WebServer server(80);

void setup()
{
  Serial.begin(9600);
 
  Serial.println("\nSETUP");

  pinMode(relayPin, OUTPUT);
  emon1.voltage(voltagePin, VOLT_CAL, 1.7); // esp32
  emon1.current(currentPin, CURRENT_CAL);

  connectToNetwork();
  setup_routing();
}

float ampData[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
float voltageData[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int iteration = 0;
unsigned long lastRefreshTime = 0L;
unsigned long REFRESH_INTERVAL = 500L;

void loop()
{
  gatherMeasurementData(ampData, voltageData);

  server.handleClient();
}

void connectToNetwork() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Establishing connection to WiFi..");
  }

  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
}

int gatherMeasurementData(float ampData[], float voltageData[]) {
  unsigned long currentMillis = millis();

  if ((currentMillis - lastRefreshTime) >= REFRESH_INTERVAL) {
    lastRefreshTime += REFRESH_INTERVAL;
    Serial.println(iteration);

    emon1.calcVI(20, 100);
    
    float supplyAmp = emon1.Irms;
    float supplyVoltage = emon1.Vrms;

    if (iteration < 10) {
      ampData[iteration] = supplyAmp;
      voltageData[iteration] = supplyVoltage;
      iteration += 1;
    }

    if (iteration == 10) {
      float resultingVolts = 0;
      float resultinAmps = 0;

      for (int i = 0; i < 10; i++) {
        resultingVolts = resultingVolts + voltageData[i];
        resultinAmps = resultinAmps + ampData[i];
      }

      Serial.println("Send data;");

//      postMeasurementsToServer(resultingVolts / 10, resultinAmps / 10, (resultingVolts * resultinAmps) / 10);
      iteration = 0;
    }

    return iteration;
  }
}

void postMeasurementsToServer(float voltage, float current, float power) {
  Serial.println("Posting JSON data to server...");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(createMeasurmentServerUrl);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<1024> doc;

    time_t timeNow = now();

    doc["voltage"] = voltage;
    doc["current"] = current;
    doc["power"] = power;
    doc["createdAt"] = timeNow;

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {

      String response = http.getString();

      Serial.println(httpResponseCode);
      Serial.println(response);
    }
    else {
      Serial.printf("Error occurred while sending HTTP POST");
    }
  }
}

void getAnyData() {
  HTTPClient http;
  http.begin(serverUrl);

  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  }
  else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
}

void setup_routing() {
  server.on("/relay", HTTP_POST, handleRelay);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("setup_routing(): Routing is set up");
}

void handleRelay() {
  StaticJsonDocument<250> requestDoc;

  requestDoc.clear();

  if (server.hasArg("plain") == false) {
    Serial.println("Error: handleRelay");
   
    server.send(500, "application/json", "{\"success\": false}");
    return;
  }

  Serial.println("handleRelay(): POST request");

  String requestBody = server.arg("plain");
  deserializeJson(requestDoc, requestBody);
  
  int relayStatus = requestDoc["status"];
  String relayId = requestDoc["id"];

  Serial.println("");
  Serial.print("handleRelay(): status value: ");
  Serial.print(relayStatus);
  Serial.println("");
  Serial.print("handleRelay(): id value: ");
  Serial.print(relayId);


  if (!relayId.equals(deviceId)) {
   
    server.send(500, "application/json", "{\"success\": false}");
    return;
  }

  if (relayStatus == 1) {
    digitalWrite(relayPin, HIGH);
  } else {
    digitalWrite(relayPin, LOW);
  }

  server.send(200, "application/json", "{\"success\": true}");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}
