#include <PMS.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// =========================== Definicje pinów ===========================
#define SDA_PIN 6
#define SCL_PIN 7
#define RX_PIN 20         // RX dla PMS7003
#define TX_PIN 21         // TX dla PMS7003
#define RGB_PIN 4     // RGB LED pin 8 built in RGB LED
#define PIXELS_NUM 1      // Ilość diod LED NeoPixel

// ======================== Konfiguracja urządzenia ======================

// ======================== Debugowanie ===========================
#define DEBUG 1
#if DEBUG 
  #define DEBUG_PRINT(x) Serial.print(x);
  #define DEBUG_PRINTLN(x) Serial.println(x);
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)

#endif





// ======================== Obiekty globalne ======================
Adafruit_BME280 bme;
Adafruit_NeoPixel strip(PIXELS_NUM, RGB_PIN, NEO_GRB + NEO_KHZ800);
WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor("ESP32_weather_station");
HardwareSerial mySerial(0);
PMS pms(mySerial);
PMS::DATA data;

// ======================== Struktury danych ======================
struct PMS_data {
  uint16_t PM1 = NULL;
  uint16_t PM25 = NULL;
  uint16_t PM10 = NULL;
};

struct AQI_data {
  int AQI_PM25 = NULL;
  int AQI_PM10 = NULL;
  String AQI_PM25_desc;
};

struct BME280_data {
  float temperature;
  float pressure;
  float humidity;
};

// ======================== Zmienne globalne ======================
PMS_data PMS7003;
AQI_data AQIdata;
BME280_data BMEdata;
static int delay_time = 30; // Delay (s) dla PMS
static int send_data_delay = 60; // Delay (s) dla wysyłania do influxdb


// ======================== Funkcje pomocnicze ====================
void Save_data(PMS_data* pms_data, uint16_t PM1, uint16_t PM25, uint16_t PM10) {
  pms_data->PM1 = PM1;
  pms_data->PM25 = PM25;
  pms_data->PM10 = PM10;
}

void BME280_save() {
  BMEdata.humidity = bme.readHumidity();
  BMEdata.pressure = bme.readPressure() / 100.0F;
  BMEdata.temperature = bme.readTemperature();
}

void setRGBColor(uint8_t red, uint8_t green, uint8_t blue) {
  strip.setPixelColor(0, strip.Color(red, green, blue));
  strip.show();
}

int AQI_algorithm(uint16_t PM, uint16_t Clow, uint16_t Chigh, uint16_t Ilow, uint16_t Ihigh) {
  return int((float(Ihigh - Ilow) / (Chigh - Clow)) * (PM - Clow) + Ilow);
}

int AQI_for_PM25(uint16_t PM25) {
  if (PM25 <= 12) return AQI_algorithm(PM25, 0, 12, 0, 50);
  if (PM25 <= 35) return AQI_algorithm(PM25, 12, 35, 51, 100);
  if (PM25 <= 55) return AQI_algorithm(PM25, 35, 55, 101, 150);
  if (PM25 <= 150) return AQI_algorithm(PM25, 55, 150, 151, 200);
  if (PM25 <= 250) return AQI_algorithm(PM25, 150, 250, 201, 300);
  if (PM25 <= 350) return AQI_algorithm(PM25, 250, 350, 301, 400);
  if (PM25 <= 500) return AQI_algorithm(PM25, 350, 500, 401, 500);
  return 500;
}

int AQI_for_PM10(uint16_t PM10) {
  if (PM10 <= 54) return AQI_algorithm(PM10, 0, 54, 0, 50);
  if (PM10 <= 154) return AQI_algorithm(PM10, 54, 154, 51, 100);
  if (PM10 <= 254) return AQI_algorithm(PM10, 154, 254, 101, 150);
  if (PM10 <= 354) return AQI_algorithm(PM10, 254, 354, 151, 200);
  if (PM10 <= 424) return AQI_algorithm(PM10, 354, 424, 201, 300);
  if (PM10 <= 504) return AQI_algorithm(PM10, 424, 504, 301, 400);
  if (PM10 <= 604) return AQI_algorithm(PM10, 504, 604, 401, 500);
  return 500;
}

void Show_data_PMS(PMS_data data) {
  DEBUG_PRINTLN("====== POMIARY ======");
  DEBUG_PRINT("AQI PM2.5: ");
  DEBUG_PRINT(AQI_for_PM25(data.PM25));
  DEBUG_PRINT(" | AQI PM10: ");
  DEBUG_PRINTLN(AQI_for_PM10(data.PM10));
  DEBUG_PRINT("PM1.0: ");
  DEBUG_PRINT(data.PM1);
  DEBUG_PRINT(" µg/m³ | PM2.5: ");
  DEBUG_PRINT(data.PM25);
  DEBUG_PRINT(" µg/m³ | PM10: ");
  DEBUG_PRINTLN(data.PM10);
  DEBUG_PRINT("=====================");
}
void Request_data_PMS() {
  bool success = false;
  pms.requestRead();
  unsigned long StartTime = millis();

  while (!success && (millis() - StartTime < 5000)) {
    success = pms.readUntil(data);
  }
  if (success) {
    Save_data(&PMS7003, data.PM_AE_UG_1_0, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0);
    if (PMS7003.PM25 > 0 && PMS7003.PM10 > 0) {
      AQIdata.AQI_PM25 = AQI_for_PM25(PMS7003.PM25);
      AQIdata.AQI_PM10 = AQI_for_PM10(PMS7003.PM10);
      Show_data_PMS(PMS7003);
    }
  } else {
    Serial.println("Nie udało się pobrać danych, sprawdź czujnik!");
  }
}
void PMS_Task(void *pvParameters) {
  const int delay_time = *(int *)pvParameters;
  while (true) {
    Request_data_PMS();
    vTaskDelay(pdMS_TO_TICKS(delay_time * 1000));
  }
}

// ======================== Funkcje setup i loop ====================
void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Wire.begin(SDA_PIN, SCL_PIN);
  xTaskCreate(PMS_Task, "PMS Task", 2048, &delay_time, 2, NULL);
  bme.begin(0x76);
  strip.begin();
  strip.show();

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  sensor.addTag("Device_info", DEVICE_TYPE);
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  
}

void loop() {
  BME280_save();

  sensor.clearFields();
  sensor.addField("PM 1", PMS7003.PM1);
  sensor.addField("PM 25", PMS7003.PM25);
  sensor.addField("PM 10", PMS7003.PM10);
  sensor.addField("AQI index for PM 25", AQIdata.AQI_PM25);
  sensor.addField("AQI index for PM 10", AQIdata.AQI_PM10);
  sensor.addField("Temperature", BMEdata.temperature);
  sensor.addField("Humidity", BMEdata.humidity);
  sensor.addField("Pressure", BMEdata.pressure);

  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }

  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  delay(send_data_delay*1000);
}
