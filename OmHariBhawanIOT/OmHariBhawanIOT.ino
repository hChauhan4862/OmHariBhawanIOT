#define _DISABLE_TLS_
#define _DEBUG_

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23X17.h>

#include <EEPROM.h>

#include <WiFiManager.h>
#include <ThingerESP8266.h>

#include <Arduino.h>
#include <IRremoteESP8266.h>
// #include <ThingerConsole.h>
#include <IRrecv.h>
#include <IRutils.h>

#include "splash_logo.h"
#include "arduino_secrets.h"
#include "config.h"
#include "timer.h"

Adafruit_SSD1306 display(OLED_SDIN, OLED_SCLK, OLED_DC, OLED_RST, OLED_CS);
Adafruit_MCP23X17 mcp;
ThingerESP8266 thing(THINGER_USERNAME, THINGER_DEVICE_ID, THINGER_DEVICE_CREDENTIAL);
// ThingerConsole console(thing);



int t = 0, i = 0;
bool                WIFI_STATE = 0;
long                DISP_FREEZ = 0;   // TIME FOR DISPLAY FREEZE
bool             device_status = 1;   // DISPLAY POWER STATUS
unsigned long   schedule_check = 0;   // TIME FOR LAST SCHEDULE CHECK
unsigned long      water_check = 0;   // TIME FOR LAST WATER LEVEL CHECK
unsigned long temprature_check = 0;   // TIME FOR LAST TEMPRATURE CHECK
int                water_level = 0;   // WATER LEVEL STATUS
long                  duration;       // Echo return duration for Distance Sensor
int                   distance;       //  Current Calculated Distance by Sensor

void handleLED(pson &in, int ledNum) {
  if (in.is_empty()) {
    in = EEPROM_SWVAL[ledNum];
  } else {
    change_led(ledNum, in, 0);
  }
}

void handleOUTPUT(pson & out, int value) {
  out = value;
};

