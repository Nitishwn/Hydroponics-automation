#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ThingSpeak.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <algorithm>

// WiFi credentials
const char* ssid = "********";
const char* password = "**********";

// ThingSpeak API Key and Channel ID
const char* thingSpeakApiKey = "******";
unsigned long thingSpeakChannelID = ********;

// Gemini API Key
const char* geminiApiKey = "*******************";
const char* geminiApiEndpoint = "*************************";

// Sensor Pins
#define PH_PIN 35       // Analog pin for pH Sensor
#define TDS_PIN 34      // Analog pin for TDS Sensor
#define DHT_PIN 4       // Digital pin for DHT Sensor
#define RELAY_PIN 26    // Relay Control Pin
#define LED_PIN 27      // LED for pH indication

// Sensor Configuration
#define TDS_VOLTAGE_REF 3.3
#define TDS_RESOLUTION 4095.0
#define TDS_FACTOR 0.5

#define VREF 3.3
#define ADC_RESOLUTION 4095.0
#define DHT_TYPE DHT11

// pH Control Configuration
#define PH_THRESHOLD 6.5  // pH threshold above which relay turns on
#define RELAY_ON_TIME 2000  // Time in milliseconds to keep relay on (2 seconds)

// AsyncWebServer instance
AsyncWebServer server(80);
DHT dht(DHT_PIN, DHT_TYPE);

// Global variables
WiFiClient client;
HTTPClient http;
float temperature = 0.0, humidity = 0.0, tdsValue = 0.0, pHValue = 0.0;
String lastRecommendation = "";
bool recommendationInProgress = false;

// Relay control variables
bool relayActivated = false;
unsigned long relayStartTime = 0;

int getMedianAnalogRead(int pin) {
  int readings[10];
  for (int i = 0; i < 10; i++) {
    readings[i] = analogRead(pin);
    delay(10);
  }
  std::sort(readings, readings + 10);
  return readings[5];
}

String getRecommendation(float tds, float ph, float temp, float hum) {
  if (WiFi.status() != WL_CONNECTED) {
    return "Error: WiFi not connected";
  }

  DynamicJsonDocument requestDoc(1024);
  JsonObject contents = requestDoc["contents"].createNestedObject();
  JsonArray parts = contents["parts"].createNestedArray();
  
  // Create prompt for Gemini
  String prompt = "As a water quality and plant health expert, provide a concise practical recommendation based on these sensor readings: ";
  prompt += "pH: " + String(ph, 2) + ", ";
  prompt += "TDS: " + String(tds, 2) + " ppm, ";
  prompt += "Temperature: " + String(temp, 2) + "°C, ";
  prompt += "Humidity: " + String(hum, 2) + "%. ";
  prompt += "Focus on immediate actions needed for optimal water quality and plant health. Keep response under 150 words.";
  
  JsonObject part = parts.createNestedObject();
  part["text"] = prompt;
  
  String requestBody;
  serializeJson(requestDoc, requestBody);
  
  String url = String(geminiApiEndpoint) + "?key=" + geminiApiKey;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(requestBody);
  String response = "No response";
  
  if (httpResponseCode > 0) {
    response = http.getString();
    DynamicJsonDocument responseDoc(8192);
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      if (responseDoc.containsKey("candidates") && responseDoc["candidates"].size() > 0) {
        if (responseDoc["candidates"][0].containsKey("content") && 
            responseDoc["candidates"][0]["content"].containsKey("parts") && 
            responseDoc["candidates"][0]["content"]["parts"].size() > 0) {
          response = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
        } else {
          response = "Error: Unexpected API response format";
        }
      } else {
        response = "Error: No recommendation generated";
      }
    } else {
      response = "Error parsing API response: " + String(error.c_str());
    }
  } else {
    response = "Error: HTTP response code " + String(httpResponseCode);
  }
  
  http.end();
  return response;
}

