#include <map>

// static const uint8_t D0 = 16, D1 = 5, D2 = 4, D3 = 0, D4 = 2, D5 = 14, D6 = 12,D7 = 13, D8 = 15;
static const uint8_t D00 = 22;
int           AP_TIMEOUT = 40;

const uint16_t  kRecvPin = D6; IRrecv irrecv(kRecvPin); decode_results results; // IR

#define OLED_SDIN D7 //MOSI (Connect to display D1 or SDA)
#define OLED_SCLK D5 //SCLK (Connect to display D0 or SCL)
#define OLED_DC   D4 //Connect to display DC
#define OLED_RST  D3 //Connect to display RES
#define OLED_CS   D00 //Not connected to display, use a spare pin (e.g. D0 is NodeMCU LED)

#define MCP_HIGH 1 // PIN VALUE FOR TURNING ON LED
#define MCP_LOW  0 // PIN VALUE FOR TURNING OFF LED

const int TRIG_PIN = D0; // FOr Distance Sensor
const int ECHO_PIN = D8; // FOr Distance Sensor

int EEPROM_SWVAL[] = {
                        0,                      // FAKE EEPROM VALUE (NOT USED)
                        0, 0, 0, 0, 0, 0, 0, 0, // LED STATE (1-8)
                        0, 0, 0, 0, 0, 0, 0, 0, // LED STATE (9-16) - For future use
                        0, 0, 0, 0, // Schedule 1
                        0, 0, 0, 0, // Schedule 2
                        0, 0, 0, 0, // Schedule 3
                        0, 0, 0, 0, // Schedule 4
                        0, 0, 0, 0, // Schedule 5
                        0, 0, 0, 0, // Schedule 6
                        0, 0, 0, 0, // Schedule 7
                        0, 0, 0, 0, // Schedule 8
                        0, 0, 0, 0, 0, 0, 0, 0 // Schedule State (1-8)
                };

std::map<unsigned long long int, int> IR_VALUE_MAP_1 = {
    {16236607, 10}, // ON 
    {16203967, 11}, // OFF
    {16195807, 12}, // R - AP MODE
    {16228447, 13}, // G - WIFI STATE
    {16212127, 14}, // W - TEMPRATURE
    {156, 15},      // POWER - SIGLE POWER BUTTON
    {16244767, 0},  // WATER LEVEL - 0
    {16191727, 1}, 
    {16224367, 2}, 
    {16208047, 3}, 
    {16199887, 4}, 
    {16232527, 5}, 
    {16216207, 6}, 
    {16189687, 7}, 
    {16222327, 8}, 
    {16206007, 9}
};

int IR_VALUES(unsigned long long int v) {
    // check if key exists in map
    if (IR_VALUE_MAP_1.find(v) != IR_VALUE_MAP_1.end()) {
        return IR_VALUE_MAP_1[v];
    }
    return -1;
}

bool SCHEDULE_CHECK(int START_1, int END_1, int START_2, int END_2, int CURRENT_TIME){
    int c = CURRENT_TIME;
    bool SCH_1 = (
        START_1 <= c && c < END_1) // if CURRENT_TIME is between START and END
        || 
        (END_1 < START_1 && ((START_1 >= c && END_1 > c) || (START_1 <= c && END_1 <= c)) // if START is greater than END and CURRENT_TIME is between START and 24:00 or between 00:00 and END
    );
    bool SCH_2 = (
        START_2 <= c && c < END_2) // if CURRENT_TIME is between START and END
        || 
        (END_2 < START_2 && ((START_2 >= c && END_2 > c) || (START_2 <= c && END_2 <= c)) // if START is greater than END and CURRENT_TIME is between START and 24:00 or between 00:00 and END
    );
    return SCH_1 || SCH_2;
}