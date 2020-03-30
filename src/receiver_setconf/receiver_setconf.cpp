#include "settings.h"

Preferences p;

void setup() {
    delay(5000);
    Serial.begin(115200);

    p.begin("FFNRS");
    p.clear();

    // General settings
    p.putUChar("MAXRANGE", receiver.maxRange);
    p.putUChar("BATTERYCELLS", receiver.batteryCells);
    p.putUChar("BATTERYTYPE", receiver.batteryType);
    p.putUChar("MOTORPOLES", receiver.motorPoles);
    p.putUChar("WHEELDIA", receiver.wheelDiameter);
    p.putUChar("WHEELPULLEY", receiver.wheelPulley);
    p.putUChar("MOTORPULLEY", receiver.motorPulley);
    p.putUChar("VESCCOUNT", receiver.vescCount);
    p.putUChar("VESCMODE", receiver.vescMode);

    // Estop settings
    p.putUChar("ESTOPMAX", receiver.estopMaxBrake);
    p.putFloat("ESTOPTIME", receiver.estopTime);
    p.putUChar("ESTOPRELEASE", receiver.estopRelease);
    p.putFloat("ESTOPINTERVAL", receiver.estopInterval);

    // Advanced settings

    // Profile 1
    p.putUChar("P1MAXSPEED", profile1.maxSpeed);
    p.putFloat("P1MOTORMAX", profile1.motor_current_max);
    p.putFloat("P1MOTORBRAKE", profile1.motor_current_brake);
    p.putFloat("P1BATTMAX", profile1.battery_current_max);
    p.putFloat("P1BATTREGEN", profile1.battery_current_max_regen);
    p.putFloat("P1BATTCUTST", profile1.battery_voltage_cutoff_start);
    p.putFloat("P1BATTCUTEND", profile1.battery_voltage_cutoff_end);
    p.putFloat("P1ABSMAXERPM", profile1.abs_max_erpm);
    p.putFloat("P1ABSMAXERPMR", profile1.abs_max_erpm_reverse);

    // Profile 2
    p.putUChar("P2MAXSPEED", profile2.maxSpeed);
    p.putFloat("P2MOTORMAX", profile2.motor_current_max);
    p.putFloat("P2MOTORBRAKE", profile2.motor_current_brake);
    p.putFloat("P2BATTMAX", profile2.battery_current_max);
    p.putFloat("P2BATTREGEN", profile2.battery_current_max_regen);
    p.putFloat("P2BATTCUTST", profile2.battery_voltage_cutoff_start);
    p.putFloat("P2BATTCUTEND", profile2.battery_voltage_cutoff_end);
    p.putFloat("P2MAXERPM", profile2.abs_max_erpm);
    p.putFloat("P2MAXERPMREV", profile2.abs_max_erpm_reverse);

    // Profile 3
    p.putUChar("P3MAXSPEED", profile3.maxSpeed);
    p.putFloat("P3MOTORMAX", profile3.motor_current_max);
    p.putFloat("P3MOTORBRAKE", profile3.motor_current_brake);
    p.putFloat("P3BATTMAX", profile3.battery_current_max);
    p.putFloat("P3BATTREGEN", profile3.battery_current_max_regen);
    p.putFloat("P3BATTCUTST", profile3.battery_voltage_cutoff_start);
    p.putFloat("P3BATTCUTEND", profile3.battery_voltage_cutoff_end);
    p.putFloat("P3MAXERPM", profile3.abs_max_erpm);
    p.putFloat("P3MAXERPMREV", profile3.abs_max_erpm_reverse);

    // Profile 4
    p.putUChar("P4MAXSPEED", profile4.maxSpeed);
    p.putFloat("P4MOTORMAX", profile4.motor_current_max);
    p.putFloat("P4MOTORBRAKE", profile4.motor_current_brake);
    p.putFloat("P4BATTMAX", profile4.battery_current_max);
    p.putFloat("P4BATTREGEN", profile4.battery_current_max_regen);
    p.putFloat("P4BATTCUTST", profile4.battery_voltage_cutoff_start);
    p.putFloat("P4BATTCUTEND", profile4.battery_voltage_cutoff_end);
    p.putFloat("P4MAXERPM", profile4.abs_max_erpm);
    p.putFloat("P4MAXERPMREV", profile4.abs_max_erpm_reverse);

    // Profile 5
    p.putUChar("P5MAXSPEED", profile5.maxSpeed);
    p.putFloat("P5MOTORMAX", profile5.motor_current_max);
    p.putFloat("P5MOTORBRAKE", profile5.motor_current_brake);
    p.putFloat("P5BATTMAX", profile5.battery_current_max);
    p.putFloat("P5BATTREGEN", profile5.battery_current_max_regen);
    p.putFloat("P5BATTCUTST", profile5.battery_voltage_cutoff_start);
    p.putFloat("P5BATTCUTEND", profile5.battery_voltage_cutoff_end);
    p.putFloat("P5MAXERPM", profile5.abs_max_erpm);
    p.putFloat("P5MAXERPMREV", profile5.abs_max_erpm_reverse);

    Serial.print("\nReceiver settings set!");
    Serial.print("\nFree entries:" + String(p.freeEntries()));
    p.end();
}

void loop() { ;; }