String searchForRecommendation(String searchTerm) {
  if (WiFi.status() != WL_CONNECTED) {
    return "Error: WiFi not connected";
  }

  DynamicJsonDocument requestDoc(1024);
  JsonObject contents = requestDoc["contents"].createNestedObject();
  JsonArray parts = contents["parts"].createNestedArray();
  
  // Create prompt for Gemini search
  String prompt = "As a water quality and plant health expert, provide specific information about '" + searchTerm + "' ";
  prompt += "in the context of current sensor readings: ";
  prompt += "pH: " + String(pHValue, 2) + ", ";
  prompt += "TDS: " + String(tdsValue, 2) + " ppm, ";
  prompt += "Temperature: " + String(temperature, 2) + "°C, ";
  prompt += "Humidity: " + String(humidity, 2) + "%. ";
  prompt += "Focus on practical advice related to the search term. Keep response under 200 words.";
  
  JsonObject part = parts.createNestedObject();
  part["text"] = prompt;
  
  String requestBody;
  serializeJson(requestDoc, requestBody);
  
  String url = String(geminiApiEndpoint) + "?key=" + geminiApiKey;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(requestBody);
  String response = "No response";
  
  if (httpResponseCode > 0) {
    response = http.getString();
    DynamicJsonDocument responseDoc(8192);
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      if (responseDoc.containsKey("candidates") && responseDoc["candidates"].size() > 0) {
        if (responseDoc["candidates"][0].containsKey("content") && 
            responseDoc["candidates"][0]["content"].containsKey("parts") && 
            responseDoc["candidates"][0]["content"]["parts"].size() > 0) {
          response = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
        } else {
          response = "Error: Unexpected API response format";
        }
      } else {
        response = "Error: No information found for '" + searchTerm + "'";
      }
    } else {
      response = "Error parsing API response: " + String(error.c_str());
    }
  } else {
    response = "Error: HTTP response code " + String(httpResponseCode);
  }
  
  http.end();
  return response;
}

void updateSensorData() {
  int tdsRaw = getMedianAnalogRead(TDS_PIN);
  float tdsVoltage = tdsRaw * TDS_VOLTAGE_REF / TDS_RESOLUTION;
  tdsValue = tdsVoltage / TDS_FACTOR;

  int phRaw = getMedianAnalogRead(PH_PIN);
  float phVoltage = (phRaw * VREF) / ADC_RESOLUTION;
  pHValue = -4.973 * phVoltage + 21.12;

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  Serial.println("=================================");
  Serial.print("TDS Value: "); Serial.print(tdsValue); Serial.println(" ppm");
  Serial.print("pH Value: "); Serial.print(pHValue); Serial.println(" pH");
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" °C");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
  Serial.print("Relay Status: "); Serial.println(digitalRead(RELAY_PIN) == HIGH ? "ON" : "OFF");
  Serial.println("=================================");
}

// Control relay based on pH value
void manageRelay() {
  // If pH is above threshold, turn on relay
  if (pHValue > PH_THRESHOLD && !relayActivated) {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    relayActivated = true;
    relayStartTime = millis();
    Serial.println("Relay turned ON due to high pH!");
  }
  
  // Check if it's time to turn off the relay
  if (relayActivated && (millis() - relayStartTime >= RELAY_ON_TIME)) {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    relayActivated = false;
    Serial.println("Relay turned OFF after timeout");
  }
}

