#include <SPI.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>


// variables of INTERNET CONNECTION
//===========================
IPAddress catalog_ip(192, 168, 1, 18);
const int broker_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);
const char* ssid = "NETGEAR23";
const char* password = "quaintcoconut226";
const String clientID = "00124";
unsigned long lastConnect_mqtt = 0;
unsigned long T_reconnect_MQTT = 0;

//SENSOR INFO
//==============================
const String WaterLevel = "WATERLEVEL-";
const String Photocell = "PHOTOCELL-";
const String Switch = "SWITCH-";
const String SoilMoisture = "SOILMOISTURE-";
int MY_number_sensors = 0;
struct SensorInfo {
  String Name;
  String Status;
  String measure;
  long timestamp;
  long measure_refresh;
  int measure_stability;
  bool flag;
};
struct SensorInfo Sensors[3];
String receiver_sensor_list[3];
int my_number_receiver = 0;
unsigned long now = 0;
long mqtt_keep_alive = 10 * 1000;
long measure_refresh = 1 * 1000;
int measure_resolution = 1;
int measure_stability = 0;


void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // wait for serial port to connect. Needed for native USB port only
  }
  //
  WiFi.begin(ssid, password);
  WIFI_CONNECT();
  client.setServer(catalog_ip, 1883);
  client.setCallback(callback);
  PHOTOCELL("0");
  Serial.println("DEVICE INITILIZED!");
}

void loop() {
  now = millis();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  PHOTOCELL("0");
}

void WIFI_CONNECT() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte *payload, unsigned int length) {
  Serial.print("message for ");
  int i = 0;
  String msg_info[4] = {};
  message_split(const_cast<char*>(topic), &msg_info[0]);
  String mittente = msg_info[0];
  String S_name = msg_info[1];
  //_________________________________

  for (byte j = 0; j < MY_number_sensors; j++) {
    if (S_name == Sensors[j].Name) {
      Serial.print("Sensor: " + S_name);
      String msgIn;
      for (int j = 0; j < length; j++) {
        msgIn = msgIn + char(payload[j]);
      }
      Sensors[j].measure = msgIn;
      break;
    }
  }
}

void message_split(char* msgCopy, String *msg_info) {
  unsigned int i = 0;
  char* token;
  while ((token = strtok_r(msgCopy, "/", &msgCopy)) != NULL) {
    msg_info[i] = String(token);
    i = i + 1;
  }
}

void SWITCH(String idx) {
  String S_name = Switch + clientID + idx;
  int pin_SWITCH = 12;
  int flag_syncro = 0;
  for (byte i = 0; i < MY_number_sensors; i++) {
    if (Sensors[i].Name == S_name) {
      flag_syncro = 1;
      // REFRESH-CICLE ----------------------------------
      if (now - Sensors[i].timestamp > mqtt_keep_alive) {
        Sensors[i].flag = true;
      }

      // DUTY-CICLE ----------------------------------
      if (Sensors[i].measure != Sensors[i].Status) {
        Sensors[i].flag = true;
        if (Sensors[i].measure == "on") {
          digitalWrite(pin_SWITCH, HIGH);
        }
        else {
          digitalWrite(pin_SWITCH, LOW);
        }
      }
      // PUBLISH-CICLE ----------------------------------
      if (Sensors[i].flag == true) {
        Sensors[i].flag = false;
        Sensors[i].timestamp = now;
        Sensors[i].Status = Sensors[i].measure;
        MQTT_PUBLISH(i);
      }
      break;
    }
  }
  // SETUP -----------------------------------------
  if (flag_syncro == 0) {
    Sensors[MY_number_sensors] = {S_name, "status", "off", now, now, 0, true};
    MY_number_sensors += 1;
    receiver_sensor_list[my_number_receiver] = S_name;
    my_number_receiver += 1;
    pinMode(pin_SWITCH, OUTPUT);
    Serial.println("setup device " + S_name);
  }
}

void PHOTOCELL(String idx) {
  String S_name = Photocell + clientID + idx;
  const int pin_SENSOR = A0;
  int flag_syncro = 0;
  for (byte i = 0; i < MY_number_sensors; i++) {
    if (Sensors[i].Name == S_name) {
      flag_syncro = 1;
      // REFRESH-CICLE ----------------------------------
      if (now - Sensors[i].timestamp > mqtt_keep_alive) {
        Sensors[i].flag = true;
      }
      int measure;
      //DUTY-CICLE--------------------------------------
      if (now - Sensors[i].measure_refresh > measure_refresh || Sensors[i].flag == true) {
        Sensors[i].measure_refresh = now;
        measure = analogRead(pin_SENSOR);
        delay(5);
        if ( abs(measure - Sensors[i].measure.toInt()) > measure_resolution) {
          Sensors[i].measure_stability += 1;
        }
      }
      if (Sensors[i].measure_stability > measure_stability) {
        Sensors[i].flag = true;
        Sensors[i].measure_stability = 0;
      }
      //PUBLISH-CICLE-----------------------------------------
      if (Sensors[i].flag == true) {
        Sensors[i].measure = String(measure);
        Sensors[i].flag = false;
        Sensors[i].timestamp = now;
        MQTT_PUBLISH(i);
      }
      break;
    }
  }
  // SETUP -----------------------------------------
  if (flag_syncro == 0) {
    Sensors[MY_number_sensors] = {S_name, "status", "0", now, now, 0, true};
    MY_number_sensors += 1;
    Serial.println("setup device " + S_name);
  }
}

