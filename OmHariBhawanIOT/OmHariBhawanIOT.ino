#define _DISABLE_TLS_
#define _DEBUG_
//#define THINGER_SERVER "65.0.130.160"
//#define THINGER_PORT 9000
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23X17.h>

#include <EEPROM.h>

#include <NTPClient.h>
#include <TimeLib.h>
#include <WiFiManager.h>
#include <ThingerESP8266.h>
#include <WiFiUdp.h>

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <ThingerConsole.h>
#include <IRrecv.h>
#include <IRutils.h>

#include "slpashLogo.h"
#include "arduino_secrets.h"


// int D0 = 16, D1 = 5, D2 = 4, D3 = 0, D4 = 2, D5 = 14, D6 = 12,D7 = 13, D8 = 15, D00 = 22;
static const uint8_t D00 = 22;
int AP_timeout = 40;

const uint16_t kRecvPin = D6; IRrecv irrecv(kRecvPin); decode_results results; // IR

#define OLED_SDIN D7 //MOSI (Connect to display D1 or SDA)
#define OLED_SCLK D5 //SCLK (Connect to display D0 or SCL)
#define OLED_DC   D4 //Connect to display DC
#define OLED_RST  D3 //Connect to display RES
#define OLED_CS   D00 //Not connected to display, use a spare pin (e.g. D0 is NodeMCU LED)

#define MCP_HIGH 1
#define MCP_LOW  0

