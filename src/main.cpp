#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ir_Daikin.h>
#include <DHT.h>
IRDaikinESP daikinir(D2);  // An IR LED is controlled by GPIO pin 4 (D2)
DHT dht;

//Wifi
const char* ssid = "...";
const char* password = "...";

//MQTT server
const char* mqttServer = "...";
const char* mqttUsername = "...";
const char* mqttPassword = "...";

//MQTT topic
const char* topicModeSet = "bedroom/ac/mode/set";
const char* topicTemperatureSet = "bedroom/ac/temperature/set";
const char* topicFanSet = "bedroom/ac/fan/set";
const char* topicSwingSet = "bedroom/ac/swing/set";

const char* topicModeState = "bedroom/ac/mode/state";
const char* topicTemperatureState = "bedroom/ac/temperature/state";
const char* topicFanState = "bedroom/ac/fan/state";
const char* topicSwingState = "bedroom/ac/swing/state";
const char* topicCurrentTemperatureState = "bedroom/ac/currenttemperature/state";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastread;
float humidity, temperature;

void setupWifi() {
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

void setupAc() {
  daikinir.begin();
  daikinir.setPower(0);
  daikinir.setFan(kDaikinFanAuto);
  daikinir.setMode(kDaikinCool);
  daikinir.setSwingHorizontal(1);
  daikinir.setSwingVertical(1);
  daikinir.setTemp(23);
}

void sendIrSignal() {
  daikinir.send();
}

void sendMqttState() {
  switch (daikinir.getPower()) {
    case 0:
      client.publish(topicModeState, "off");
      break;
    case 1:
      switch (daikinir.getMode()) {
        case kDaikinAuto:
          client.publish(topicModeState, "auto");
          break;
        case kDaikinCool:
          client.publish(topicModeState, "cool");
          break;
        case kDaikinHeat:
          client.publish(topicModeState, "heat");
          break;
        case kDaikinDry:
          client.publish(topicModeState, "dry");
          break;
        case kDaikinFan:
          client.publish(topicModeState, "fan_only");
          break;
        default:
          client.publish(topicModeState, "cool");
      }
      break;
    default:
      client.publish(topicModeState, "off");
  }
  switch (daikinir.getFan()) {
    case kDaikinFanAuto:
      client.publish(topicFanState, "auto");
      break;
    case kDaikinFanMin:
      client.publish(topicFanState, "low");
      break;
    case kDaikinFanMed:
      client.publish(topicFanState, "medium");
      break;
    case kDaikinFanMax:
      client.publish(topicFanState, "high");
      break;
    default:
      client.publish(topicFanState, "auto");
  }

  if (daikinir.getSwingHorizontal()) {
    client.publish(topicSwingState, "on");
  } else {
    client.publish(topicSwingState, "off");
  }

  client.publish(topicTemperatureState, String(daikinir.getTemp()).c_str());
  Serial.println("Sent state to mqtt server.");
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

  if (strcmp(topic, topicModeSet) == 0) {
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
  } else if (strcmp(topic, topicTemperatureSet) == 0) {
    daikinir.setTemp(strtoul(p_payload.c_str(), NULL, 10));
  } else if (strcmp(topic, topicFanSet) == 0) {
    if (p_payload == "auto") {
      daikinir.setFan(kDaikinFanAuto);
    } else if (p_payload == "low") {
      daikinir.setFan(kDaikinFanMin);
    } else if (p_payload == "medium") {
      daikinir.setFan(kDaikinFanMed);
    } else if (p_payload == "high") {
      daikinir.setFan(kDaikinFanMax);
    }
  } else if (strcmp(topic, topicSwingSet) == 0) {
    if (p_payload == "on") {
      daikinir.setSwingHorizontal(1);
      daikinir.setSwingVertical(1);
    } else {
      daikinir.setSwingHorizontal(0);
      daikinir.setSwingVertical(0);
    }
  }

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

  sendIrSignal();
  sendMqttState();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqttUsername, mqttPassword)) {
      Serial.println("connected");
      client.subscribe(topicModeSet);
      client.subscribe(topicTemperatureSet);
      client.subscribe(topicFanSet);
      client.subscribe(topicSwingSet);
      sendMqttState();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupAc();
  dht.setup(D3);
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
}

char *f2s(float f, int p){
  char * pBuff;                         // use to remember which part of the buffer to use for dtostrf
  const int iSize = 10;                 // number of bufffers, one for each float before wrapping around
  static char sBuff[iSize][20];         // space for 20 characters including NULL terminator for each float
  static int iCount = 0;                // keep a tab of next place in sBuff to use
  pBuff = sBuff[iCount];                // use this buffer
  if(iCount >= iSize -1){               // check for wrap
    iCount = 0;                         // if wrapping start again and reset
  }
  else{
    iCount++;                           // advance the counter
  }
  return dtostrf(f, 0, p, pBuff);       // call the library function
}

void sendCurrentTemperatureState() {
  if((millis() - lastread) > 60000) // time to update
  {
    temperature = dht.getTemperature();
    humidity = dht.getHumidity();
    lastread = millis();
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    } else {
      char payload[6];
      sprintf(payload,"%s",f2s(temperature, 0));
      client.publish(topicCurrentTemperatureState, payload);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  sendCurrentTemperatureState();
}
