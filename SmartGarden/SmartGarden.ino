#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Adafruit_ADS1X15.h>

// Номер выхода, на котором висит силовой ключ
#define relay 12

// Пороговые значения аналогового датчика
#define sensor_min 0
#define sensor_max 25000

// Пороговые значения, к которым приводится сигнал с датчика
#define mapped_min 0
#define mapped_max 100

// Период включения реле в миллисекундах
#define relay_work_period 2000

// Период проверки датчика в миллисекундах
#define relay_check_period 5000

// Контрольное значение, при котором будет включаться реле
#define control_value 70

// Имя и пароль беспроводной сети
#define STASSID "lol"
#define STAPSK  "kekcheburek"

// Данные для подключения к mqtt брокеру
#define mqtt_server "192.168.31.16"
#define mqtt_port 1883

// Топики для отправки
#define watering_topic "/class/stand3/pump"
#define humidity_topic "/class/stand3/humidity"
#define commands_topic "/class/stand3/commands"

// Размер сообщения в байтах
#define MSG_BUFFER_SIZE  (100)



const char* ssid     = STASSID;
const char* password = STAPSK;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
int value = 0;
Adafruit_ADS1115 ads;
bool isRelayOn = false;
bool isRelayForceOn = false;
unsigned long relayTimer = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  String currentTopic = String(topic);
  if (currentTopic == commands_topic){
    isRelayForceOn = true;
  }
   
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");

      client.publish("outTopic", "hello world");

      client.subscribe(commands_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      delay(5000);
    }
  }
}


void setup() {
  pinMode(relay, OUTPUT);
  // put your setup code here, to run once:
  Serial.begin(115200);

  ads.setGain(GAIN_TWOTHIRDS);  

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }
  ads.startComparator_SingleEnded(0, 1000);

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

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
}

void sendMqttMessage(char *topic, String payload){
    snprintf (msg, MSG_BUFFER_SIZE, "%s", payload);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(topic, msg);
}

void loop() {
  int16_t adc0;
  adc0 = ads.getLastConversionResults();
  adc0 = map(adc0, sensor_min, sensor_max, mapped_min, mapped_max);
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (millis() % relay_check_period == 0){
    Serial.print("AIN0: "); Serial.println(adc0);
    sendMqttMessage(humidity_topic, String(adc0));

    if (adc0 > control_value && !isRelayOn || isRelayForceOn){
      digitalWrite(relay, 1);
      isRelayOn = !isRelayOn;
      relayTimer = millis();
      sendMqttMessage(watering_topic, "watering");
    }
  }

  if(isRelayForceOn && !isRelayOn){
    digitalWrite(relay, 1);
    isRelayOn = !isRelayOn;
    isRelayForceOn = false;
    relayTimer = millis();
    sendMqttMessage(watering_topic, "watering1");
  }
  
  if (millis()-relayTimer>relay_work_period && isRelayOn){
    isRelayOn = !isRelayOn;
    isRelayForceOn = false;
    relayTimer = 0;
    digitalWrite(relay, 0);
  }
}
