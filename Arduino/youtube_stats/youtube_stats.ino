/*
 * YouTube Statistics
 * Version 1.10
 * Last updated 7/24/2021
 * Adds optional Home Assistant MQTT Discovery
 */
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <YoutubeApi.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "Credentials.h"          //File must exist in same folder as .ino.  Edit as needed for project
#include "Settings.h"             //File must exist in same folder as .ino.  Edit as needed for project

//GLOBAL VARIABLES
bool mqttConnected = false;       //Will be enabled if defined and successful connnection made.  This var should be checked upon any MQTT actin.
long lastReconnectAttempt = 0;    //If MQTT connected lost, attempt reconnenct
uint16_t ota_time = ota_boot_time_window;
uint16_t ota_time_elapsed = 0;           // Counter when OTA active
unsigned long nextRunTime;
long subs = 0;
//Setup Local Access point if enabled via WIFI Mode
#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2)
  const char* APssid = AP_SSID;        
  const char* APpassword = AP_PWD;  
#endif
//Setup Wifi if enabled via WIFI Mode
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2)
  #include "Credentials.h"                    // Edit this file in the same directory as the .ino file and add your own credentials
  const char *ssid = SID;
  const char *password = PW;
#endif
//Setup MQTT if enabled - only available when WiFi is also enabled
#if (WIFIMODE == 1 || WIFIMODE == 2) && (MQTTMODE == 1)    // MQTT only available when on local wifi
  const char *mqttUser = MQTTUSERNAME;
  const char *mqttPW = MQTTPWD;
  const char *mqttClient = MQTTCLIENT;
  const char *mqttTopicSub = MQTT_TOPIC_SUB;
#endif

WiFiClient espClient;
PubSubClient client(espClient);    
ESP8266WebServer server;

WiFiClientSecure secClient;
YoutubeApi api(API_KEY, secClient);


// ============================================
//   SETUP
// ============================================
void setup() {
  // Serial monitor
  Serial.begin(115200);
  Serial.println("Booting...");
  //----------------
  //WiFi Connection
  //----------------
  WiFi.setSleepMode(WIFI_NONE_SLEEP);  //Disable WiFi Sleep
  delay(200);
  // WiFi - AP Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2) 
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(APssid, APpassword);    // IP is usually 192.168.4.1
#endif
  // WiFi - Local network Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2) 
  byte count = 0;
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // Stop if cannot connect
    if (count >= 60) {
      // Could not connect to local WiFi 
      Serial.println();
      Serial.println("Could not connect to WiFi.");     
      return;
    }
    delay(500);
    count++;
  }
  Serial.println();
  Serial.println("Successfully connected to Wifi");
  IPAddress ip = WiFi.localIP();
  secClient.setInsecure();
#endif   
  //-----------------------------------------
  // Setup MQTT - only if enabled and on WiFi  
  //-----------------------------------------
#if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
  byte mcount = 0;
  //char topicPub[32];
  client.setServer(MQTTSERVER, MQTTPORT);
  client.setCallback(callback);
  Serial.print("Connecting to MQTT broker.");
  while (!client.connected( )) {
    Serial.print(".");
    client.connect(mqttClient, mqttUser, mqttPW);
    if (mcount >= 60) {
      Serial.println();
      Serial.println("Could not connect to MQTT broker. MQTT disabled.");
      // Could not connect to MQTT broker
      return;
    }
    delay(500);
    mcount++;
  }
  mqttConnected = true;
  Serial.println();
  Serial.println("Successfully connected to MQTT broker.");
  client.subscribe(mqttTopicSub);
  String outTopic = mqttTopicPub + "/status";
  client.publish(outTopic.c_str(), "mqtt_connected");
#endif
  //------------------------------
  // Home Assistant MQTT Discovery
  //------------------------------
  if (mqttConnected) {
      setup_ha_discovery();
  }
  
  //-----------------------------
  // Setup OTA Updates
  //-----------------------------
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
  });
  ArduinoOTA.begin();
  // Setup handlers for web calls for OTAUpdate and Restart
  server.on("/restart",[](){
    server.send(200, "text/html", "<h1>Restarting...</h1>");
    delay(1000);
    ESP.restart();
  });
  server.on("/otaupdate",[]() {
    server.send(200, "text/html", "<h1>Ready for upload...<h1><h3>Start upload from IDE now</h3>");
    ota_flag = true;
    ota_time = ota_time_window;
    ota_time_elapsed = 0;
  });
  server.begin();
  Serial.println("Staring main loop...");
}

