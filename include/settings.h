#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <Arduino.h>
#include <datatypes.h>
#include <Preferences.h>

// Remote specific settings from here
struct RemoteSettings {
    uint8_t remoteLockTimeout;
    uint8_t remoteSleepTimeout;
    uint8_t displayBatteryMin;
} remote;

struct BoardNames {
    String board0;
    String board1;
    String board2;
    String board3;
    String board4;
    String board5;
    String board6;
    String board7;
    String board8;
    String board9;
    String board10;
    String board11;
    String board12;
    String board13;
    String board14;
    String board15;
    String board16;
    String board17;
    String board18;
    String board19;
} boardNames;

// Receiver specific settings from here
struct ReceiverSettings {
    uint8_t  maxRange           = 40;      // km
    uint8_t  batteryCells       = 10;
    uint8_t  batteryType        = 0;   // 0: Li-ion | 1: LiPo
    uint8_t  motorPoles         = 14;
    uint8_t  wheelDiameter      = 97;
    uint8_t  wheelPulley        = 36;
    uint8_t  motorPulley        = 15;
    uint8_t  vescCount          = 1;
    uint8_t vescMode            = 0;

    // ESTOP
    uint8_t estopMaxBrake       = 100;
    float estopTime             = 10.0;
    uint8_t estopRelease        = 5;
    float estopInterval         = 0.1;
} receiver;

// Profiles from here (BOARD SPECIFIC)
struct VescProfile1 {
    uint8_t maxSpeed                        = 11;
    float motor_current_max                 = 1.1;
    float motor_current_brake               = 1.2;
    float battery_current_max               = 1.3;
    float battery_current_max_regen         = 1.4;
    float battery_voltage_cutoff_start      = 32.1;
    float battery_voltage_cutoff_end        = 29.1;
    float abs_max_erpm                      = 50000; 
    float abs_max_erpm_reverse              = 50000;
} profile1;

struct VescProfile2 {
    uint8_t maxSpeed                        = 22;
    float motor_current_max                 = 2.1;
    float motor_current_brake               = 2.2;
    float battery_current_max               = 2.3;
    float battery_current_max_regen         = 2.4;
    float battery_voltage_cutoff_start      = 32.2;
    float battery_voltage_cutoff_end        = 29.2;
    float abs_max_erpm                      = 50000; 
    float abs_max_erpm_reverse              = 50000;
} profile2;

struct VescProfile3 {
    uint8_t maxSpeed                        = 33;
    float motor_current_max                 = 3.1;
    float motor_current_brake               = 3.2;
    float battery_current_max               = 3.3;
    float battery_current_max_regen         = 3.4;
    float battery_voltage_cutoff_start      = 32.3;
    float battery_voltage_cutoff_end        = 29.3;
    float abs_max_erpm                      = 50000; 
    float abs_max_erpm_reverse              = 50000;
} profile3;

struct VescProfile4 {
    uint8_t maxSpeed                        = 44;
    float motor_current_max                 = 4.1;
    float motor_current_brake               = 4.2;
    float battery_current_max               = 4.3;
    float battery_current_max_regen         = 4.4;
    float battery_voltage_cutoff_start      = 32.4;
    float battery_voltage_cutoff_end        = 29.4;
    float abs_max_erpm                      = 50000; 
    float abs_max_erpm_reverse              = 50000;
} profile4;

struct VescProfile5 {
    uint8_t maxSpeed                        = 80;
    float motor_current_max                 = 5.1;
    float motor_current_brake               = 5.2;
    float battery_current_max               = 5.3;
    float battery_current_max_regen         = 5.4;
    float battery_voltage_cutoff_start      = 32.5;
    float battery_voltage_cutoff_end        = 29.5;
    float abs_max_erpm                      = 50000; 
    float abs_max_erpm_reverse              = 50000;
} profile5;




/** Uncomment this to set default settings




**/



#endif