<div align="center">
<h1>AirForecast Open Source Project</h1>
</div>

**AirForecast** was designed to collect and visualize air quality data, as well as predict future values using the [Holt-Winters](https://www.pmorgan.com.au/tutorials/holt-winters-method-and-northam-temperature-data/) model. It utilizes **InfluxDB** for data storage and **Grafana** for visualization. Two **RGB LEDs** provide instant AQI feedback without needing a screen. This project was inspired by the increasing frequency of wildfires worldwide. **AirForecast** helps users stay informed about air quality and potential health risks.


## 🚀 Project Features  
- **2 RGB LEDs** – Instant AQI (PM2.5) visualization  
- **Grafana** – Interactive air quality data visualization  
- **InfluxDB** – Stores data for up to **30 days**  
- **Holt-Winters Model** – Predicts future air quality trends  
- **Environmental Sensors** – Measures **PM1.0, PM2.5, PM10, temperature, humidity, and pressure**  


## 📊 **Grafana Dashboard Output**  
<div align="center">
<img src="Pictures/Grafana_visualize.png" width="800">
</div>

---
<div align="center">
<h1>How to build this project</h1>
</div>


## 🔧 **Required Components**  
- **ESP32** development board  
- **PMS7003** (PM1.0, PM2.5, PM10 air quality sensor)  
- **BME280** (temperature, humidity, pressure sensor)  
- **2 RGB LEDs** (NeoPixel)  



## ⚡ **Pinout & Wiring**  

| Component  | ESP32 C3 Mini Pins |
|------------|---------|
| PMS7003 RX | **21** |
| PMS7003 TX | **20** |
| BME280 SDA | **6** |
| BME280 SCL | **7** |
| RGB LED    | **4** |



## 📡 **WiFi & InfluxDB Configuration**  

### **1️⃣ Creating a New Bucket**  
Follow these steps to create the **"PMS sensor project"** bucket in **InfluxDB**:

➡️ **Step 1:** Log in to **InfluxDB Cloud** at [https://cloud2.influxdata.com](https://cloud2.influxdata.com)  
➡️ **Step 2:** In the left menu, go to **Load Data** → **Buckets**  
➡️ **Step 3:** Click **+ CREATE BUCKET**  
➡️ **Step 4:** Enter **Bucket Name**: `PMS sensor project`  
➡️ **Step 5:** Set **Retention Period** (e.g., `30 days`)  
➡️ **Step 6:** Click **Create** to save the bucket  

### **2️⃣ Generating an API Token**  
To send data to InfluxDB, you need an **API Token**:

➡️ **Step 1:** In the left menu, go to **Load Data** → **API Tokens**  
➡️ **Step 2:** Click **+ Generate API Token**  
➡️ **Step 3:** Select **All Access Token** or **Custom Token**  
➡️ **Step 4:** (For **Custom Token**) → Give **Read & Write** permissions for your bucket  
➡️ **Step 5:** Click **Generate Token**  
➡️ **Step 6:** Copy and **save** the API Token (you **cannot view it again**!)  



### **3️⃣ Updating Your Configuration and uploading**  
Once you have your **Bucket Name** and **API Token**, update your configuration and upload sketch to ESP32 C3 Mini:

```cpp
#define WIFI_SSID "Your_WiFi_SSID"          // WiFi network name
#define WIFI_PASSWORD "Your_WiFi_Password"  // WiFi password

#define INFLUXDB_URL "http://your-influxdb-server"  // InfluxDB server URL
#define INFLUXDB_TOKEN "your-api-token"             // API Token from InfluxDB
#define INFLUXDB_ORG "your-organization"            // Organization name in InfluxDB
#define INFLUXDB_BUCKET "PMS sensor project"        // Your newly created bucket
```
## 📊 Setting Up Grafana to Fetch Data from InfluxDB  
Adding InfluxDB as a Data Source

➡️ **Step 1:** Open **Grafana** and log in  
➡️ **Step 2:** Click on the ⚙️ **Settings** icon in the left menu  
➡️ **Step 3:** Select **Data Sources**  
➡️ **Step 4:** Click the **+ Add data source** button  
➡️ **Step 5:** Choose **InfluxDB** from the list  

##  Configuring InfluxDB Connection  

###  **Basic Settings**  
- **Name:** `PMS sensor project` *(or any name you prefer)*  
- Select **Flux** (the preferred query language for InfluxDB 2.0)  

###  **HTTP Settings**  
- **URL:** https://us-east-1-1.aws.cloud2.influxdata.com *(This URL may vary depending on your InfluxDB cloud region—check your InfluxDB dashboard.)*
###  **Enable Basic Authentication**  
- Toggle **Basic auth** ✅
###  **Basic Auth Details**  
- **User:** *(Your InfluxDB email, e.g., `your-email@example.com`)*  
- **Password:** *(Use your password for influxdb account)*
  

###  **InfluxDB Details**  
- **Organization:** *(Your InfluxDB organization)*  
- **Token:** *(Paste the API Token generated from InfluxDB.)*  
##  Dashboards in Grafana
Import a JSON file to your dashboard. You can find a pre-configured dashboard in the files.

