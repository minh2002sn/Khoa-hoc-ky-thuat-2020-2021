#include <Wire.h>
#include "MAX30100_PulseOximeter.h"           //Sensor
#include <PubSubClient.h>                     //MQTT
#include <ESP8266WiFi.h>                      //WiFi
#include <WiFiUdp.h>                          //Clock
#include <NTPClient.h>
#include <Adafruit_GFX.h>                     //Oled
#include <Adafruit_SSD1306.h>
#include "SoftwareSerial.h"                   //SIM
#include <ESP8266HTTPClient.h>                //Location
#include  <ArduinoJson.h>
#include "ESP8266WiFi.h"

//For WiFi and MQTT
const char* ssid = "Đẹp thì vô";
const char* pass = "daucatmoi";
const char* mqtt = "mqtt.ngoinhaiot.com";

//For Location
const char* Host = "www.unwiredlabs.com";
String endpoint = "/v2/process.php";
String token = "ad221ce71f1f02";
String jsonString = "{\n";
double latitude = 0.0;
double longitude = 0.0;

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

PulseOximeter pox;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP udp;
NTPClient ntp(udp, "0.vn.pool.ntp.org", 7*3600);
SoftwareSerial sim800l(D6,D5);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

#define bell D0
#define buttonpin D7

String SDT="84906887794";

unsigned long timer0 = 0;
unsigned long timer1 = 0;
unsigned long timer2 = 0;
unsigned long timer3 = 0;

char a1[350] = "Canh bao. Nhip tim (nhip/phut) hoac nong do bao hoa Oxi (%) trong mau cua ban dang nam ngoai muc binh thuong. Dieu do co the la bieu hien cua viec ban da lam co nhung hoat dong lam thay doi nhip tim/nong do oxi cua ban hoac cung co the la dau hieu cua cac benh ly ma ban chua biet. Hay den trung tam y te va xin y kien cua chuyen gia tu van.";
char a2[140] = "Khan cap. Nguoi dung dang can su giup do cua nguoi than va bac si. Toa do (Sao chep toa do va nhap vao google map) 10.762669, 106.680170";
char a3[150];

char msgHR[10];
char msgSPO2[10];

float hr = 0;
int spo2 = 0;
int spo2min = 95;
int spo2max = 100;

int counter = 0;
int counter1 = 0;

int buttonstatus = 0;
int lastbuttonstatus = 0;





void setWiFi(){
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println();
  Serial.println("WiFi Connecting");
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
}






void setMQTT(){
  client.setServer(mqtt, 1111);
  client.setCallback(callback);
  
  while(!client.connected()){
    delay(500);
    Serial.println("MQTT connecting");
    if(client.connect("ESP8266client", "minh2002sn", "minh2002sn")){
      Serial.println("Connected");
    }
    else{
      Serial.print("failed with state: ");
      Serial.println(client.state());
      delay(2000);
    }
  
  }
  client.publish("minh2002sn/BHYT/test", "Connected");
  client.publish("minh2002sn/BHYT/user", "Elders");
  client.subscribe("minh2002sn/BHYT/mode");
}






void setMAX30100(){
  Serial.print("Initializing pulse oximeter..");

  if (!pox.begin()) {
      Serial.println("FAILED");
      for(;;);
  } else {
      Serial.println("SUCCESS");
  }

  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);
}






void setOLED(){
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

}






void callback(char* topic, byte* payload, long int length){
  Serial.print("Message arrive: [ ");
  Serial.print(topic);
  Serial.print(" ]: ");
  for(int i=0; i<length; i++){
    Serial.print((char)payload[i]);
    if((char) payload[i] == '1'){
      spo2min = 95;
      spo2max = 100;
      client.publish("minh2002sn/BHYT/user", "Elders");
    } else{
      spo2min = 87;
      spo2max = 93;
      client.publish("minh2002sn/BHYT/user", "COPD");
    }
  }
  Serial.println();
  Serial.println(spo2min);
  Serial.println(spo2max);
}