void SOIL_MOISTURE(String idx) {
  String S_name = SoilMoisture + clientID + idx;
  const int pin_SENSOR = A0;
  int flag_syncro = 0;
  for (byte i = 0; i < MY_number_sensors; i++) {
    if (Sensors[i].Name == S_name) {
      flag_syncro = 1;
      // REFRESH-CICLE ----------------------------------
      if (now - Sensors[i].timestamp > mqtt_keep_alive) {
        Sensors[i].flag = true;
      }
      //DUTY-CICLE------------------------------------------
      int measure;
      if (now - Sensors[i].measure_refresh > measure_refresh || Sensors[i].flag == true) {
        Sensors[i].measure_refresh = now;
        measure = analogRead(pin_SENSOR);
        delay(5);
        if ( abs(measure - Sensors[i].measure.toInt()) > measure_resolution) {
          Sensors[i].measure_stability += 1;
        }
      }
      if (Sensors[i].measure_stability > measure_stability) {
        Sensors[i].flag = true;
        Sensors[i].measure_stability = 0;
      }
      //PUBLISH-CICLE-----------------------------------------
      if (Sensors[i].flag == true) {
        Sensors[i].measure = String(measure);
        Sensors[i].flag = false;
        Sensors[i].timestamp = now;
        MQTT_PUBLISH(i);
      }
      break;
    }
  }
  // SETUP -----------------------------------------
  if (flag_syncro == 0) {
    Sensors[MY_number_sensors] = {S_name, "status", "0", now, now, 0, true};
    MY_number_sensors += 1;
    Serial.println("setup device " + S_name);
  }
}


void WATER_LEVEL(String idx) {
  String S_name = WaterLevel + clientID + idx;
  const int trigPin = 16;
  const int echoPin = 5;
  int flag_syncro = 0;
  for (byte i = 0; i < MY_number_sensors; i++) {
    if (Sensors[i].Name == S_name) {
      flag_syncro = 1;
      // REFRESH-CICLE ---------------------
      if (now - Sensors[i].timestamp > mqtt_keep_alive) {
        Sensors[i].flag = true;
      }
      //DUTY-CICLE--------------------------
      String measure;
      if (now - Sensors[i].measure_refresh > measure_refresh || Sensors[i].flag == true) {
        Sensors[i].measure_refresh = now;
        delay(5);
        digitalWrite(trigPin, LOW);
        delayMicroseconds(1);
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);
        long duration = pulseIn(echoPin, HIGH);
        float distance = float(duration * 0.034) / 2.0;
        measure = String(distance, 2);
        if ( abs(measure.toFloat() - Sensors[i].measure.toFloat()) > float(measure_resolution) ) {
          Sensors[i].measure_stability += 1;
        }
      }
      if (Sensors[i].measure_stability > measure_stability) {
        Sensors[i].flag = true;
        Sensors[i].measure_stability = 0;
      }
      //PUBLISH-CICLE----------------------------
      if (Sensors[i].flag == true) {
        Sensors[i].measure = measure;
        Sensors[i].flag = false;
        Sensors[i].timestamp = now;
        MQTT_PUBLISH(i);
      }
      break;
    }
  }
  // SETUP -----------------------------------------
  if (flag_syncro == 0) {
    Sensors[MY_number_sensors] = {S_name, "status", "0", now, now, 0, true};
    MY_number_sensors += 1;
    Serial.println("setup device " + S_name);
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(trigPin, LOW);
    digitalWrite(echoPin, LOW);
  }
}


void MQTT_PUBLISH(byte i) {
  String topic = "device/" + Sensors[i].Name;
  client.publish(const_cast<char*>(topic.c_str()), const_cast<char*>(Sensors[i].measure.c_str()));
  Serial.println("send sensor messange " + Sensors[i].measure);
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    // Attempt to connect
    if (client.connect(const_cast<char*>(clientID.c_str()))) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      if (my_number_receiver == 0) {
        client.subscribe("inTopic");
      }
      else {
        for (byte j = 0; j < my_number_receiver; j++) {
          String topic = "setting/" + receiver_sensor_list[j];
          client.subscribe(const_cast<char*>(topic.c_str()));
          delay(5);
        }
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
