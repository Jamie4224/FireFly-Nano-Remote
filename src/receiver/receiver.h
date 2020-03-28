
#include <Arduino.h>
#include <Smoothed.h>
#include "CPU.h"
#include "globals.h"
#include "radio.h"
#include "utils.h"
#include "VescUart.h"
#include <Preferences.h>

#ifdef RECEIVER_SCREEN
  #include <Adafruit_GFX.h>
  #include "Adafruit_SSD1306.h"
  // fonts
  #include "fonts/Lato_Regular_7.h"
  #include "fonts/Digital.h"
  #include "fonts/Pico.h"
  #include <Fonts/Org_01.h> // Adafruit
  #include <Fonts/FreeSans9pt7b.h>
  #include <Fonts/FreeSans12pt7b.h>

  // Wifi
#endif

Preferences prefs;

// Data structures
ReceiverPacket recvPacket;
RemotePacket remPacket;
TelemetryPacket telemetry;
ConfigPacket boardConfig;
InfoPacket boardInfo;

AppState state = IDLE;

// get MAC address / CPU serial
uint32_t boardID;

// send configuration on power on
bool sendConfig = true;

// send only updated telemetry
bool telemetryUpdated = false;
unsigned long telemetryTime = 0;
bool uartTelemetryAvailable = true;

// Last time data was pulled from VESC
unsigned long lastUartPull = 0;

// Variables to hold values for speed and distance calculation
float gearRatio;
float ratioRpmSpeed;
float ratioPulseDistance;

uint32_t timeoutTimer = 0;
bool receivedData = false;
const short timeoutMax = 500;

// Cruise control
uint16_t cruiseThrottle;
uint16_t cruiseRPM;
bool cruising;
unsigned long lastCruiseControl; //
unsigned long cruiseControlStart;

// Endless ride
unsigned long timeSpeedReached;

const float AUTO_BRAKE_INTERVAL = 0.1;  // increase the brake force every 0.1s

bool connected = false;
uint8_t throttle;
uint8_t lastThrottle;

String wifiStatus;
String updateStatus;

unsigned long lastBrakeTime;

enum vesc_profiles { PROFILE_1, PROFILE_2, PROFILE_3, PROFILE_4, PROFILE_5 };


struct VescProfile {
    uint8_t maxSpeed;
    float motor_current_max;
    float motor_current_brake;
    float battery_current_max;
    float battery_current_max_regen;
    float battery_voltage_cutoff_start;
    float battery_voltage_cutoff_end;
    float abs_max_erpm;
    float abs_max_erpm_reverse;
};

VescProfile profile_1;
VescProfile profile_2;
VescProfile profile_3;
VescProfile profile_4;
VescProfile profile_5;

VescProfile vesc_profile[5] {profile_1, profile_2, profile_3, profile_4, profile_5};

struct ReceiverData {
  uint8_t lastProfile = 0;
} receiverData;

enum VESC_MODES { UART_ONLY, UART_ADC};

struct ReceiverSettings {
  uint8_t maxRange;
  uint8_t batteryCells;
  uint8_t batteryType;
  uint8_t motorPoles;
  uint8_t wheelDiameter;
  uint8_t wheelPulley;
  uint8_t motorPulley;
  uint8_t vescCount;
  uint8_t vescMode; // 0: UART Only, 1: ADC + UART

  uint8_t estopMax;
  float estopTime;
  uint8_t estopRelease;
  float estopInterval;
} receiverSettings;



#ifdef RECEIVER_SCREEN
const GFXfont* fontDigital = &Segment13pt7b;  // speed, distance, ...
// const GFXfont* fontPico = &Segment6pt7b;      //
const GFXfont* fontDesc = &Dialog_plain_9;    // km/h
const GFXfont* fontMicro = &Org_01;         // connection screen

const GFXfont* fontBig = &FreeSans12pt7b;         // connection screen
const GFXfont* font = &FreeSans9pt7b;         // connection screen

void updateScreen();
void drawBattery();
#endif

bool prepareUpdate();
void acquireSetting();
void calculateRatios();
void controlStatusLed();
void coreTask(void * pvParameters );
bool dataAvailable();
void stateMachine();
void getUartData();
bool inRange(int val, int minimum, int maximum);
void radioExchange();
bool receiveData();
bool sendData(uint8_t response);
void sendUartData();
void loadData();
void loadSettings();
void loadProfile(uint8_t profile, bool openprefs);
void setBoardConfig();
bool pushVescProfile(uint8_t profile);
void setState(AppState newState);
void setStatus(uint8_t code);
void setThrottle(uint16_t value);
void setCruise(uint8_t speed);
void speedControl(uint16_t throttle , bool trigger );
String uint64ToAddress(uint64_t number);
String uint64ToString(uint64_t number);
void shutdownBoard();



void updateSetting(uint8_t setting, uint64_t value);

void debug(String x);



void SerializeInt32(char (&buf)[4], int32_t val);

int32_t ParseInt32(const char (&buf)[4]);