// HTML Dashboard with advanced UI/UX
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0; padding: 0;
      background: linear-gradient(to bottom, #74ebd5, #9face6);
      display: flex; flex-direction: column;
      align-items: center; justify-content: center;
      min-height: 100vh;
    }
    h2 { font-size: 3rem; color: #222; }
    .sensor-container {
      display: flex; gap: 20px; flex-wrap: wrap; justify-content: center;
    }
    .sensor-block {
      width: 200px; height: 200px;
      background: rgba(255, 255, 255, 0.9);
      border-radius: 20px; text-align: center;
      display: flex; flex-direction: column; align-items: center; justify-content: center;
    }
    .sensor-title { font-size: 1.5rem; color: #333; }
    .sensor-value { font-size: 2rem; font-weight: bold; color: #007BFF; }
    button {
      margin-top: 30px; padding: 15px 30px;
      font-size: 1.2rem; border-radius: 10px;
      border: none; background-color: #28a745;
      color: white; cursor: pointer;
    }
    button:disabled {
      background-color: #aaa;
      cursor: wait;
    }
    #result {
      margin-top: 20px; font-size: 1.2rem;
      color: #333; max-width: 600px; text-align: center;
      background: rgba(255, 255, 255, 0.8);
      padding: 20px; border-radius: 10px;
    }
    .search-container {
      margin-top: 30px;
      width: 90%;
      max-width: 600px;
    }
    .search-box {
      width: 100%;
      padding: 15px;
      border-radius: 10px;
      border: none;
      font-size: 1.2rem;
    }
    .search-button {
      margin-top: 10px;
      width: 100%;
      padding: 15px;
      background-color: #0066ff;
      color: white;
      border: none;
      border-radius: 10px;
      font-size: 1.2rem;
      cursor: pointer;
    }
    .search-button:disabled {
      background-color: #aaa;
    }
    .relay-status {
      margin-top: 20px;
      padding: 10px 20px;
      border-radius: 10px;
      font-size: 1.2rem;
      font-weight: bold;
    }
    .relay-on {
      background-color: #ff6b6b;
      color: white;
    }
    .relay-off {
      background-color: #a7c5e3;
      color: #333;
    }
  </style>
</head>
<body>
  <h2>Sensor Data</h2>
  <div class="sensor-container">
    <div class="sensor-block"><div class="sensor-title">TDS</div><div class="sensor-value" id="tds">%TDS%</div> ppm</div>
    <div class="sensor-block"><div class="sensor-title">pH</div><div class="sensor-value" id="ph">%PH%</div></div>
    <div class="sensor-block"><div class="sensor-title">Temperature</div><div class="sensor-value" id="temp">%TEMP%</div>°C</div>
    <div class="sensor-block"><div class="sensor-title">Humidity</div><div class="sensor-value" id="hum">%HUM%</div>%</div>
  </div>
  
  <div class="relay-status %RELAY_CLASS%" id="relayStatus">Relay: %RELAY_STATUS%</div>
  
  <button id="submitBtn" onclick="getRecommendation()">Get Recommendation</button>
  <div id="result">%LAST_RECOMMENDATION%</div>
  
  <div class="search-container">
    <input type="text" class="search-box" id="searchInput" placeholder="Search for specific plant or water condition..." />
    <button class="search-button" id="searchBtn" onclick="searchRecommendation()">Search</button>
  </div>

<script>
  let currentData = { tds: 0, ph: 0, temperature: 0, humidity: 0 };

  async function updateSensorData() {
    try {
      const response = await fetch('/data');
      const data = await response.json();
      
      document.getElementById("tds").innerText = data.tds.toFixed(2);
      document.getElementById("ph").innerText = data.ph.toFixed(2);
      document.getElementById("temp").innerText = data.temperature.toFixed(2);
      document.getElementById("hum").innerText = data.humidity.toFixed(2);
      
      // Update relay status
      const relayStatus = document.getElementById("relayStatus");
      if(data.relay === "ON") {
        relayStatus.innerText = "Relay: ON";
        relayStatus.className = "relay-status relay-on";
      } else {
        relayStatus.innerText = "Relay: OFF";
        relayStatus.className = "relay-status relay-off";
      }

      currentData = data;
    } catch (error) {
      console.error("Error fetching sensor data:", error);
    }
  }

  setInterval(updateSensorData, 1000);

  async function getRecommendation() {
    const button = document.getElementById("submitBtn");
    button.disabled = true;
    button.innerText = "Getting Recommendation...";
    document.getElementById("result").innerText = "Analyzing data...";

    try {
      const response = await fetch("/recommendation");
      if (!response.ok) {
        throw new Error(`HTTP error! Status: ${response.status}`);
      }

      const text = await response.text();
      document.getElementById("result").innerText = text;
    } catch (err) {
      document.getElementById("result").innerText = "Failed to get recommendation: " + err.message;
      console.error("Error:", err);
    } finally {
      button.disabled = false;
      button.innerText = "Get Recommendation";
    }
  }

  async function searchRecommendation() {
    const searchTerm = document.getElementById("searchInput").value.trim();
    if (!searchTerm) {
      alert("Please enter a search term");
      return;
    }

    const button = document.getElementById("searchBtn");
    button.disabled = true;
    button.innerText = "Searching...";
    document.getElementById("result").innerText = "Searching for information...";

    try {
      const response = await fetch("/search?term=" + encodeURIComponent(searchTerm));
      if (!response.ok) {
        throw new Error(`HTTP error! Status: ${response.status}`);
      }

      const text = await response.text();
      document.getElementById("result").innerText = text;
    } catch (err) {
      document.getElementById("result").innerText = "Search failed: " + err.message;
      console.error("Search Error:", err);
    } finally {
      button.disabled = false;
      button.innerText = "Search";
    }
  }
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  ThingSpeak.begin(client);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();

  // Server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = index_html;
    html.replace("%TDS%", String(tdsValue));
    html.replace("%PH%", String(pHValue));
    html.replace("%TEMP%", String(temperature));
    html.replace("%HUM%", String(humidity));
    
    // Relay status
    String relayStatus = digitalRead(RELAY_PIN) == HIGH ? "ON" : "OFF";
    String relayClass = digitalRead(RELAY_PIN) == HIGH ? "relay-on" : "relay-off";
    html.replace("%RELAY_STATUS%", relayStatus);
    html.replace("%RELAY_CLASS%", relayClass);
    
    html.replace("%LAST_RECOMMENDATION%", lastRecommendation.isEmpty() ? 
                "Click 'Get Recommendation' for AI analysis based on current sensor readings" : 
                lastRecommendation);
    request->send(200, "text/html", html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    updateSensorData();
    String json;
    StaticJsonDocument<256> doc;
    doc["ph"] = pHValue;
    doc["tds"] = tdsValue;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["relay"] = digitalRead(RELAY_PIN) == HIGH ? "ON" : "OFF";
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Recommendation endpoint
  server.on("/recommendation", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (recommendationInProgress) {
      request->send(200, "text/plain", "A recommendation is already being generated. Please wait a moment.");
      return;
    }
    
    recommendationInProgress = true;
    String recommendation = getRecommendation(tdsValue, pHValue, temperature, humidity);
    lastRecommendation = recommendation;
    recommendationInProgress = false;
    
    request->send(200, "text/plain", recommendation);
  });

  // Search endpoint
  server.on("/search", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("term")) {
      request->send(400, "text/plain", "Error: No search term provided");
      return;
    }
    
    String searchTerm = request->getParam("term")->value();
    if (searchTerm.isEmpty()) {
      request->send(400, "text/plain", "Error: Empty search term");
      return;
    }
    
    if (recommendationInProgress) {
      request->send(200, "text/plain", "A request is already being processed. Please wait a moment.");
      return;
    }
    
    recommendationInProgress = true;
    String searchResult = searchForRecommendation(searchTerm);
    recommendationInProgress = false;
    
    request->send(200, "text/plain", searchResult);
  });

  server.begin();
}

void loop() {
  updateSensorData();
  manageRelay();  // Check and manage relay based on pH value

  // Send data to ThingSpeak every 15 seconds
  static unsigned long lastThingSpeakTime = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastThingSpeakTime > 15000) {
    lastThingSpeakTime = currentTime;
    
    ThingSpeak.setField(1, tdsValue);
    ThingSpeak.setField(2, pHValue);
    ThingSpeak.setField(3, temperature);
    ThingSpeak.setField(4, humidity);

    int response = ThingSpeak.writeFields(thingSpeakChannelID, thingSpeakApiKey);
    if (response == 200) {
      Serial.println("Data sent to ThingSpeak successfully.");
    } else {
      Serial.print("Error sending data to ThingSpeak. HTTP error code: ");
      Serial.println(response);
    }
  }
  
  delay(1000);
}