void schedule_operation(pson & in, int startIndex) {
  if (in.is_empty()) {
    in["1"] = EEPROM_SWVAL[startIndex];
    in["2"] = EEPROM_SWVAL[startIndex + 1];
    in["3"] = EEPROM_SWVAL[startIndex + 2];
    in["4"] = EEPROM_SWVAL[startIndex + 3];
  } else {
    EEPROM_SWVAL[startIndex] = in["1"];
    EEPROM_SWVAL[startIndex + 1] = in["2"];
    EEPROM_SWVAL[startIndex + 2] = in["3"];
    EEPROM_SWVAL[startIndex + 3] = in["4"];
    for (i = startIndex; i <= (startIndex+4); i++) {
      t = EEPROM_SWVAL[i];
      EEPROM.write(i, t);
    }
    EEPROM.commit();
  }
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT); // Sets the TRIG_PIN as an Output
  pinMode(ECHO_PIN, INPUT); // Sets the ECHO_PIN as an Input

  Serial.begin(9600); // Start the Serial communication to send messages to the computer
  EEPROM.begin(512);  // Initialize the EEPROM to store the data in it for power loss
  mcp.begin_I2C();    // Initialize the MCP23017

  for (i = 1; i <= 16; i++) {
    // EEPROM VALUE RESTORE FOR RELAY
    mcp.pinMode(i, OUTPUT);
    EEPROM_SWVAL[i] = (EEPROM.read(i) == 1) ? 1 : 0;
    t = EEPROM_SWVAL[i];
    mcp.digitalWrite(i, t ? MCP_HIGH : MCP_LOW);

    if (i == 8) {break;} // skip last 8 pins :: will use in future
  }

  for (int i = 17; i <= 46; i++) {
    // EEPROM VALUE RESTORE FOR SCHEDULE
    t = EEPROM.read(i);
    Serial.print("EPROAM MEM:- ");
    Serial.println(t);
    EEPROM_SWVAL[i] = ( t == 255) ? 0 : t;
  }

  irrecv.enableIRIn(); // Start the IR receiver
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around

  display.begin(SSD1306_SWITCHCAPVCC); // initialize with the I2C addr 0x3D (for the 128x64) Display
  
  display.clearDisplay(); display.drawBitmap(0, 0,  ohtLogoBitmap, 128, 64, WHITE, BLACK); display.display(); // Show initial display buffer contents on the screen
  delay(2000); display.setTextSize(1); display.setTextColor(WHITE); // wait and clear the display after a delay

  //pinMode(LED_BUILTIN, OUTPUT);
  //thing["led"] << digitalPin(LED_BUILTIN);
  thing["millis"] >> outputValue(millis()); // Send the millis() value to the thinger.io server

  thing["power"] << [](pson & in) {
    if (in.is_empty()) {
      in = device_status;
    } else {
      device_status = in;
      change_power();
    }
  };

  thing["LED_1"] << std::bind(&handleLED, std::placeholders::_1, 1);
  thing["LED_2"] << std::bind(&handleLED, std::placeholders::_1, 2);
  thing["LED_3"] << std::bind(&handleLED, std::placeholders::_1, 3);
  thing["LED_4"] << std::bind(&handleLED, std::placeholders::_1, 4);
  thing["LED_5"] << std::bind(&handleLED, std::placeholders::_1, 5);
  thing["LED_6"] << std::bind(&handleLED, std::placeholders::_1, 6);
  thing["LED_7"] << std::bind(&handleLED, std::placeholders::_1, 7);
  thing["LED_8"] << std::bind(&handleLED, std::placeholders::_1, 8);

  // FUTURE USE
  // thing["LED_9"] << std::bind(&handleLED, std::placeholders::_1, 9);
  // thing["LED_10"] << std::bind(&handleLED, std::placeholders::_1, 10);
  // thing["LED_11"] << std::bind(&handleLED, std::placeholders::_1, 11);
  // thing["LED_12"] << std::bind(&handleLED, std::placeholders::_1, 12);
  // thing["LED_13"] << std::bind(&handleLED, std::placeholders::_1, 13);
  // thing["LED_14"] << std::bind(&handleLED, std::placeholders::_1, 14);
  // thing["LED_15"] << std::bind(&handleLED, std::placeholders::_1, 15);
  // thing["LED_16"] << std::bind(&handleLED, std::placeholders::_1, 16);

  thing["TEMPRATURE"] >> std::bind(&handleOUTPUT, std::placeholders::_1, get_temprature());
  thing["M_power"]    >> std::bind(&handleOUTPUT, std::placeholders::_1, device_status);
  thing["water"]      >> std::bind(&handleOUTPUT, std::placeholders::_1, get_watertank());
  thing["M_LED_1"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[1]);
  thing["M_LED_2"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[2]);
  thing["M_LED_3"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[3]);
  thing["M_LED_4"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[4]);
  thing["M_LED_5"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[5]);
  thing["M_LED_6"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[6]);
  thing["M_LED_7"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[7]);
  thing["M_LED_8"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[8]);

  // FUTURE USE
  // thing["M_LED_9"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[9]);
  // thing["M_LED_10"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[10]);
  // thing["M_LED_11"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[11]);
  // thing["M_LED_12"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[12]);
  // thing["M_LED_13"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[13]);
  // thing["M_LED_14"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[14]);
  // thing["M_LED_15"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[15]);
  // thing["M_LED_16"] >> std::bind(&handleOUTPUT, std::placeholders::_1, EEPROM_SWVAL[16]);

  thing["SCHEDULE_1"] << std::bind(schedule_operation, std::placeholders::_1, 17);
  thing["SCHEDULE_2"] << std::bind(schedule_operation, std::placeholders::_1, 21);
  thing["SCHEDULE_3"] << std::bind(schedule_operation, std::placeholders::_1, 25);
  thing["SCHEDULE_4"] << std::bind(schedule_operation, std::placeholders::_1, 29);
  thing["SCHEDULE_5"] << std::bind(schedule_operation, std::placeholders::_1, 33);
  thing["SCHEDULE_6"] << std::bind(schedule_operation, std::placeholders::_1, 37);
  

  // thing["LED_1"] << [](pson & in) {     // LED_1 is the name of the resource 
  //   if (in.is_empty()) {
  //     in = EEPROM_SWVAL[1]; // if the resource is empty, then send the value of the LED
  //   } else {
  //     change_led(1, in, 0); // if the resource is not empty, then change the LED state
  //   }
  // };
  // thing["LED_2"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in = EEPROM_SWVAL[2];
  //   } else {
  //     change_led(2, in, 0);
  //   }
  // };
  // thing["LED_3"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in = EEPROM_SWVAL[3];
  //   } else {
  //     change_led(3, in, 0);
  //   }
  // };
  // thing["LED_4"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in = EEPROM_SWVAL[4];
  //   } else {
  //     change_led(4, in, 0);
  //   }
  // };
  // thing["LED_5"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in = EEPROM_SWVAL[5];
  //   } else {
  //     change_led(5, in, 0);
  //   }
  // };
  // thing["LED_6"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in = EEPROM_SWVAL[6];
  //   } else {
  //     change_led(6, in, 0);
  //   }
  // };
  // thing["LED_7"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in = EEPROM_SWVAL[7];
  //   } else {
  //     change_led(7, in, 0);
  //   }
  // };
  // thing["LED_8"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in = EEPROM_SWVAL[8];
  //   } else {
  //     change_led(8, in, 0);
  //   }
  // };

  // thing["TEMPRATURE"] >> [](pson & out) {
  //   out = get_temprature();
  // };
  // thing["M_LED_1"] >> [](pson & out) {
  //   out = EEPROM_SWVAL[1];
  // };
  // thing["M_LED_2"] >> [](pson & out) {
  //   out = EEPROM_SWVAL[2];
  // };
  // thing["M_LED_3"] >> [](pson & out) {
  //   out = EEPROM_SWVAL[3];
  // };
  // thing["M_LED_4"] >> [](pson & out) {
  //   out = EEPROM_SWVAL[4];
  // };
  // thing["M_LED_5"] >> [](pson & out) {
  //   out = EEPROM_SWVAL[5];
  // };
  // thing["M_LED_6"] >> [](pson & out) {
  //   out = EEPROM_SWVAL[6];
  // };
  // thing["M_LED_7"] >> [](pson & out) {
  //   out = EEPROM_SWVAL[7];
  // };
  // thing["M_LED_8"] >> [](pson & out) {
  //   out = EEPROM_SWVAL[8];
  // };
  // thing["M_power"] >> [](pson & out) {
  //   out = device_status;
  // };
  // thing["water"] >> [](pson & out) {
  //   out = get_watertank();
  // };

  // thing["SCHEDULE_1"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in["1"] = EEPROM_SWVAL[17];
  //     in["2"] = EEPROM_SWVAL[18];
  //     in["3"] = EEPROM_SWVAL[19];
  //     in["4"] = EEPROM_SWVAL[20];
  //   } else {
  //     EEPROM_SWVAL[17] = in["1"];
  //     EEPROM_SWVAL[18] = in["2"];
  //     EEPROM_SWVAL[19] = in["3"];
  //     EEPROM_SWVAL[20] = in["4"];
  //     for (i = 17; i <= 20; i++) {
  //       t = EEPROM_SWVAL[i];
  //       EEPROM.write(i, t);
  //     }
  //     EEPROM.commit();
  //   }
  // };
  // thing["SCHEDULE_2"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in["1"] = EEPROM_SWVAL[21];
  //     in["2"] = EEPROM_SWVAL[22];
  //     in["3"] = EEPROM_SWVAL[23];
  //     in["4"] = EEPROM_SWVAL[24];
  //   } else {
  //     EEPROM_SWVAL[21] = in["1"];
  //     EEPROM_SWVAL[22] = in["2"];
  //     EEPROM_SWVAL[23] = in["3"];
  //     EEPROM_SWVAL[24] = in["4"];
  //     for (i = 21; i <= 24; i++) {
  //       t = EEPROM_SWVAL[i];
  //       EEPROM.write(i, t);
  //     }
  //     EEPROM.commit();
  //   }
  // };
  // thing["SCHEDULE_3"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in["1"] = EEPROM_SWVAL[25];
  //     in["2"] = EEPROM_SWVAL[26];
  //     in["3"] = EEPROM_SWVAL[27];
  //     in["4"] = EEPROM_SWVAL[28];
  //   } else {
  //     EEPROM_SWVAL[25] = in["1"];
  //     EEPROM_SWVAL[26] = in["2"];
  //     EEPROM_SWVAL[27] = in["3"];
  //     EEPROM_SWVAL[28] = in["4"];
  //     for (i = 25; i <= 28; i++) {
  //       t = EEPROM_SWVAL[i];
  //       EEPROM.write(i, t);
  //     }
  //     EEPROM.commit();
  //   }
  // };
  // thing["SCHEDULE_4"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in["1"] = EEPROM_SWVAL[29];
  //     in["2"] = EEPROM_SWVAL[30];
  //     in["3"] = EEPROM_SWVAL[31];
  //     in["4"] = EEPROM_SWVAL[32];
  //   } else {
  //     EEPROM_SWVAL[29] = in["1"];
  //     EEPROM_SWVAL[30] = in["2"];
  //     EEPROM_SWVAL[31] = in["3"];
  //     EEPROM_SWVAL[32] = in["4"];
  //     for (i = 29; i <= 32; i++) {
  //       t = EEPROM_SWVAL[i];
  //       EEPROM.write(i, t);
  //     }
  //     EEPROM.commit();
  //   }
  // };
  // thing["SCHEDULE_5"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in["1"] = EEPROM_SWVAL[33];
  //     in["2"] = EEPROM_SWVAL[34];
  //     in["3"] = EEPROM_SWVAL[35];
  //     in["4"] = EEPROM_SWVAL[36];
  //   } else {
  //     EEPROM_SWVAL[33] = in["1"];
  //     EEPROM_SWVAL[34] = in["2"];
  //     EEPROM_SWVAL[35] = in["3"];
  //     EEPROM_SWVAL[36] = in["4"];
  //     for (i = 33; i <= 36; i++) {
  //       t = EEPROM_SWVAL[i];
  //       EEPROM.write(i, t);
  //     }
  //     EEPROM.commit();
  //   }
  // };
  // thing["SCHEDULE_6"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in["1"] = EEPROM_SWVAL[37];
  //     in["2"] = EEPROM_SWVAL[38];
  //     in["3"] = EEPROM_SWVAL[39];
  //     in["4"] = EEPROM_SWVAL[40];
  //   } else {
  //     EEPROM_SWVAL[37] = in["1"];
  //     EEPROM_SWVAL[38] = in["2"];
  //     EEPROM_SWVAL[39] = in["3"];
  //     EEPROM_SWVAL[40] = in["4"];
  //     for (i = 37; i <= 40; i++) {
  //       t = EEPROM_SWVAL[i];
  //       EEPROM.write(i, t);
  //     }
  //     EEPROM.commit();
  //   }
  // };
  // thing["SCHEDULE_7"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in["1"] = EEPROM_SWVAL[41];
  //     in["2"] = EEPROM_SWVAL[42];
  //     in["3"] = EEPROM_SWVAL[43];
  //     in["4"] = EEPROM_SWVAL[44];
  //   } else {
  //     EEPROM_SWVAL[41] = in["1"];
  //     EEPROM_SWVAL[42] = in["2"];
  //     EEPROM_SWVAL[43] = in["3"];
  //     EEPROM_SWVAL[44] = in["4"];
  //     for (i = 41; i <= 44; i++) {
  //       t = EEPROM_SWVAL[i];
  //       EEPROM.write(i, t);
  //     }
  //     EEPROM.commit();
  //   }
  // };
  // thing["SCHEDULE_8"] << [](pson & in) {
  //   if (in.is_empty()) {
  //     in["1"] = EEPROM_SWVAL[45];
  //     in["2"] = EEPROM_SWVAL[46];
  //     in["3"] = EEPROM_SWVAL[47];
  //     in["4"] = EEPROM_SWVAL[48];
  //   } else {
  //     EEPROM_SWVAL[45] = in["1"];
  //     EEPROM_SWVAL[46] = in["2"];
  //     EEPROM_SWVAL[47] = in["3"];
  //     EEPROM_SWVAL[48] = in["4"];
  //     for (i = 45; i <= 48; i++) {
  //       t = EEPROM_SWVAL[i];
  //       EEPROM.write(i, t);
  //     }
  //     EEPROM.commit();
  //   }
  // };
}
void loop() {
  unsigned long currentMillis = millis(); // get the current "time" (actually the number of milliseconds since the program started)
  if (!isTimeSet() && WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    bool t= timeClient.update();
    // Below line is for debugging purpose only
    if (t) {
      Serial.println("NTP SERVER TIME UPDATED ...");
      Serial.println(timeClient_getDayName());
      Serial.println(timeClient_getClockTime());
    }
  }

  if ((WiFi.status() == WL_CONNECTED && WIFI_STATE==0) || (WiFi.status() != WL_CONNECTED && WIFI_STATE==1)){
    // detect wifi state change
      WIFI_STATE = !WIFI_STATE;
      DISP_FREEZ = millis();
      hc_print("WI_FI_STATE");
  }

  if (WiFi.status() == WL_CONNECTED) {
    thing.handle(); // this is how IoTaaP handles messages
  }

  if (irrecv.decode(&results)) {
    unsigned int ircode = IR_VALUES(results.value);

    Serial.print("IR SENSOR VALUE - ");
    Serial.println(ircode);

    if (ircode == 10) {
      // device_status = !device_status;
      device_status=true;
      change_power();
    }
    else if(ircode == 11){
      device_status=false;
      change_power();
    }
    else if (ircode == 12 && device_status) {
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
            wm.setConfigPortalTimeout(AP_TIMEOUT);
            if (!wm.startConfigPortal("Om Hari Bhawan - IOT", "9876543210")) {
              Serial.println("failed to connect and hit timeout");
            }
          }
        }
      }
    }
    else if (ircode == 13) {
      DISP_FREEZ = millis();
      hc_print("WI_FI_STATE");
    }
    else if (ircode == 14) {
      DISP_FREEZ = millis();
      hc_print("TEMPRATURE");
    }
    else if (ircode == 0) {
      DISP_FREEZ = millis();
      hc_print("WATER");
    }
    else if (ircode >= 1 && ircode <= 8) {
      change_led(ircode, 2, 0);
    }
    irrecv.resume();  // Receive the next value
  }

  if (isTimeSet() && currentMillis - schedule_check >= 10000) {
    // SCHEDULE CHECKING EVERY 10 SECONDS
    int currentMinutes = timeClient_getDayMinutes();
    int switchNo, scheduleState;

    Serial.print("Schedule Searching _ Current Minuts:- ");
    Serial.println(currentMinutes);

    for (i = 17; i <= 48; i += 4) { // SCHEDULE TIMEs having gap of 4
      switchNo      = ((i - 16) / 4) + 1; // SWITCH NO.
      scheduleState = switchNo + 48;   // Schedulling State

      int START_1 = EEPROM_SWVAL[i];
      int END_1   = EEPROM_SWVAL[i + 1];
      int START_2 = EEPROM_SWVAL[i + 2];
      int END_2   = EEPROM_SWVAL[i + 3];

      if (SCHEDULE_CHECK(START_1, END_1, START_2, END_2, currentMinutes)) {
        if (EEPROM_SWVAL[scheduleState] == 1) {continue;}

        EEPROM_SWVAL[scheduleState] = 1;
        change_led(switchNo, 1, 2);
        EEPROM.write(scheduleState, 1); EEPROM.commit();
      }
      else if (EEPROM_SWVAL[scheduleState] == 1) {
        EEPROM_SWVAL[scheduleState] = 0;
        change_led(switchNo, 0, 2);
        EEPROM.write(scheduleState, 0); EEPROM.commit();
      }
      
    }
    schedule_check = millis();
  }
  if (WIFI_STATE && currentMillis - water_check >= 10000) {
    // WATER LEVEL CHECKING EVERY 10 SECONDS
    thing.stream(thing["water"]);
    water_check = millis();
  }
  if (WIFI_STATE && currentMillis - temprature_check >= 10000) {
    // TEMPRATURE CHECKING EVERY 10 SECONDS
    thing.stream(thing["TEMPRATURE"]);
    temprature_check = millis();
  }

  hc_print("home");
}