void at(String _atcm,unsigned long _dl){
  sim800l.print(_atcm+"\r\n");
  delay(_dl);
}






void setup(){
  Serial.begin(115200);
  sim800l.begin(115200);

  pinMode(bell, OUTPUT);
  pinMode(buttonpin, INPUT);
  digitalWrite(bell, 0);

  setOLED();
  setWiFi();
  setMQTT();
  setMAX30100();

  showtime();
}






void loop(){
  client.loop();
  pox.update();
  ntp.update();
  DynamicJsonBuffer jsonBuffer;
  unsigned long delta = millis() - timer1;
  allarm();
  readbutton();

  
  if((millis() - timer0) >= 1000){
    if(hr > 45 and (hr <= 60 or hr >= 100)){
      counter++;
      Serial.println("a");  
    }
    if(spo2 > 50 and spo2 < 100 and (spo2 <= spo2min or spo2 >= spo2max)){
      counter++;
      Serial.println("b");
    }
    if(counter == 20){
      sendsms();
      Serial.println("WARNING");
      at("AT",1000);
      at("AT+CMGF=1",1000);
      at("AT+CSCS=\"GSM\"",1000);
      at("AT+CMGS=\"" + SDT+"\"",2000);
      at(a1,1000);
      sim800l.write(26);     // ctlrZ
    }
    if(delta >= 5000){
      if(delta < 10000){
        Serial.println("No finger");
        client.publish("minh2002sn/BHYT/State", "No finger");
        nofinger();
        counter = 0;
        hr = 0;
        spo2 = 0;
      } else{
        Serial.println("Date+Time");
        client.publish("minh2002sn/BHYT/State", "No finger");
        showtime();
        counter1 = 0;
      }
    }
  timer0 = millis();
  } 

  if(counter1 == 0){
    if((millis() - timer3) >= 10000){
      location();
      timer3 = millis();
      //Serial.println("sent");
    }
  }
}






// Callback (registered below) fired when a pulse is detected
void onBeatDetected(){
  //Serial.println("Beat!");
  counter1 = 1;
  hr = pox.getHeartRate();
  spo2 = pox.getSpO2();

  //MQTT
  client.publish("minh2002sn/BHYT/State", "Active");
  sprintf(msgHR, "%0.2f", hr);
  sprintf(msgSPO2, "%d", spo2);
  client.publish("minh2002sn/BHYT/HeartRate", msgHR);
  client.publish("minh2002sn/BHYT/SPO2", msgSPO2);

  //OLED
  display.clearDisplay();
  display.setTextSize(1,3);
  
  display.setCursor(0,0);
  display.print(hr);
  display.print(" bpm");

  display.setCursor(0,28);
  display.print(spo2);
  display.print(" %");

  display.display();

  timer1 = millis();
  
}






