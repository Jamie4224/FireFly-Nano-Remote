#ifndef _CONST_H
#define _CONST_H

#include <Arduino.h>
#include <datatypes.h>

//#define FAKE_UART // Comment out after pairing the remote and connecting VESC

#define DEBUG // Uncomment DEBUG if you need to debug the remote

const COMM_PACKET_ID VESC_COMMAND = COMM_GET_VALUES; // VESC
// const COMM_PACKET_ID VESC_COMMAND = COMM_GET_UNITY_VALUES; // Enertion Unity // Disabled for now



// default board configuration
const int MAX_SPEED = 38;       // KM/H
const int MAX_RANGE = 45;       // KM
const int BATTERY_CELLS = 10;
const int BOARD_BATTERY_TYPE = 0;     // 0: LI-ION | 1: LIPO
const int MOTOR_POLES = 14;
const int WHEEL_DIAMETER = 90;
const int WHEEL_PULLEY = 36;
const int MOTOR_PULLEY = 15;






/*
  Endless ride - when remote is off and speed is over 12 km/h for 3 seconds,
  cruise control will be activated when speed drops below 12 km/h.

  Slide the board backwards while standing on it or foot brake
  to produce a spike in the current and stop the board.
*/
const bool  AUTO_CRUISE_ON = false;     // disabled by default
const float PUSHING_SPEED = 12.0;       // km/h
const float PUSHING_TIME = 3.0;         // seconds
const float CRUISE_CURRENT_SPIKE = 5.0; // Amps

// boad will stop after 30s if current is low
const float AUTO_CRUISE_TIME = 30.0;    // seconds
const float CRUISE_CURRENT_LOW = 5.0;   // Amps

// auto stop if remote is off and speed is over 20 km/h
const float MAX_PUSHING_SPEED = 20.0;   // km/h

// Auto stop (in seconds)
const float AUTO_BRAKE_TIME = 15.0;    // time to apply the full brakes
const int AUTO_BRAKE_RELEASE = 5;     // time to release brakes after the full stop

// UART
const int UART_SPEED = 115200;
const uint16_t uartPullInterval = 200;
const int UART_TIMEOUT = 10; // 10ms for 115200 bauds, 100ms for 9600 bauds
const int REMOTE_RX_TIMEOUT = 20; // ms



// REMOTE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


const int REMOTE_LOCK_TIMEOUT = 5; // seconds to lock throttle when idle
const int REMOTE_SLEEP_TIMEOUT = 180; // seconds to go to sleep mode

// turn off display if battery < 15%
const int DISPLAY_BATTERY_MIN = 15;



#define VERSION 2

// Remote > receiver
struct RemotePacket {
  uint32_t address;
  // --------------  // keep 4 byte alignment!
  uint8_t  version;  // 1
  uint8_t  command;	 // Throttle | Light | Settings
  uint8_t  data;     // e.g. throttle value
  uint8_t  counter;
  // --------------
};

// commands
const uint8_t SET_THROTTLE  = 1;
const uint8_t SET_CRUISE    = 2;

const uint8_t GET_CONFIG    = 3;
const uint8_t SET_STATE     = 4;
const uint8_t SET_PROFILE     = 5;
const uint8_t SET_MODE     = 6;
const uint8_t SET_BOARD_SHUTDOWN = 7;



// state machine
enum AppState {
  IDLE,       // remote is not connected
  NORMAL,
  PUSHING,
  CRUISE,
  ENDLESS,
  CONNECTED,  // riding with connected remote
  CONNECTING,
  MENU,
  STOPPING,   // emergency brake when remote has disconnected
  STOPPED,
  PAIRING,
  BOARDS_MENU,
  UPDATE,     // update over WiFi
  COASTING    // waiting for board to slowdown
};

enum ControlMode { CM_NORMAL, CM_PUSH_ASSIST};

// Receiver > remote  3 bytes
struct ReceiverPacket {
  uint8_t type;
  uint8_t chain;	// CRC from RemotePacket
  uint8_t state;   // Mode: Pairing, BT, ...
  uint8_t profile;

  uint8_t controlMode;
  uint8_t r1;
  uint8_t r2;
  uint8_t r3;
};

// responses type
const uint8_t ACK_ONLY  = 1;
const uint8_t TELEMETRY = 2;
const uint8_t CONFIG    = 3;
const uint8_t BOARD_ID  = 4;

struct InfoPacket {
  ReceiverPacket header;
  // --------------  // keep 4 byte alignment!
  int32_t id;
  // --------------
  uint8_t r0;
  uint8_t r1;
  uint16_t r2;
  // --------------
  uint16_t r3;
  uint16_t r4;
  // --------------
};

const int PACKET_SIZE = sizeof(InfoPacket);
const int CRC_SIZE = 1;

// New VESC values 12 + 3
struct TelemetryPacket {
  ReceiverPacket header;
  // -----------------  // keep 4 byte alignment!
  uint16_t speed;       // km/h * 100
  uint8_t tempMotor;
  uint8_t tempFET;
  // -----------------
  uint16_t voltage;     // volts * 100
  uint16_t distance;    // km * 100 - 10m accuracy
  // -----------------
  int16_t motorCurrent; // motor amps * 100
  int16_t inputCurrent; // battery amps * 100
  // -----------------

  uint16_t f2w(float f) { return f * 100; } // pack float
  float w2f(uint16_t w) { return float(w) / 100; }; // unpack float

  int16_t f2wi(float f) { return f * 100; } // pack float
  float w2fi(int16_t w) { return float(w) / 100; }; // unpack float

  float getSpeed() { return w2f(speed); }
  void setSpeed(float f) { speed = f2w(f); }

  float getVoltage() { return w2f(voltage); }
  void setVoltage(float f) { voltage = f2w(f); }

  float getDistance() { return w2f(distance); }
  void setDistance(float f) { distance = f2w(f); }

  float getMotorCurrent() { return w2fi(motorCurrent); }
  void setMotorCurrent(float f) { motorCurrent = f2wi(f); }

  float getInputCurrent() { return w2fi(inputCurrent); }
  void setInputCurrent(float f) { inputCurrent = f2wi(f); }
};

// board setting
struct ConfigPacket {
  ReceiverPacket header;
  // TODO: IS THIS NECESSARY?
  // -------------------  // keep 4 byte alignment!
  uint8_t  maxSpeed;	    // m/s
  uint8_t  maxRange;      // km
  uint8_t  batteryCells;
  uint8_t  batteryType;   // 0: Li-ion | 1: LiPo
  // -------------------
  uint8_t  motor_current_max;
  uint8_t  motor_current_brake;
  uint8_t  battery_current_max;
  uint8_t  battery_current_max_regen;
  // -------------------
  int16_t r1;  // battery amps * 100
  int16_t r2;
  // -------------------
  float getMaxSpeed() { return (maxSpeed) / 100; }
  void setMaxSpeed(float f) { maxSpeed = f * 100; }

  float getMotorCurrentMax() { return (motor_current_max) / 100; }
  void setMotorCurrentMax(float f) { motor_current_max = f * 100; }

  float getMotorCurrentBrake() { return (motor_current_brake) / 100; }
  void setMotorCurrentBrake(float f) { motor_current_brake = f * 100; }

  float getBatteryCurrentMax() { return (battery_current_max) / 100; }
  void setBatteryCurrentMax(float f) { battery_current_max = f * 100; }

  float getBatteryCurrentMaxRegen() { return (battery_current_max_regen) / 100; }
  void setBatteryCurrentMaxRegen(float f) { battery_current_max_regen = f * 100; }
};

const int default_throttle = 127;

#endif