Adafruit_SSD1306 display(OLED_SDIN, OLED_SCLK, OLED_DC, OLED_RST, OLED_CS);
Adafruit_MCP23X17 mcp;
ThingerESP8266 thing(THINGER_USERNAME, THINGER_DEVICE_ID, THINGER_DEVICE_CREDENTIAL);
ThingerConsole console(thing);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "in.pool.ntp.org", 19800);
int timeClient_getDayMinutes(){
  return (timeClient.getHours()*60)+timeClient.getMinutes();
}
String timeClient_getFormattedTime() {
  unsigned long rawTime = timeClient.getEpochTime();
  unsigned long hours = (rawTime % 86400L) / 3600;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  unsigned long seconds = rawTime % 60;
  String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

  return hoursStr + ":" + minuteStr + ":" + secondStr;
}
String timeClient_getClockTime() {
  unsigned long rawTime = timeClient.getEpochTime();
  unsigned long hours = (rawTime % 86400L) / 3600;
  hours=hours > 12 ? hours-12 : hours;
  hours=hours == 0 ? 12 : hours;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  return hoursStr + ":" + minuteStr;
}
char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char monthOfYear[12][12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
String timeClient_getDayName() {
	unsigned long t = timeClient.getEpochTime();
	int d = timeClient.getDay();
	return daysOfTheWeek[d];
}

int time_status = 0;

const int trigPin = D0; // FOr Distance Sensor
const int echoPin = D8; // FOr Distance Sensor
long duration; // FOr Distance Sensor
int distance; // FOr Distance Sensor

int swval[] = {0,
               0, 0, 0, 0, 0, 0, 0, 0,
               0, 0, 0, 0, 0, 0, 0, 0,
               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
              };
int t = 0, i = 0, wifi_state = 0;
long dis_freej = 0;
int device_status = 1;
unsigned long schedule_check = 0;
unsigned long water_check = 0;
unsigned long temprature_check = 0;
int water_level = 0;
void setup() {
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  Serial.begin(9600);
  EEPROM.begin(512);
  mcp.begin_I2C();
  for (i = 1; i <= 8; i++) {
    mcp.pinMode(i, OUTPUT);
    swval[i] = (EEPROM.read(i) == 1) ? 1 : 0;
    t = swval[i];
    mcp.digitalWrite(i, t ? MCP_HIGH : MCP_LOW);
  }
  for (int i = 9; i <= 48; i++) {
    t = EEPROM.read(i);
    Serial.print("EPROAM MEM:- ");
    Serial.println(t);
    swval[i] = ( t == 255) ? 0 : t;
  }

  irrecv.enableIRIn();
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  display.begin(SSD1306_SWITCHCAPVCC);
  
  display.clearDisplay(); display.drawBitmap(0, 0,  ohtLogoBitmap, 128, 64, WHITE, BLACK); display.display();
  delay(2000); display.setTextSize(1); display.setTextColor(WHITE);

  //pinMode(LED_BUILTIN, OUTPUT);
  //thing["led"] << digitalPin(LED_BUILTIN);
  thing["millis"] >> outputValue(millis());
  thing["LED_1"] << [](pson & in) {
    if (in.is_empty()) {
      in = swval[1];
    } else {
      change_led(1, in, 0);
    }
  };
  thing["LED_2"] << [](pson & in) {
    if (in.is_empty()) {
      in = swval[2];
    } else {
      change_led(2, in, 0);
    }
  };
  thing["LED_3"] << [](pson & in) {
    if (in.is_empty()) {
      in = swval[3];
    } else {
      change_led(3, in, 0);
    }
  };
  thing["LED_4"] << [](pson & in) {
    if (in.is_empty()) {
      in = swval[4];
    } else {
      change_led(4, in, 0);
    }
  };
  thing["LED_5"] << [](pson & in) {
    if (in.is_empty()) {
      in = swval[5];
    } else {
      change_led(5, in, 0);
    }
  };
  thing["LED_6"] << [](pson & in) {
    if (in.is_empty()) {
      in = swval[6];
    } else {
      change_led(6, in, 0);
    }
  };
  thing["LED_7"] << [](pson & in) {
    if (in.is_empty()) {
      in = swval[7];
    } else {
      change_led(7, in, 0);
    }
  };
  thing["LED_8"] << [](pson & in) {
    if (in.is_empty()) {
      in = swval[8];
    } else {
      change_led(8, in, 0);
    }
  };
  thing["power"] << [](pson & in) {
    if (in.is_empty()) {
      in = device_status;
    } else {
      device_status = in;
      change_power();
    }
  };

  thing["TEMPRATURE"] >> [](pson & out) {
    out = get_temprature();
  };
  thing["M_LED_1"] >> [](pson & out) {
    out = swval[1];
  };
  thing["M_LED_2"] >> [](pson & out) {
    out = swval[2];
  };
  thing["M_LED_3"] >> [](pson & out) {
    out = swval[3];
  };
  thing["M_LED_4"] >> [](pson & out) {
    out = swval[4];
  };
  thing["M_LED_5"] >> [](pson & out) {
    out = swval[5];
  };
  thing["M_LED_6"] >> [](pson & out) {
    out = swval[6];
  };
  thing["M_LED_7"] >> [](pson & out) {
    out = swval[7];
  };
  thing["M_LED_8"] >> [](pson & out) {
    out = swval[8];
  };
  thing["M_power"] >> [](pson & out) {
    out = device_status;
  };
  thing["water"] >> [](pson & out) {
    out = get_watertank();
  };
  thing["SCHEDULE_1"] << [](pson & in) {
    if (in.is_empty()) {
      in["1"] = swval[17];
      in["2"] = swval[18];
      in["3"] = swval[19];
      in["4"] = swval[20];
    } else {
      swval[17] = in["1"];
      swval[18] = in["2"];
      swval[19] = in["3"];
      swval[20] = in["4"];
      for (i = 17; i <= 20; i++) {
        t = swval[i];
        EEPROM.write(i, t);
      }
      EEPROM.commit();
    }
  };
  thing["SCHEDULE_2"] << [](pson & in) {
    if (in.is_empty()) {
      in["1"] = swval[21];
      in["2"] = swval[22];
      in["3"] = swval[23];
      in["4"] = swval[24];
    } else {
      swval[21] = in["1"];
      swval[22] = in["2"];
      swval[23] = in["3"];
      swval[24] = in["4"];
      for (i = 21; i <= 24; i++) {
        t = swval[i];
        EEPROM.write(i, t);
      }
      EEPROM.commit();
    }
  };
  thing["SCHEDULE_3"] << [](pson & in) {
    if (in.is_empty()) {
      in["1"] = swval[25];
      in["2"] = swval[26];
      in["3"] = swval[27];
      in["4"] = swval[28];
    } else {
      swval[25] = in["1"];
      swval[26] = in["2"];
      swval[27] = in["3"];
      swval[28] = in["4"];
      for (i = 25; i <= 28; i++) {
        t = swval[i];
        EEPROM.write(i, t);
      }
      EEPROM.commit();
    }
  };
  thing["SCHEDULE_4"] << [](pson & in) {
    if (in.is_empty()) {
      in["1"] = swval[29];
      in["2"] = swval[30];
      in["3"] = swval[31];
      in["4"] = swval[32];
    } else {
      swval[29] = in["1"];
      swval[30] = in["2"];
      swval[31] = in["3"];
      swval[32] = in["4"];
      for (i = 29; i <= 32; i++) {
        t = swval[i];
        EEPROM.write(i, t);
      }
      EEPROM.commit();
    }
  };
  thing["SCHEDULE_5"] << [](pson & in) {
    if (in.is_empty()) {
      in["1"] = swval[33];
      in["2"] = swval[34];
      in["3"] = swval[35];
      in["4"] = swval[36];
    } else {
      swval[33] = in["1"];
      swval[34] = in["2"];
      swval[35] = in["3"];
      swval[36] = in["4"];
      for (i = 33; i <= 36; i++) {
        t = swval[i];
        EEPROM.write(i, t);
      }
      EEPROM.commit();
    }
  };
  thing["SCHEDULE_6"] << [](pson & in) {
    if (in.is_empty()) {
      in["1"] = swval[37];
      in["2"] = swval[38];
      in["3"] = swval[39];
      in["4"] = swval[40];
    } else {
      swval[37] = in["1"];
      swval[38] = in["2"];
      swval[39] = in["3"];
      swval[40] = in["4"];
      for (i = 37; i <= 40; i++) {
        t = swval[i];
        EEPROM.write(i, t);
      }
      EEPROM.commit();
    }
  };
  thing["SCHEDULE_7"] << [](pson & in) {
    if (in.is_empty()) {
      in["1"] = swval[41];
      in["2"] = swval[42];
      in["3"] = swval[43];
      in["4"] = swval[44];
    } else {
      swval[41] = in["1"];
      swval[42] = in["2"];
      swval[43] = in["3"];
      swval[44] = in["4"];
      for (i = 41; i <= 44; i++) {
        t = swval[i];
        EEPROM.write(i, t);
      }
      EEPROM.commit();
    }
  };
  thing["SCHEDULE_8"] << [](pson & in) {
    if (in.is_empty()) {
      in["1"] = swval[45];
      in["2"] = swval[46];
      in["3"] = swval[47];
      in["4"] = swval[48];
    } else {
      swval[45] = in["1"];
      swval[46] = in["2"];
      swval[47] = in["3"];
      swval[48] = in["4"];
      for (i = 45; i <= 48; i++) {
        t = swval[i];
        EEPROM.write(i, t);
      }
      EEPROM.commit();
    }
  };
}
void loop() {
  unsigned long currentMillis = millis();
  if ((time_status == 0 || timeClient.getEpochTime() < 100000) && WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    if (timeClient.update() == 1) {
      Serial.println("NTP SERVER TIME UPDATED ...");
      Serial.println(timeClient_getDayName());
      Serial.println(timeClient_getClockTime());
      if (timeClient.getEpochTime() > 100000) {
        time_status = 1;
      }
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (wifi_state == 0) {
      wifi_state = 1;
      dis_freej = millis();
      hc_print("WI_FI_STATE");
    }
    thing.handle();
  } else {
    if (wifi_state == 1) {
      wifi_state = 0;
      dis_freej = millis();
      hc_print("WI_FI_STATE");
    }
  }

  if (irrecv.decode(&results)) {
    unsigned int ircode = IR_VALUES(results.value);
    Serial.print("IR SENSOR VALUE - ");
    Serial.println(ircode);
    if (ircode == 10) {
      // device_status = device_status ? 0 : 1;
      device_status=1;
      change_power();
    }
    else if(ircode == 11){
      device_status=0;
      change_power();
    }
    else if (ircode == 12 && device_status == 1) {
      hc_print("AP_MODE_STEP-1");
      Serial.println("detetct ap mode 1");
      for (i = 1; i <= 10; i++)
      {
        irrecv.resume();
        delay(100);
        if (irrecv.decode(&results)) {
          ircode = IR_VALUES(results.value);
          Serial.println(ircode);
          if (ircode == 12) {
            i = 11;
            hc_print("AP_MODE");
            WiFiManager wm;
            wm.setConfigPortalTimeout(AP_timeout);
            if (!wm.startConfigPortal("Om Hari Bhawan - IOT", "9876543210")) {
              Serial.println("failed to connect and hit timeout");
            }
          }
        }
      }
    }
    else if (ircode == 13) {
      dis_freej = millis();
      hc_print("WI_FI_STATE");
    }
    else if (ircode == 14) {
      dis_freej = millis();
      hc_print("TEMPRATURE");
    }
    else if (ircode == 0) {
      dis_freej = millis();
      hc_print("WATER");
    }
    else if (ircode >= 1 && ircode <= 8) {
      change_led(ircode, 2, 0);
    }
    irrecv.resume();  // Receive the next value
  }

  if (time_status == 1 && currentMillis - schedule_check >= 5000) {
    Serial.print("Schedule Searching _ Current Minuts:- ");
    int a = 0, b = 0, c = timeClient_getDayMinutes(), sc_time_1 = 0, sc_time_2 = 0, sc_time_3 = 0, sc_time_4 = 0;
    Serial.println(c);
    for (i = 17; i <= 48; i += 4) {
      a = ((i - 16) / 4) + 1; // SWITCH NO.
      b = a + 8;   // Is Schedulling On

      sc_time_1 = swval[i];
      sc_time_2 = swval[i + 1];
      sc_time_3 = swval[i + 2];
      sc_time_4 = swval[i + 3];

      if ((sc_time_1 <= c && c < sc_time_2) || (sc_time_2 < sc_time_1 && ((sc_time_1 >= c && sc_time_2 > c) || (sc_time_1 <= c && sc_time_2 <= c)))) {
        Serial.println("Schedule Found");
        if (swval[b] == 1) {
          continue;
        }
        swval[b] = 1;
        change_led(a, 1, 2);
        EEPROM.write(b, 1); EEPROM.commit();
      }
      else if ((sc_time_3 <= c && c < sc_time_4) || (sc_time_4 < sc_time_3 && ((sc_time_3 >= c && sc_time_4 > c) || (sc_time_3 <= c && sc_time_4 <= c)))) {
        Serial.println("Schedule Found");
        if (swval[b] == 1) {
          continue;
        }
        swval[b] = 1;
        change_led(a, 1, 2);
        EEPROM.write(b, 1); EEPROM.commit();
      }
      else if (swval[b] == 1) {
        Serial.println("Schedule Off");
        swval[b] = 0;
        change_led(a, 0, 2);
        EEPROM.write(b, 0); EEPROM.commit();
      }
    }
    schedule_check = millis();
  }
  if (wifi_state && currentMillis - water_check >= 10000) {
    thing.stream(thing["water"]);
    water_check = millis();
  }
  if (wifi_state && currentMillis - temprature_check >= 10000) {
    thing.stream(thing["TEMPRATURE"]);
    temprature_check = millis();
  }

  hc_print("home");
}

void hc_print(String t)
{
  if (device_status == 0) {
    //display.clearDisplay();
    //display.display();
    return;
  }
  if (millis() - dis_freej <= 3000 && t == "home") {
    return;
  }

  display.clearDisplay();
  if (t == "home") {
    if (time_status == 1) {
      display.setTextSize(4); display.setCursor(5, 5); display.println(timeClient_getClockTime()); // Show Time
    }
    else {
      display.setTextSize(3); display.setCursor(0, 5); display.println("Offline\n"); // Show Time
    }
    dis_freej = millis()-1000;
  }
  else if (t == "AP_MODE_STEP-1") {
    display.setTextSize(2); display.setCursor(25, 5); display.println("AP MODE\n");
    display.setTextSize(1); display.setCursor(0, 30); display.println("Press Mode Key Again");
  }
  else if (t == "AP_MODE") {
    display.setTextSize(2); display.setCursor(25, 5); display.println("AP MODE\n");
    display.setTextSize(1); display.setCursor(22, 30); display.println("IP: 192.168.4.1");
  }
  else if (t == "WI_FI_STATE") {
    if ((WiFi.status() == WL_CONNECTED)) {
      display.setTextSize(2); display.setCursor(10, 5); display.print(" WiFi OK");
    }
    else {
      display.setTextSize(2); display.setCursor(5, 5); display.print("WiFi ERROR");
    }
  }
  else if (t == "WATER") {
    display.setTextSize(2); display.setCursor(2, 2); display.println("WATER TANK\n");
    display.setTextSize(2); display.print(get_watertank());display.print("% FILL");
    //display.drawRect(14, 22, 100, 20); // (x,y,w,h)
    //display.fillRect(14, 22, 100, 20); // (x,y,w,h)
  }
  else if (t == "TEMPRATURE") {
    display.setTextSize(2); display.setCursor(2, 2); display.println("TEMPRATURE\n");
    display.setTextSize(1);
    double cels = get_temprature();
    double farh = (cels * 9 / 5) + 32;
    display.print(cels);
    display.print(" ");
    display.print((char)8451);
    display.print("C");
    display.print(" | ");
    display.print(farh);
    display.print(" ");
    display.print((char)8451);
    display.print("F");
    //display.drawRect(14, 22, 100, 20); // (x,y,w,h)
    //display.fillRect(14, 22, 100, 20); // (x,y,w,h)
  }
  display.setTextSize(1); display.setCursor(0, 45); display.println("\n   Chauhan's Family");        // Tag Line
  display.display();
}
int get_watertank()
{
  digitalWrite(trigPin, LOW);   // Makes trigPin low
  delayMicroseconds(2);       // 2 micro second delay

  digitalWrite(trigPin, HIGH);  // tigPin high
  delayMicroseconds(10);      // trigPin high for 10 micro seconds
  digitalWrite(trigPin, LOW);   // trigPin low

  duration = pulseIn(echoPin, HIGH);   //Read echo pin, time in microseconds
  distance = duration * 0.034 / 2;   //Calculating actual/real distance

  int per = distance - 30;
  per = (per > 100) ? 100 : per;
  per = (per < 1) ? 0 : per;
  per = 100 - per;

  int test = (per / 10);
  if (wifi_state && test != water_level) {
    Serial.print("TEST - ");
    Serial.print(test);
    Serial.print("SAVED - ");
    Serial.print(water_level);
    Serial.print("PER - ");
    Serial.println(per);
    pson data;
    data["per"] = per;
    data["level"] = water_level;
    thing.call_endpoint("WATER_STATUS_API", data);
    water_level = test;
    Serial.println("WATER ENDPOINT CALL");
  }

  return per;
}
double get_temprature()
{
  return (analogRead(A0) * 3.3 * 100 / 1023.0);
}
void hc_print2(String A, String S, int B) {
  if (device_status == 0) {
    display.clearDisplay();
    display.display();
    return;
  }

  display.clearDisplay();
  display.setTextSize(2); display.setCursor(0, 5); display.println(A);
  display.setTextSize(2); display.setCursor(10, 25); display.print(S + " "); display.println(B);

  display.setTextSize(1); display.setCursor(0, 45); display.println("\n  Chauhan's Family");        // Tag Line
  display.display();
}
void change_led(int led, int status, int type) {
  if (device_status == 0) {
    return;
  }
  if (status != 2 && status != 1 && status != 0) {
    return;
  }
  if (status == 2) {
    Serial.print("Led Status Update By Trigger - ");
    status = swval[led] ? 0 : 1;
  }
  else {
    Serial.print("Led Status Update Online - ");
  }
  dis_freej = millis();
  if (type == 2) {
    if (status) {
      Serial.print("SWITCH "); Serial.print(led); Serial.println(", SCHEDEULE OFF");
      hc_print2("SCHEDULING", "LED ON ", led);
    }
    else {
      Serial.print("SWITCH "); Serial.print(led); Serial.println(", SCHEDEULE ON");
      hc_print2("SCHEDULING", "LED OFF", led);
    }
  }
  else {
    if (swval[led + 8] == 1) {
      if (status == 0) {
        Serial.print("SWITCH "); Serial.print(led); Serial.println(", MANUALLY OFF");
        hc_print2("  MANUALLY", "LED OFF", led);
      }
      else {
        Serial.print("SWITCH ");
        Serial.print(led);
        Serial.println(", MANUALLY ON");
        hc_print2("  MANUALLY", "LED ON ", led);
      }
    }
    else {
      if (status == 0) {
        hc_print2("SWITCH OFF", "Change", led);
      }
      else {
        hc_print2(" SWITCH ON", "Change", led);
      }
    }
  }
  mcp.digitalWrite(led, status ? MCP_HIGH : MCP_LOW);
  swval[led] = status;
  
  if(led==1)     {thing.stream(thing["M_LED_1"]);}
  else if(led==2){thing.stream(thing["M_LED_2"]);}
  else if(led==3){thing.stream(thing["M_LED_3"]);}
  else if(led==4){thing.stream(thing["M_LED_4"]);}
  else if(led==5){thing.stream(thing["M_LED_5"]);}
  else if(led==6){thing.stream(thing["M_LED_6"]);}
  else if(led==7){thing.stream(thing["M_LED_7"]);}
  else if(led==8){thing.stream(thing["M_LED_8"]);}
  EEPROM.write(led, status);
  EEPROM.commit();
}
void change_power() {
	thing.stream(thing["M_power"]);
  for (i = 1; i <= 8; i++) {
    if (device_status == 0) {
	  display.clearDisplay();
	  display.display();
	  mcp.digitalWrite(i, MCP_LOW);
    }
    else {
      t = swval[i];
      mcp.digitalWrite(i, t ? MCP_HIGH : MCP_LOW);
    }
  }
}
int IR_VALUES(unsigned long long int v)
{
  int r = 50;
  switch (v)
  {
    case 16236607:
      r = 10; //ON
      break;

    case 16203967:
      r = 11; // OFF
      break;

    case 16195807:
      r = 12; // R - AP MODE
      break;

    case 16228447:
      r = 13; // G - WIFI STATE
      break;

    case 16212127:
      r = 14; // W - TEMPRATURE
      break;
      
    case 156:
      r = 15; // POWER - SIGLE POWER BUTTON
      break;

    case 16244767:
      r = 0;
      break;

    case 16191727:
      r = 1;
      break;

    case 16224367:
      r = 2;
      break;

    case 16208047:
      r = 3;
      break;

    case 16199887:
      r = 4;
      break;

    case 16232527:
      r = 5;
      break;

    case 16216207:
      r = 6;
      break;

    case 16189687:
      r = 7;
      break;

    case 16222327:
      r = 8;
      break;

    case 16206007:
      r = 9;
      break;
  }
  return r;
}
