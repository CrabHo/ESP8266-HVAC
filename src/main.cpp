#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ir_Daikin.h>

IRDaikinESP daikinir(D2);  // An IR LED is controlled by GPIO pin 4 (D2)


//openhab MQQT settings
const char* ssid = "...";
const char* password = "...";
const char* mqtt_server = "...";
const char* brokerusername = "...";
const char* brokerpassword = "...";

//topic
const char* power_set = "bedroom/ac/power/set";
const char* mode_set = "bedroom/ac/mode/set";
const char* temperature_set = "bedroom/ac/temperature/set";
const char* fan_set = "bedroom/ac/fan/set";
const char* swing_set = "bedroom/ac/swing/set";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String p_payload;

  for (int i = 0; i < length; i++) {
    p_payload.concat((char)payload[i]);
  }
  Serial.println(p_payload);

  if (strcmp(topic, mode_set) == 0) {
    daikinir.setPower(1);
    if (p_payload == "auto") {
      daikinir.setMode(kDaikinAuto);
    } else if (p_payload == "cool") {
      daikinir.setMode(kDaikinCool);
    } else if (p_payload == "heat") {
      daikinir.setMode(kDaikinHeat);
    } else if (p_payload == "dry") {
      daikinir.setMode(kDaikinDry);
    } else if (p_payload == "fan_only") {
      daikinir.setMode(kDaikinFan);
    } else {
      daikinir.setPower(0);
    }
  } else if (strcmp(topic, temperature_set) == 0) {
    daikinir.setTemp(strtoul(p_payload.c_str(), NULL, 10));
  } else if (strcmp(topic, fan_set) == 0) {
    if (p_payload == "auto") {
      daikinir.setFan(kDaikinFanAuto);
    } else if (p_payload == "low") {
      daikinir.setFan(kDaikinFanMin);
    } else if (p_payload == "medium") {
      daikinir.setFan(kDaikinFanMed);
    } else if (p_payload == "high") {
      daikinir.setFan(kDaikinFanMax);
    }
  } else if (strcmp(topic, swing_set) == 0) {
    if (p_payload == "on") {
      daikinir.setSwingHorizontal(1);
      daikinir.setSwingVertical(1);
    } else {
      daikinir.setSwingHorizontal(0);
      daikinir.setSwingVertical(0);
    }
  }
  if (strcmp(topic, power_set) != 0) {
    Serial.print("power=");
    Serial.println(daikinir.getPower());
    Serial.print("mode=");
    Serial.println(daikinir.getMode());
    Serial.print("temp=");
    Serial.println(daikinir.getTemp());
    Serial.print("fan=");
    Serial.println(daikinir.getFan());
    Serial.print("swingh=");
    Serial.println(daikinir.getSwingHorizontal());
    Serial.print("swingv=");
    Serial.println(daikinir.getSwingVertical());
    daikinir.send();
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), brokerusername, brokerpassword)) {
      Serial.println("connected");
      client.subscribe(power_set);
      client.subscribe(mode_set);
      client.subscribe(temperature_set);
      client.subscribe(fan_set);
      client.subscribe(swing_set);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  daikinir.begin();
  daikinir.setPower(0);
  daikinir.setFan(kDaikinFanAuto);
  daikinir.setMode(kDaikinCool);
  daikinir.setSwingHorizontal(1);
  daikinir.setSwingVertical(1);
  daikinir.setTemp(25);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