// =============================================================
// *************** MQTT Message Processing *********************
// =============================================================
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  /*
   * Add any commands submitted here
   * Example:
   * if (strcmp(topic, "cmnd/matrix/mode")==0) {
   *   MyVal = message;
   *   Do something
   *   return;
   * };
   */
};
// Reconnect to broker if connection lost (usually a result of a Home Assistant/broker/server reboot)
boolean reconnect() {
  if (client.connect(mqttClient, mqttUser, mqttPW)) {
    //resubscribe
    client.subscribe(mqttTopicSub);
  }
  return client.connected();
}
//-------------------------------
// Home Assistant MQTT Discovery
//-------------------------------
void setup_ha_discovery() {
  if (ha_discovery) {
    char buffer[256];
    char buffer1[256];
    char buffer2[256];
    char buffer3[256];
    StaticJsonDocument<200> doc;
    //Sensors
    //API Status
    doc.clear();
    doc["name"] = "YouTube API Status";
    doc["state_topic"] = MQTT_TOPIC_PUB"/status";
    serializeJson(doc, buffer);
    client.publish("homeassistant/sensor/youtube_api_status/config", buffer, true);
    //Total Views
    doc.clear();
    doc["name"] = "YouTube Total Views";
    doc["state_topic"] = MQTT_TOPIC_PUB"/views";
    serializeJson(doc, buffer1);
    client.publish("homeassistant/sensor/youtube_total_views/config", buffer1, true);
    //Subscribers
    doc.clear();
    doc["name"] = "YouTube Subscribers";
    doc["state_topic"] = MQTT_TOPIC_PUB"/subs";
    serializeJson(doc, buffer2);
    client.publish("homeassistant/sensor/youtube_subscribers/config", buffer2, true);
   //Number of Videos
    doc.clear();
    doc["name"] = "YouTube Videos";
    doc["state_topic"] = MQTT_TOPIC_PUB"/videos";
    serializeJson(doc, buffer3);
    client.publish("homeassistant/sensor/youtube_videos/config", buffer3, true);
  } else {
    // publish with empty payload, which will remove HA entities if previously created
    client.publish("homeassistant/sensor/youtube_api_status/config", "");
    client.publish("homeassistant/sensor/youtube_total_views/config", "");
    client.publish("homeassistant/sensor/youtube_subscribers/config", "");
    client.publish("homeassistant/sensor/youtube_videos/config", "");
  }
}
// ===============================================================
//   MAIN LOOP
// ===============================================================
void loop() {
  //Handle OTA updates when OTA flag set via HTML call to http://ip_address/otaupdate
  if (ota_flag) {
    uint16_t ota_time_start = millis();
    while (ota_time_elapsed < ota_time) {
      ArduinoOTA.handle();  
      ota_time_elapsed = millis()-ota_time_start;   
      delay(10); 
    }
    ota_flag = false;
  }
  //Handle any web calls
  server.handleClient();
  // Reconnect to MQTT if enabled and not connected
  if (MQTTMODE == 1) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 60000) {   //attempt reconnect once per minute. Drop usually a result of Home Assistant/broker server restart
        lastReconnectAttempt = now;
        // Attempt to reconnect
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
//    } else {
//      // Client connected
//      client.loop();
    }
    client.loop();
  }
  delay(1000);
  String topicSubs = mqttTopicPub + "/subs";
  String topicViews = mqttTopicPub + "/views";
  String topicVids = mqttTopicPub + "/videos";
  String topicStatus = mqttTopicPub + "/status";
  //REMAINDER OF CODE HERE:
  if (millis() > nextRunTime)  {
    if(api.getChannelStatistics(CHANNEL_ID))
    {
      if (mqttConnected) {
        char outSubs[6];
        char outViews[6];
        char outVids[6];
        sprintf(outSubs, "%5u", api.channelStats.subscriberCount);
        sprintf(outViews, "%5u", api.channelStats.viewCount);
        sprintf(outVids, "%5u", api.channelStats.videoCount);
        client.publish(topicStatus.c_str(), "API_OK", true);
        client.publish(topicSubs.c_str(), outSubs, true);
        client.publish(topicViews.c_str(), outViews, true); 
        client.publish(topicVids.c_str(), outVids, true);
      } else {
        Serial.println("API Connection: OK");
        Serial.println("---------Stats---------");
        Serial.print("Subscriber Count: ");
        Serial.println(api.channelStats.subscriberCount);
        Serial.print("View Count: ");
        Serial.println(api.channelStats.viewCount);
        Serial.print("Video Count: ");
        Serial.println(api.channelStats.videoCount);
        Serial.println("------------------------");
      }
    } else {
      if (mqttConnected) {
        client.publish(topicStatus.c_str(), "API_FAILED", true);
      } else {
        Serial.println("API Call Failed");
      }
    }
    nextRunTime = millis() + timeBetweenRequests;
  }
}
