// ===============================================================================
#define WIFIMODE 2                           // 0 = Only Soft Access Point, 1 = Only connect to local WiFi network with UN/PW, 2 = Both
#define MQTTMODE 1                           // 0 = Disable MQTT, 1 = Enable (will only be enabled if WiFi mode = 1 or 2 - broker must be on same network)
#define MQTTCLIENT "MQTTClientYT"            // MQTT Client Name (should be unique across all clients
#define MQTT_TOPIC_SUB "cmnd/youtube/#"      // Default MQTT subscribe topic 
#define MQTT_TOPIC_PUB "stat/youtube"        // Default MQTT publish topic
#define OTA_HOSTNAME "youtubeOTA"            // Hostname to broadcast as port in the IDE of OTA Updates

// ---------------------------------------------------------------------------------------------------------------
// Options - Defaults upon boot-up or any other custom ssttings
// ---------------------------------------------------------------------------------------------------------------
bool ota_flag = true;                    // Must leave this as true for board to broadcast port to IDE upon boot
uint16_t ota_time_elapsed = 0;           // Counter when OTA active
uint16_t ota_boot_time_window = 2500;    // minimum time on boot for IP address to show in IDE ports, in millisecs
uint16_t ota_time_window = 20000;        // time to start file upload when ota_flag set to true (after initial boot), in millsecs
unsigned long timeBetweenRequests = 120000; // time between YouTube API requests (120000 = 2 minutes)
