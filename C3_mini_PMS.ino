#include <PMS.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 8        
#define PIXELS_NUM 1     
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS_NUM, RGB_PIN, NEO_GRB + NEO_KHZ800);

#define RX_PIN 20        // Pin RX dla PMS7003
#define TX_PIN 21        // Pin TX dla PMS7003

static int delay_time = 5; // Get data from PMS with seconds of delay 

HardwareSerial mySerial(0); 
PMS pms(mySerial);
PMS::DATA data;

struct PMS_data {
  uint16_t PM1;
  uint16_t PM25;
  uint16_t PM10;
};
PMS_data PMS7003;

void Save_data(PMS_data* pms_data, uint16_t PM1, uint16_t PM25, uint16_t PM10) {
  pms_data->PM1 = PM1;
  pms_data->PM25 = PM25;
  pms_data->PM10 = PM10;
}

void setRGBColor(uint8_t red, uint8_t green, uint8_t blue) {
  strip.setPixelColor(0, strip.Color(red, green, blue)); 
  strip.show(); 
}
//Clow - dolna granice stezenia Chigh - gorna granica stezenia, Ilow-dolna granica indeksu, IHigh - Gorna granica indeksu
int AQI_algorithm(uint16_t PM, uint16_t Clow, uint16_t Chigh, uint16_t Ilow, uint16_t Ihigh) {
  return int((float(Ihigh - Ilow) / (Chigh - Clow)) * (PM - Clow) + Ilow);
}

// AQI dla PM2.5 (µg/m³) zgodnie z US EPA AQI
int AQI_for_PM25(uint16_t PM25) {
  if (PM25 <= 12) {
    return AQI_algorithm(PM25, 0, 12, 0, 50);
  } else if (PM25 <= 35) {
    return AQI_algorithm(PM25, 12, 35, 51, 100);
  } else if (PM25 <= 55) {
    return AQI_algorithm(PM25, 35, 55, 101, 150);
  } else if (PM25 <= 150) {
    return AQI_algorithm(PM25, 55, 150, 151, 200);
  } else if (PM25 <= 250) {
    return AQI_algorithm(PM25, 150, 250, 201, 300);
  } else if (PM25 <= 350) { // Dodałem górny przedział, aby był zgodny z AQI
    return AQI_algorithm(PM25, 250, 350, 301, 400);
  } else if (PM25 <= 500) {
    return AQI_algorithm(PM25, 350, 500, 401, 500);
  } else {
    return 500; // Maksymalny AQI
  }
}

// AQI dla PM10 (µg/m³) zgodnie z US EPA AQI
int AQI_for_PM10(uint16_t PM10) {
  if (PM10 <= 54) {
    return AQI_algorithm(PM10, 0, 54, 0, 50);
  } else if (PM10 <= 154) {
    return AQI_algorithm(PM10, 54, 154, 51, 100);
  } else if (PM10 <= 254) {
    return AQI_algorithm(PM10, 154, 254, 101, 150);
  } else if (PM10 <= 354) {
    return AQI_algorithm(PM10, 254, 354, 151, 200);
  } else if (PM10 <= 424) {
    return AQI_algorithm(PM10, 354, 424, 201, 300);
  } else if (PM10 <= 504) {
    return AQI_algorithm(PM10, 424, 504, 301, 400);
  } else if (PM10 <= 604) {
    return AQI_algorithm(PM10, 504, 604, 401, 500);
  } else {
    return 500; // Maksymalny AQI
  }
}

void Show_data(PMS_data data) {
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
    Serial.print(data.PM10);
    Serial.println(" µg/m³");
    Serial.println("=====================");
}

void Request_data_PMS(){
  bool success = false;
  
  pms.wakeUp();
  vTaskDelay(pdMS_TO_TICKS(5000));  //Stable readings
  pms.requestRead();
  unsigned long StartTime = millis();
  while(!success && (millis() - StartTime < 5000)){
    success = pms.readUntil(data);
  }
  if (success) {
    setRGBColor(0,255,0);
    Save_data(&PMS7003, data.PM_AE_UG_1_0, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0);
    pms.sleep(); 
    Show_data(PMS7003);
  }else{
    pms.sleep();
    Serial.println("Nie udało się pobrać danych, sprawdź czujnik!");
    setRGBColor(255,0,0);
  }
  setRGBColor(0,0,0);

}
//FreeRTOS
void PMS_Task(void *pvParameters){
  const int delay_time = *(int *)pvParameters;
  while(true){
    Request_data_PMS();
    vTaskDelay(pdMS_TO_TICKS(delay_time * 1000));
  }
}
void setup() {
  //Serial
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  //PMS conf
  pms.passiveMode(); //Manual control of PMS sensor 
  //RGB CONF
  strip.begin();
  strip.show(); 
  //FreeRTOS for getting PMS data with delay without intercepting loop
  xTaskCreate(PMS_Task,"PMS Task",2048,&delay_time,2,NULL);
  


}
void loop() {


}