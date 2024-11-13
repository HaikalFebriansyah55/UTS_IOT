#include <WiFi.h>            // Gunakan <WiFi.h> jika menggunakan ESP32
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define DHTPIN 4                   // Pin data DHT (ubah sesuai kebutuhan)
#define DHTTYPE DHT11              // Ubah ke DHT11 jika menggunakan DHT11
#define LDR_PIN 32                 // Pin LDR (ubah sesuai kebutuhan)
#define RED_PIN 19                 // Pin LED merah
#define YELLOW_PIN 18              // Pin LED kuning
#define GREEN_PIN 5                // Pin LED hijau
#define BUZZER_PIN 33
#define RELAY_PIN 35
DHT dht(DHTPIN, DHTTYPE);

// Konfigurasi WiFi
const char* ssid = "Pixel";
const char* password = "12345670";

// Konfigurasi MQTT
const char* mqtt_server = "192.168.45.116"; // Ganti dengan IP lokal komputer yang menjalankan Mosquitto
const int mqtt_port = 1885;
const char* sensor_data_topic = "sensor/data";  // Topik tunggal untuk semua data

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LDR_PIN, INPUT);  // Atur pin LDR sebagai input
  pinMode(RED_PIN, OUTPUT);  // Atur pin LED merah sebagai output
  pinMode(YELLOW_PIN, OUTPUT); // Atur pin LED kuning sebagai output
  pinMode(GREEN_PIN, OUTPUT);  // Atur pin LED hijau sebagai output
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {  // Ganti dengan "ESP32Client" jika menggunakan ESP32
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Baca data suhu, kelembaban, dan LDR
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int ldr_value = analogRead(LDR_PIN);

  // Pastikan data valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Dapatkan timestamp (misalnya dalam milidetik sejak perangkat menyala)
  unsigned long timestamp = millis();

  // Buat objek JSON untuk menggabungkan data
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  jsonDoc["ldr"] = ldr_value;
  jsonDoc["timestamp"] = timestamp;

  // Serialisasi JSON ke string
  char jsonBuffer[200];
  serializeJson(jsonDoc, jsonBuffer);

  // Kirim data ke topik MQTT tunggal
  client.publish(sensor_data_topic, jsonBuffer);

  // Tampilkan data di Serial Monitor
  Serial.println("Data JSON yang dikirim:");
  Serial.println(jsonBuffer);

  // Kontrol LED berdasarkan nilai LDR
  if (temperature > 35) {  // Kondisi untuk lampu Merah
    digitalWrite(RED_PIN, HIGH);
    digitalWrite(YELLOW_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(RELAY_PIN, HIGH);
  } else if (temperature < 35 && temperature > 30) {  // Kondisi untuk lampu kuning
    digitalWrite(RED_PIN, LOW);
    digitalWrite(YELLOW_PIN, HIGH);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);
  } else {  // Kondisi untuk lampu Hijau
    digitalWrite(RED_PIN, LOW);
    digitalWrite(YELLOW_PIN, LOW);
    digitalWrite(GREEN_PIN, HIGH);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);
  }

  delay(2000);  // Kirim data setiap 2 detik
}
