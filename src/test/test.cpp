/* WIFI AP CAPTIVE 
#include <WiFi.h>
#include <DNSServer.h>

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WiFiServer server(80);

String responseHTML = ""
  "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body>"
  "<h1>Hello World!</h1><p>This is a captive portal example. All requests will "
  "be redirected here.</p></body></html>";

void setup() { 
  WiFi.disconnect();   //added to start with the wifi off, avoid crashing
  WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("FireFly Nano Remote");

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(responseHTML);
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
  }
}


**/

/*
  This is a simple example to test Micro TF card via SPI 1.
  Debug log output via UART.

  Micro TF card pins to ESP32 GPIOs via this connecthin:
  SD_CS   -- GPIO22
  SD_MOSI -- GPIO23
  SD_SCK  -- GPIO17
  SD_MISO -- GPIO13

  by Aaron.Lee from HelTec AutoMation, ChengDu, China
  �ɶ��������Զ����Ƽ����޹�˾
  https://heltec.org

  this project also realess in GitHub:
  https://github.com/HelTecAutomation/Heltec_ESP32
*/

/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/


/**
#include <Arduino.h>
#include "CPU.h"
#include "utils.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  BLEDevice::init("Long name works now");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setValue("Hello World says Neil");
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
}

**/

#include <Arduino.h>
#include "CPU.h"
#include "utils.h"
#include "VescUart.h"
#include "test.h"
#include "datatypes.h"

HardwareSerial MySerial(1);

mc_configuration motorConfigPackage;
bldcMeasure bldc;

void setup() {
  delay(1000);
  pinMode(LED, OUTPUT);

   //Serial.begin(9600);

  SetSerialPort(&MySerial);
      // Comment this line for debugging without VESC
  MySerial.begin(115200, SERIAL_8N1, RX, TX);
 


  delay(1000);

  test();

}

bool test()   {
  if(!VescUartGet(motorConfigPackage)) {
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    return false;
  }


  delay(100);
  motorConfigPackage.l_max_erpm = 34400.00;
  
  if(!VescUartSet(motorConfigPackage)){
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    return false;
  }

  digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED, LOW);
}

void loop() {
  //test();

  delay(5000);
  if(VescUartGet(bldc)){
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    if(bldc.inpVoltage > 30.0){
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    Serial.write((int) bldc.inpVoltage);
    }
  }
}