void location(){
  char bssid[6];
  DynamicJsonBuffer jsonBuffer;

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  //Serial.println("scan done");

  //if (n == 0 ) {
      //Serial.println("No networks available");
  //} else {
      //Serial.print(n);
      //Serial.println(" networks found");
  //}

  // now build the jsonString...
  jsonString = "{\n";
  jsonString += "\"token\" : \"";
  jsonString += token;
  jsonString += "\",\n";
  jsonString += "\"id\" : \"saikirandevice01\",\n";
  jsonString += "\"wifi\": [\n";
  for (int j = 0; j < n; ++j) {
    jsonString += "{\n";
    jsonString += "\"bssid\" : \"";
    jsonString += (WiFi.BSSIDstr(j));
    jsonString += "\",\n";
    jsonString += "\"signal\": ";
    jsonString += WiFi.RSSI(j);
    jsonString += "\n";
    if (j < n - 1) {
        jsonString += "},\n";
    } else {
        jsonString += "}\n";
    }
  }
  jsonString += ("]\n");
  jsonString += ("}\n");
  //Serial.println(jsonString);

  WiFiClientSecure client1;
  client1.setInsecure();

  //Connect to the client and make the api call
  //Serial.println("Requesting URL: https://" + (String)Host + endpoint);
  if (client1.connect(Host, 443)) {
    //Serial.println("Connected");
    client1.println("POST " + endpoint + " HTTP/1.1");
    client1.println("Host: " + (String)Host);
    client1.println("Connection: close");
    client1.println("Content-Type: application/json");
    client1.println("User-Agent: Arduino/1.0");
    client1.print("Content-Length: ");
    client1.println(jsonString.length());
    client1.println();
    client1.print(jsonString);
    delay(500);
  }

  //Read and parse all the lines of the reply from server
  while (client1.available()) {
    String line = client1.readStringUntil('\r');
    JsonObject& root = jsonBuffer.parseObject(line);
    if (root.success()) {
      latitude    = root["lat"];
      longitude   = root["lon"];
      //accuracy    = root["accuracy"];

      //Serial.println();
      //Serial.print("Latitude = ");
      //Serial.println(latitude, 6);
      //Serial.print("Longitude = ");
      //Serial.println(longitude, 6);
      //Serial.print("Accuracy = ");
      //Serial.println(accuracy);

      char x[15];
      char y[15];
    
      sprintf(x, "%0.6f", latitude);
      sprintf(y, "%0.6f", longitude);
      
      client.publish("minh2002sn/BHYT/Latitude", x);
      client.publish("minh2002sn/BHYT/Longitude", y);

      strcpy(a3, a2);
      strcat(a3, x);
      strcat(a3, ", ");
      strcat(a3, y);
    }
  }

  //Serial.println("closing connection");
  //Serial.println();
  client1.stop();
}






void readbutton(){
  buttonstatus = digitalRead(buttonpin);
  if (buttonstatus != lastbuttonstatus){
    if (buttonstatus == HIGH){
      sendsms();
      Serial.println("Send message");
      at("AT",1000);
      at("AT+CMGF=1",1000);
      at("AT+CSCS=\"GSM\"",1000);
      at("AT+CMGS=\"" + SDT+"\"",2000);
      at(a3,1000);
      sim800l.write(26);     // ctlrZ   
    }
  }
  lastbuttonstatus = buttonstatus;
}






void nofinger(){
  //No finger
  display.clearDisplay();  
  display.setCursor(27,1);
  display.setTextSize(1,3);
  display.println("No");
  display.setCursor(15,26);
  display.println("finger");
  display.display();
}






void showtime(){
  unsigned long epochTime = ntp.getEpochTime();

  display.clearDisplay();
  //Get time
  //Serial.print(ntp.getHours());  
  //Serial.print("h ");
  //Serial.print(ntp.getMinutes());
  //Serial.println("m");
  display.setTextSize(2,3);
  display.setCursor(4,0);
  display.print(ntp.getHours());
  display.print(":");
  display.print(ntp.getMinutes());

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;
  //Serial.print(monthDay);
  //Serial.print(", ");
  //Serial.print(currentMonth);
  //Serial.print(", ");
  //Serial.println(currentYear);
  display.setCursor(4,28);
  display.setTextSize(1,3);
  display.print(monthDay);
  display.print("/");
  display.print(currentMonth);
  display.print("/");
  display.print(currentYear);
  
  display.display();
}





void sendsms(){
  display.clearDisplay();
  display.setCursor(12,1);
  display.setTextSize(1,3);
  display.println("Sending");
  display.setCursor(24,26);
  display.println("SMS");
  display.display();
}






void allarm(){
  //Get time
  int h = ntp.getHours();
  int m = ntp.getMinutes();
  int s = ntp.getSeconds();

  //allarm
  if((h == 9 or h == 18) and m == 24){
    if(s <= 10){
      if(millis() - timer2 >= 500){
        digitalWrite(bell, !digitalRead(bell));
        timer2 = millis();
      }
    } else{
      timer2 = 0;
      digitalWrite(bell, 0);
    }
  }
}
