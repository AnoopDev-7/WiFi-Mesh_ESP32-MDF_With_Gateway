#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secret.h"

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"
// AWS IoT Core credentials
// Wi-Fi credentials
const char* ssid = "Rabyte_Technologies";
const char* password = "R@byte#321#";

// Variables to hold discovered root node details
String rootDeviceName = "";
String rootIP = "";
String rootMAC = "";

// MQTT client
// WiFiClientSecure wifiClient;
// PubSubClient mqttClient(wifiClient);
WiFiClient espClient;
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// mDNS function to discover mesh network root node
void mDNS() {
    if (!MDNS.begin("esp32")) {
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");

    // Discover mDNS service
    int n = MDNS.queryService("_mesh-http", "_tcp");
    if (n == 0) {
        Serial.println("No mDNS services found");
    } else {
        Serial.println("mDNS services found:");
        for (int i = 0; i < n; ++i) {
            rootDeviceName = MDNS.hostname(i);
            rootIP = MDNS.IP(i).toString();
            rootMAC = MDNS.txt(i, "mac");
            
            Serial.printf("Hostname: %s\n", rootDeviceName.c_str());
            Serial.printf("IP Address: %s\n", rootIP.c_str());
            Serial.printf("Port: %d\n", MDNS.port(i));
            Serial.printf("MAC Address: %s\n", rootMAC.c_str());
        }
    }
}

void connectAWS()
{
 
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}

// void messageHandler(char* topic, byte* payload, unsigned int length)
void messageHandler(char* topic, byte* payload,unsigned int length )
{
  Serial.print("incoming: ");
  Serial.println(topic);
    String incomingData = String((char*)payload).substring(0, length); // Use length to avoid overflow
    Serial.println("MQTT Message: " + incomingData);
    StaticJsonDocument<200> doc;
  deserializeJson(doc, incomingData);
  //const char* requestData = doc["data"];
   JsonObject data = doc["data"]; 
   const char* address = doc["addrs_list"];
  String dataString;
  serializeJson(data, dataString);
  Serial.println(address);
  Serial.println(dataString);
 sendDeviceRequest(rootIP,address,dataString);

 Serial.println("mqtt request sent to http");
  // memset(payload,0,200);
  //Serial1.flush();
}

// Function to send HTTP GET request to get mesh info
void sendMeshInfoRequest(const String& rootIP) {
    HTTPClient http;
    http.begin("http://" + rootIP + "/mesh_info");
    int httpResponseCode = http.GET();

    if (httpResponseCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.println("Mesh Info Response:");
        Serial.println(response);

        // Send the response to AWS IoT Core
            if (client.publish(AWS_IOT_PUBLISH_TOPIC, response.c_str())) {
        Serial.println("Publish success!");
    } else {
        Serial.println("Publish failed.");
    }

    } else {
        Serial.printf("Error: %d\n", httpResponseCode);
    }

    http.end();
}

// Function to send HTTP POST request with MAC in header
void sendDeviceRequest(const String& rootIP,String mac,String requestBody ) {
    HTTPClient http;
  //   String requestBody = "{\"request\":\"get_device_info\"}";
   //  String requestBody = "{\"request\":\"set_status\",\"characteristics\":[{\"cid\":0,\"value\":0}]}";
    
    http.begin("http://" + rootIP + "/device_request");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Mesh-Node-Mac", mac);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode == HTTP_CODE_OK) {
         String response = http.getString();
      if (client.publish(AWS_IOT_PUBLISH_TOPIC, response.c_str())) {
        Serial.println("Publish success!");
    } else {
        Serial.println("Publish failed.");
    }

        Serial.println("Device Request Response:");
        Serial.println(response);

        // Send the response to AWS IoT Core
       // sendToAWS(response);
    } else {
        Serial.printf("Error: %d\n", httpResponseCode);
    }

    http.end();
}

void connectToWiFi(){
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to Wi-Fi...");
    }

    Serial.println("Connected to Wi-Fi");
}

void setup() {
   Serial.begin(115200);
    // Connect to Wi-Fi
   connectToWiFi();
    // AWS connect and subscribe
   connectAWS();
    // Initialize mDNS and discover root node
   mDNS();
}

void loop() {
    // Get mesh information
     client.loop();

       if (WiFi.status() != WL_CONNECTED) {


    connectToWiFi();
        delay(1000);
       if (WiFi.status() == WL_CONNECTED) {
        connectAWS();
        mDNS();
        }
  }
}