void hc_print(String t)
{
  if (!device_status) {
    //display.clearDisplay();
    //display.display();
    return;
  }
  if (millis() - DISP_FREEZ <= 3000 && t == "home") {
    return;
  }

  display.clearDisplay();
  if (t == "home") {
    if (isTimeSet()) {
      display.setTextSize(4); display.setCursor(5, 5); display.println(timeClient_getClockTime()); // Show Time
    }
    else {
      display.setTextSize(3); display.setCursor(0, 5); display.println("Offline\n"); // Show Time
    }
    DISP_FREEZ = millis()-1000;
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
  digitalWrite(TRIG_PIN, LOW);   // Makes TRIG_PIN low
  delayMicroseconds(2);       // 2 micro second delay

  digitalWrite(TRIG_PIN, HIGH);  // tigPin high
  delayMicroseconds(10);      // TRIG_PIN high for 10 micro seconds
  digitalWrite(TRIG_PIN, LOW);   // TRIG_PIN low

  duration = pulseIn(ECHO_PIN, HIGH);   //Read echo pin, time in microseconds
  distance = duration * 0.034 / 2;   //Calculating actual/real distance

  int per = distance - 30;  // 30 cm is the distance of sensor from water tank
  per = (per > 100) ? 100 : per; // if per is greater than 100 then make it 100
  per = (per < 1) ? 0 : per;  // if per is less than 1 then make it 0
  per = 100 - per;  // reverse the percentage because 100% is empty and 0% is full

  int test = (per / 10);
  if (WIFI_STATE && test != water_level) {
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
  if (!device_status) {
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
  if (!device_status || status < 0 || status > 2) {
    return;
  }

  DISP_FREEZ = millis();

  if (status == 2) {
    status = EEPROM_SWVAL[led] ? 0 : 1;
  }

  if (type == 2) {
    hc_print2("SCHEDULING", status ? "LED ON " : "LED OFF", led);
  } else if (EEPROM_SWVAL[led + 16] == 1) {
    hc_print2("  MANUALLY", status ? "LED ON " : "LED OFF", led);
  } else {
    hc_print2(status ? " SWITCH ON" : "SWITCH OFF", "Change", led);
  }

  mcp.digitalWrite(led, status ? MCP_HIGH : MCP_LOW);
  EEPROM_SWVAL[led] = status;

  if (led == 1) { thing.stream(thing["M_LED_1"]); }
  else if (led == 2) { thing.stream(thing["M_LED_2"]); }
  else if (led == 3) { thing.stream(thing["M_LED_3"]); }
  else if (led == 4) { thing.stream(thing["M_LED_4"]); }
  else if (led == 5) { thing.stream(thing["M_LED_5"]); }
  else if (led == 6) { thing.stream(thing["M_LED_6"]); }
  else if (led == 7) { thing.stream(thing["M_LED_7"]); }
  else if (led == 8) { thing.stream(thing["M_LED_8"]); }

  EEPROM.write(led, status);
  EEPROM.commit();
}

void change_power() {
	thing.stream(thing["M_power"]);
  if (!device_status) {
	  display.clearDisplay();
	  display.display();
  }

  for (i = 1; i <= 16; i++) {
    if (!device_status) {
      mcp.digitalWrite(i, MCP_LOW);
    }
    else {
      mcp.digitalWrite(i, EEPROM_SWVAL[i] ? MCP_HIGH : MCP_LOW);
    }
  }
}

// int IR_VALUES(unsigned long long int v)
// {
//   int r = 50;
//   switch (v)
//   {
//     case 16236607:
//       r = 10; //ON
//       break;

//     case 16203967:
//       r = 11; // OFF
//       break;

//     case 16195807:
//       r = 12; // R - AP MODE
//       break;

//     case 16228447:
//       r = 13; // G - WIFI STATE
//       break;

//     case 16212127:
//       r = 14; // W - TEMPRATURE
//       break;
      
//     case 156:
//       r = 15; // POWER - SIGLE POWER BUTTON
//       break;

//     case 16244767:
//       r = 0;
//       break;

//     case 16191727:
//       r = 1;
//       break;

//     case 16224367:
//       r = 2;
//       break;

//     case 16208047:
//       r = 3;
//       break;

//     case 16199887:
//       r = 4;
//       break;

//     case 16232527:
//       r = 5;
//       break;

//     case 16216207:
//       r = 6;
//       break;

//     case 16189687:
//       r = 7;
//       break;

//     case 16222327:
//       r = 8;
//       break;

//     case 16206007:
//       r = 9;
//       break;
//   }
//   return r;
// }
