#include <PMS.h>
#include <Adafruit_NeoPixel.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
// =========================== ESP32 PINS ===========================
#define SDA_PIN 6
#define SCL_PIN 7
#define RX_PIN 20     // RX for PMS7003
#define TX_PIN 21     // TX for PMS7003
#define RGB_PIN 4     // RGB LED
#define PIXELS_NUM 2  // Number of RGB leds
// ======================== InfluxDB configuration ======================
// ======================== Debug ===========================

//Change to 1 to see log messages
#define DEBUG 1
#if DEBUG
#define Debug(x) Serial.print(x);
#define Debugln(x) Serial.println(x);
#else
#define Debugln(x)
#define Debug(x)
#endif
//Change to  1 to see PMS and BME sensor data
#define SHOW_PMS 0
//Turn on or off RGB leds
#define RGB_MODE 1
#define BRIGHTNESS_PERCENT 5  //0-100%
// ======================== Obiekty globalne ======================
Adafruit_BME280 bme;
Adafruit_NeoPixel strip(PIXELS_NUM, RGB_PIN, NEO_GRB + NEO_KHZ800);
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor("ESP32_weather_station");
HardwareSerial mySerial(0);
PMS pms(mySerial);
PMS::DATA data;

// ======================== Data ======================
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

// ======================== Global Variables ======================
PMS_data PMS7003;
AQI_data AQIdata;
BME280_data BMEdata;
static int delay_time = 20;      // Delay in seconds for collecting PMS sensor data
static int send_data_delay = 60;  // Delay in seconds to send data to Influx DB
unsigned long prevMillisreconnect = 0;
unsigned long reconnectTimeout = 15;  // Delay in seconds, timeout for reconnecting
// ======================== Functions ====================
void setRGBleds(uint8_t red, uint8_t green, uint8_t blue, int num_leds) {
  strip.fill(strip.Color(red, green, blue), 0, num_leds);
  strip.show();
}
void RGBAnimation(int num_leds) {
  for (int i = 0; i <= 255; i++) {
    strip.fill(strip.Color(0, 0, i), 0, num_leds);
    strip.show();
    delay(2);
  }
  for (int x = 255; x >= 0; x--) {
    strip.fill(strip.Color(0, 0, x), 0, num_leds);
    strip.show();
    delay(2);
  }
  strip.fill(strip.Color(0, 0, 0), 0, num_leds);
}

void Save_data(PMS_data *pms_data, uint16_t PM1, uint16_t PM25, uint16_t PM10) {
  pms_data->PM1 = PM1;
  pms_data->PM25 = PM25;
  pms_data->PM10 = PM10;
}
void Request_data_BME() {
  BMEdata.humidity = bme.readHumidity();
  BMEdata.pressure = bme.readPressure() / 100.0F;
  BMEdata.temperature = bme.readTemperature();
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
void DisplayAQIColor(int AQI_value) {
  if (AQI_value <= 50) {
    setRGBleds(0, 255, 0, PIXELS_NUM);  //Green
  } else if (AQI_value <= 100) {
    setRGBleds(255, 255, 0, PIXELS_NUM);  //Yellow
  } else if (AQI_value <= 150) {
    setRGBleds(255, 165, 0, PIXELS_NUM);  //Orange
  } else if (AQI_value <= 200) {
    setRGBleds(255, 0, 0, PIXELS_NUM);  //Red
  } else if (AQI_value <= 300) {
    setRGBleds(128, 0, 128, PIXELS_NUM);  //Violet
  } else {
    setRGBleds(255, 255, 255, PIXELS_NUM);  // White
  }
}
void Show_data_PMS(PMS_data data) {
  Serial.println("====== POMIARY ======");
  Serial.print("AQI PM2.5: ");
  Serial.print(AQI_for_PM25(data.PM25));
  Serial.print(" | AQI PM10: ");
  Serial.println(AQI_for_PM10(data.PM10));
  Serial.print("PM1.0: ");
  Serial.print(data.PM1);
  Serial.print(" µg/m³ | PM2.5: ");
  Serial.print(data.PM25);
  Serial.print(" µg/m³ | PM10: ");
  Serial.println(data.PM10);
  Serial.print("T: ");
  Serial.print(BMEdata.temperature);
  Serial.print(" *c | H: ");
  Serial.print(BMEdata.humidity);
  Serial.print(" % | P: ");
  Serial.print(BMEdata.temperature);
  Serial.println(" hPa");
  Serial.print("=====================");
}
//This function is called with specified interval using FreeRTOS
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
#if RGB_MODE
      DisplayAQIColor(AQIdata.AQI_PM25);
#endif
#if SHOW_PMS
      Show_data_PMS(PMS7003);
#endif
    }
  } else {
    Debugln("Can't retrieve data from PMS sensor, check it!");
  }
}
void PMS_Task(void *pvParameters) {
  const int delay_time = *(int *)pvParameters;
  while (true) {
    Request_data_BME();  // BME280
    Request_data_PMS();
    vTaskDelay(pdMS_TO_TICKS(delay_time * 1000));
  }
}
void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Wire.begin(SDA_PIN, SCL_PIN);
  bme.begin(0x76);
  strip.begin();
  strip.show();
  strip.setBrightness(BRIGHTNESS_PERCENT);
  strip.fill(strip.Color(0, 0, 0), 0, PIXELS_NUM);
  xTaskCreate(PMS_Task, "PMS Task", 2048, &delay_time, 2, NULL);
  RGBAnimation(PIXELS_NUM);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Debugln("Connecting to WiFi.");
  while (WiFi.status() != WL_CONNECTED) {
    Debug('.');
    delay(500);
  }
  Debug("\n Connected. IP address:");
  Debugln(WiFi.localIP());
  Debug("RSSI:");
  Debugln(WiFi.RSSI());
  sensor.addTag("Device_info", DEVICE_TYPE);
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  if (client.validateConnection()) {
    Debug("Connected to InfluxDB: ");
    Debugln(client.getServerUrl());
  } else {
    Debug("InfluxDB connection failed: ");
    Debugln(client.getLastErrorMessage());
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Debugln("Wifi connection lost..");
    unsigned long reconnectMillis = millis();
    if (reconnectMillis - prevMillisreconnect >= reconnectTimeout * 1000) {
      prevMillisreconnect = reconnectMillis;
      WiFi.disconnect();
      WiFi.reconnect();
      Debugln("Reconnecting...");
    }
  } else {
    //If connected send data
    sensor.clearFields();
    sensor.addField("PM 1", PMS7003.PM1);
    sensor.addField("PM 25", PMS7003.PM25);
    sensor.addField("PM 10", PMS7003.PM10);
    sensor.addField("AQI index for PM 25", AQIdata.AQI_PM25);
    sensor.addField("AQI index for PM 10", AQIdata.AQI_PM10);
    sensor.addField("Temperature", BMEdata.temperature);
    sensor.addField("Humidity", BMEdata.humidity);
    sensor.addField("Pressure", BMEdata.pressure);
    if (!client.writePoint(sensor)) {
      Debug("InfluxDB write failed: ");
      Debugln(client.getLastErrorMessage());
    } else {
      Debugln("Data sent to InfluxDB");
    }
  }
  delay(send_data_delay * 1000);
}
