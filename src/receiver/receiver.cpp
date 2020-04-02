#include "receiver.h"

#ifdef ARDUINO_SAMD_FEATHER_M0 // Feather M0 w/Radio
#include <RH_RF69.h>

// Singleton instance of the radio driver
RH_RF69 radio(RF_CS, RF_DI0);

#elif ESP32
#include <LoRa.h>

// OTA
#include <SPI.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "wifi_credentials.h"

// Uart serial
HardwareSerial MySerial(1);
#endif

#ifdef RECEIVER_SCREEN
Adafruit_SSD1306 display(RST_OLED);
#endif

Smoothed<double> batterySensor;

Smoothed<double> motorCurrent;

#include "radio.h"

float signalStrength;
float lastRssi;

// Status blink LED
uint8_t statusCode = 0;
bool statusLedState = false;

unsigned long statusCycleTime, previousStatusMillis, currentMillis, startCycleMillis = 0;

unsigned long lastDelay;

// Initiate VescUart class for UART communication
bldcMeasure dataPackage;
mc_configuration motorConfigPackage;
nunchuckPackage chuckPackage;

void setup()
{
  // wait for VESC?
  //delay(1000);

  //Serial.begin(9600);

  // while (!Serial) {}; // wait for serial port to connect. Needed for native USB port only

  debug("Receiver");

  //loadEEPROMSettings();
  loadData();
  loadSettings();

  calculateRatios();

  if (receiverSettings.vescMode == UART_ONLY)
  {
    pinMode(LED, OUTPUT);
  }
  else if (receiverSettings.vescMode == UART_ADC)
  {
    pinMode(ADC_CRUISE, OUTPUT);
  }

  pinMode(AS_SWITCH, OUTPUT);
  digitalWrite(AS_SWITCH, LOW);

  // 10 seconds average
  batterySensor.begin(SMOOTHED_EXPONENTIAL, 10);

  // 1 sec average
  motorCurrent.begin(SMOOTHED_AVERAGE, 2);
  motorCurrent.add(0);

  //UART.setTimeout(UART_TIMEOUT); Might be necessary later on

#ifdef ARDUINO_SAMD_FEATHER_M0

#ifndef FAKE_UART
  UART.setSerialPort(&Serial1);
  Serial1.begin(UART_SPEED);
#endif

  initRadio(radio);

#elif ESP32

#ifndef FAKE_UART
  //UART.setSerialPort(&MySerial); Old
  SetSerialPort(&MySerial);
  // Comment this line for debugging without VESC
  MySerial.begin(UART_SPEED, SERIAL_8N1, RX, TX);
#endif

  initRadio();

  xTaskCreatePinnedToCore(
      coreTask,                 /* Function to implement the task */
      "coreTask",               /* Name of the task */
      10000,                    /* Stack size in words */
      NULL,                     /* Task input parameter */
      configMAX_PRIORITIES - 1, /* Priority of the task */
      NULL,                     /* Task handle. */
      0);                       /* Core where the task should run */

#endif

  debug("Setup complete - begin listening");

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);

#ifdef RECEIVER_SCREEN
  //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  //display.powerOn();
#endif
}

bool isTelemetryLost()
{
  return telemetryTime != 0                  // telemetry was active
         && secondsSince(telemetryTime) > 1; // not received recently
}

// safety check
bool isMoving()
{

  if (isTelemetryLost())
    return true; // assume movement for safety

  return telemetry.getSpeed() != 0; // moving in any direction
}

/*
   Calculate the battery level of the board based on the telemetry voltage
*/
float batteryPackPercentage(float voltage)
{

  float maxCellVoltage = 4.2;
  float minCellVoltage;

  if (boardConfig.batteryType == 0)
  { // Li-ion
    minCellVoltage = 3.1;
  }
  else
  { // Li-po
    minCellVoltage = 3.4;
  }

  float percentage = (100 - ((maxCellVoltage - voltage / boardConfig.batteryCells) / ((maxCellVoltage - minCellVoltage))) * 100);

  if (percentage > 100.0)
  {
    return 100.0;
  }
  else if (percentage < 0.0)
  {
    return 0.0;
  }

  return percentage;
}

bool prepareUpdate()
{

  // safety checks
  if (isMoving())
    return false;

  setState(UPDATE);

  // replace this with your WiFi network credentials
  const char *ssid = WIFI_SSID;         // e.g. "FBI Surveillance Van #34";
  const char *password = WIFI_PASSWORD; // e.g. "12345678";

  wifiStatus = "Connecting:";
  updateStatus = String(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    debug("Connection Failed!");
    delay(3000);
    // ESP.restart();
    return false;
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        updateStatus = "Start updating " + type;
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        updateStatus = "Progress: " + String(progress / (total / 100), 0) + "%";
        Serial.println(updateStatus);
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          updateStatus = "Auth Failed";
        else if (error == OTA_BEGIN_ERROR)
          updateStatus = "Begin Failed";
        else if (error == OTA_CONNECT_ERROR)
          updateStatus = "Connect Failed";
        else if (error == OTA_RECEIVE_ERROR)
          updateStatus = "Receive Failed";
        else if (error == OTA_END_ERROR)
          updateStatus = "End Failed";
      });

  ArduinoOTA.begin();

  wifiStatus = "IP: " + WiFi.localIP().toString();
  updateStatus = "Waiting...";
  debug(wifiStatus);
}

#ifdef RECEIVER_SCREEN

int getStringWidth(String s)
{

  int16_t x1, y1;
  uint16_t w1, h1;

  display.getTextBounds(s, 0, 0, &x1, &y1, &w1, &h1);
  return w1;
}

void drawString(String string, int x, int y, const GFXfont *font)
{

  display.setFont(font);

  display.setCursor(x, y);
  display.print(string);
}

void drawStringCentered(String string, int x, int y, const GFXfont *font)
{
  display.setFont(font);
  x = x - getStringWidth(string) / 2;
  drawString(string, x, y, font);
}

String getState()
{
  switch (receiverData.state)
  {
  case IDLE:
    return "Idle";
  case CONNECTED:
    return "Normal"; // dB?
  case STOPPING:
    return "Stopping";
  case STOPPED:
    return "Stopped";
  case PUSHING:
    return "Pushing";
  case ENDLESS:
    return "Cruise";
  case UPDATE:
    return "Update";
  }
}

void updateScreen()
{

  display.clearDisplay();

  switch (receiverData.state)
  {

  case IDLE:
    drawBattery();
    break;

  case ENDLESS:
    display.setTextColor(WHITE);
    display.setFont(fontDigital);

    display.setCursor(0, 20);
    display.println("CUR: " + String(telemetry.getMotorCurrent(), 1) + " A");
    display.println("SPD: " + String(telemetry.getSpeed(), 1));
    break;

  case UPDATE:
    display.setTextColor(WHITE);
    display.setFont(fontDesc);

    display.setCursor(0, 12);

    display.println(wifiStatus);
    display.println(updateStatus);
    break;

  default:
    if (throttle == default_throttle && !isMoving())
    {
      drawBattery();
    }
    else
    { // riding
      display.setTextColor(WHITE);
      display.setFont(fontDigital);

      display.setCursor(0, 20);
      display.println("THR: " + String(map(throttle, 0, 255, -100, 100)) + "%");
      display.println("SPD: " + String(telemetry.getSpeed(), 1) + " k");
    }
    break;
  }

  // ---- status ----
  display.setTextColor(WHITE);

  String s = getState() + "  " +
             String(telemetry.getVoltage(), 1) + "v  " +
             String(telemetry.getDistance(), 1) + "km";

#ifdef FAKE_UART
  s = "Board ID: " + String(boardID, HEX);
#endif

  drawStringCentered(s, 64, 62, fontDesc);

  display.display();
}

void drawBattery()
{

  display.setTextColor(WHITE);

  if (telemetry.getVoltage() == 0)
  {

    // no uart connection
    display.setFont();
    display.setCursor(0, 10);
    display.print("No UART data");

    // remote info
    display.setCursor(0, 25);
    if (!connected)
      display.print("Remote not connected");
    else
      display.print("Signal: " + String(lastRssi, 0) + " dB");

    //    display.setCursor(0, 40);
    //    display.print("Delay: " + String(lastDelay));

    return;
  }

  // --- battery ----
  int w = 120;
  int h = 46;
  display.drawRect(0, 0, w, h, WHITE);
  display.drawRect(1, 1, w - 2, h - 2, WHITE);

  display.fillRect(w, 10, 6, h - 10 - 10, WHITE);

  // fill
  float pc = batteryPackPercentage(telemetry.getVoltage());
  int x = w * pc / 100 - 8;
  display.fillRect(4, 4, x, h - 8, WHITE);

  // % value
  if (pc > 50)
  { // left side
    display.setTextColor(BLACK);
    drawStringCentered(String(pc, 0) + "%", x / 2 + 4, 31, fontBig);
  }
  else
  { // right side
    display.setTextColor(WHITE);
    drawStringCentered(String(pc, 0) + "%", x + (w - x) / 2, 31, fontBig);
  }
}
#endif // RECEIVER_SCREEN

void loop()
{ // core 1

  // get telemetry;
  //if(uartTelemetryAvailable) {
  getUartData(); // every 250  ms
  //}

#ifdef ARDUINO_SAMD_ZERO
  radioExchange();
  stateMachine();
#elif RECEIVER_SCREEN
  if (receiverData.state == UPDATE)
  {
    ArduinoOTA.handle();
  }
  //updateScreen(); // 25 ms
  vTaskDelay(1);
#endif
}

#ifdef ESP32
void coreTask(void *pvParameters)
{ // core 0

  String taskMessage = "radio task running on core ";
  taskMessage = taskMessage + xPortGetCoreID();
  Serial.println(taskMessage);

  while (true)
  {
    radioExchange();
    stateMachine();
    vTaskDelay(1);
  }
}
#endif

void pairingRequest()
{

  // safety checks

  if (millis() < 3000)
  {
    setState(PAIRING);
    return;
  }

  // todo: confirm pairing
}

bool receiveData()
{

  uint8_t len = sizeof(RemotePacket) + CRC_SIZE; // 9
  uint8_t buf[len];

  bool received = false;

#ifdef ARDUINO_SAMD_ZERO

  received = radio.recv(buf, &len);

#elif ESP32

  int bytes = 0;
  for (int i = 0; i < len; i++)
  {
    buf[i] = LoRa.read();
    bytes++;
  }
  // lastRssi = LoRa.packetRssi();
  len = bytes;
  received = true;

#endif

  if (!received)
    return false;

  // size check
  if (len != sizeof(RemotePacket) + CRC_SIZE)
  {
    debug("Wrong packet size");
    return false;
  }

  // crc check
  if (CRC8(buf, sizeof(remPacket)) != buf[sizeof(remPacket)])
  {
    debug("CRC mismatch");
    return false;
  }

  // process packet
  memcpy(&remPacket, buf, sizeof(remPacket));

// address check
#ifdef FAKE_UART
  // accept any address
#else
  if (remPacket.address != boardID)
  {
    // pairing request?
    if (!isMoving() && remPacket.command == SET_STATE && remPacket.data == PAIRING)
    {
      // accept any address
    }
    else
    {
      Serial.print("Wrong Board ID, please use: 0x");
      Serial.println(String(boardID, HEX));
      return false;
    }
  }
#endif

  if (remPacket.version != VERSION)
  {
    Serial.print("Version mismatch!");
  }

  return true;

  // signalStrength = constrain(map(radio.lastRssi(), -77, -35, 0, 100), 0, 100);
}

bool sendPacket(const void *packet, uint8_t len)
{

  // calc crc
  uint8_t crc = CRC8(packet, len);

  // struct to buffer
  len += CRC_SIZE;
  uint8_t buf[len];
  memcpy(buf, packet, len);
  buf[len - CRC_SIZE] = crc;

  bool sent = false;

#ifdef ESP32

  LoRa.beginPacket(len);
  int t = LoRa.write(buf, len);
  LoRa.endPacket();

  sent = t == len;
  // LoRa.receive(sizeof(remPacket) + CRC_SIZE);

#elif ARDUINO_SAMD_ZERO

  sent = radio.send(buf, len);

  if (sent)
    radio.waitPacketSent();

#endif

  return sent;
}

bool sendData(uint8_t response)
{

  // send packet
  switch (response)
  {

  case ACK_ONLY: // no extra data to send
    telemetry.header.type = response;
    telemetry.header.chain = remPacket.counter;
    telemetry.header.state = receiverData.state;
    telemetry.header.profile = receiverData.lastProfile;

    telemetry.header.controlMode = receiverData.controlMode;
    return sendPacket(&telemetry, sizeof(telemetry));

  case TELEMETRY:
    telemetry.header.type = response;
    telemetry.header.chain = remPacket.counter;
    telemetry.header.state = receiverData.state;
    telemetry.header.profile = receiverData.lastProfile;

    telemetry.header.controlMode = receiverData.controlMode;

    if (sendPacket(&telemetry, sizeof(telemetry)))
    {
      telemetryUpdated = false;
      return true;
    }
    break;

  case CONFIG:
    debug("Sending board configuration");
    boardConfig.header.type = response;
    boardConfig.header.chain = remPacket.counter;

    if (sendPacket(&boardConfig, sizeof(boardConfig)))
    {
      sendConfig = false; // send config once
      return true;
    }
    break;

  case BOARD_ID:
    debug("Sending board ID");
    boardInfo.header.type = response;
    boardInfo.header.chain = remPacket.counter;
    boardInfo.id = boardID;

    if (sendPacket(&boardInfo, sizeof(boardInfo)))
    {
      return true;
    }
    break;
  }

  return false;
}

bool dataAvailable()
{

#ifdef ARDUINO_SAMD_ZERO

  return radio.available();

#elif ESP32

  int packetSize = LoRa.parsePacket(sizeof(remPacket) + CRC_SIZE);
  return packetSize > 0;

#endif
}

void setState(AppState newState)
{

  switch (newState)
  {

  case IDLE:
    break;

  case PUSHING:
    timeSpeedReached = millis();
    break;

  case ENDLESS:
    if (isTelemetryLost())
      return;
    cruiseControlStart = millis();
    break;

  case CONNECTED:
    switch (receiverData.state)
    {
    case UPDATE:
      return;

      // monitor data
    case STOPPING:
      if (remPacket.data == default_throttle)
        return;
      break;
    case PUSHING:
    case ENDLESS:
    case COASTING:
      //if (remPacket.data == default_throttle) return; // Old code, would have only allowed remote to have impact upon acceleration/brake

      break;
    }
    // prevent auto-stop
    //timeoutTimer = millis();
    //connected = true;
    break;

  case STOPPING:
  case STOPPED:
    debug("disconnected");
    lastBrakeTime = millis();
    break;

  case UPDATE:
    break;
  case PAIRING:
    break;
  }

  // apply state
  //debug(newState);
  receiverData.state = newState;
}

void radioExchange()
{

  if (receiverData.state == UPDATE)
  {
    receivedData = false;
    return;
  }
  // controlStatusLed();

  /* Begin listen for transmission */
  if (dataAvailable())
  {

    // led on
    if (receiverSettings.vescMode == UART_ONLY)
      digitalWrite(LED, HIGH);

    if (receiveData())
    {

      receivedData = true;

      // debug( "New package: command " + String(remPacket.command) + ", data " + String(remPacket.data) + ", counter " + String(remPacket.counter) );

      // Send acknowledgement
      uint8_t response = ACK_ONLY;

      switch (remPacket.command)
      {
      case SET_THROTTLE:
      case SET_CRUISE:
        //setState(CONNECTED); // keep connection
        if (receiverData.state != PUSHING && receiverData.state != COASTING && receiverData.state != ENDLESS && receiverData.state != STOPPING)
              setState(CONNECTED);

        keepConnection();
        if (telemetryUpdated)
        {
          response = TELEMETRY;
        }
        break;

      case SET_STATE:
        switch (remPacket.data)
        {
        case UPDATE:
          if (!isMoving())
            prepareUpdate();
          break;
        case PAIRING:
          if (receiverData.state != UPDATE)
          {
            pairingRequest();
          }
          // request confirmed?
          if (receiverData.state == PAIRING)
            response = BOARD_ID;
          break;
        }
        break;
      case SET_MODE:
        if (!isMoving())
          setControlMode(remPacket.data);
        break;
      case SET_PROFILE:
        //pushVescProfile(remPacket.data);
        break;
      case SET_BOARD_SHUTDOWN:
        if (!isMoving())
          shutdownBoard();
        break;
      case GET_CONFIG:
        response = CONFIG;
        break;
      }

      // send config after power on
      if (sendConfig)
        response = CONFIG;

      // // allow remote to receive
      // delay(5);

      if (sendData(response))
      {
        // led on
        if (receiverSettings.vescMode == UART_ONLY)
          digitalWrite(LED, LOW);
        // debug("Sent response");
      }

      // control
      switch (remPacket.command)
      {
      case SET_THROTTLE: // control VESC speed
        // ignore during auto-stop/update/...
        if (receiverData.state == CONNECTED && receiverData.controlMode == CM_NORMAL)
        {
          setThrottle(remPacket.data);
        }
        else if (receiverData.controlMode == CM_PUSH_ASSIST)
        {
          throttle = remPacket.data;
          if (remPacket.data > 127)
            throttle = default_throttle;

          switch (receiverData.state)
          {
          case CONNECTED:
            setThrottle(throttle);
            break;

          case PUSHING:
          case ENDLESS:
          case COASTING:
            if (throttle < 127)
              setState(CONNECTED);
            break;

          default:
            break;
          }
        }
        break;

      case SET_CRUISE:
        if (receiverData.controlMode == CM_NORMAL)
        {
          setCruise();
        }
        else if (receiverData.controlMode == CM_PUSH_ASSIST)
        {
          throttle = remPacket.data;
          if (remPacket.data > 127)
            throttle = default_throttle;

          if(throttle < 127)
            setThrottle(throttle);
        }
        break;
      }
    }
    else
      receivedData = false;
  }
  /* End listen for transmission */
}

void autoCruise()
{

  if (millisSince(lastCruiseControl) > 50)
  {
    lastCruiseControl = millis();

    setCruise();
  }
}

void stateMachine()
{ // handle auto-stop, endless mode, etc...

  switch (receiverData.state)
  {

  case IDLE: // no remote connected

    setThrottle(default_throttle);
    break;

  case PUSHING: // pushing with remote connected
    // timeout handling
    connectionCheck();

    setThrottle(default_throttle);

    if (receiverData.controlMode != CM_PUSH_ASSIST || remPacket.command != SET_CRUISE || throttle != default_throttle)
    {
      setState(CONNECTED);
      return;
    }

    if (telemetry.getSpeed() >= MAX_PUSHING_SPEED)
    { // downhill
      setState(STOPPING);
      return;
    }

    if (telemetry.getSpeed() > PUSHING_SPEED)
    {
      if (secondsSince(timeSpeedReached) > PUSHING_TIME) {
        setState(ENDLESS); // start cruise control
        
      }
        
    }
    else if(telemetry.getSpeed() < PUSHING_SPEED)
    {
      setState(CONNECTED);
    }

    // OLD ENDLESS CODE
    /*
    if (telemetry.getSpeed() < PUSHING_SPEED)
    { // pushing ended
      if (AUTO_CRUISE_ON)
      {
        if (secondsSince(timeSpeedReached) > PUSHING_TIME)
          setState(ENDLESS); // start cruise control
        else
          setState(IDLE); // not enough pushing
      }
    }
    else if (telemetry.getSpeed() > MAX_PUSHING_SPEED)
    { // downhill
      setState(STOPPING);
    }
    **/
    break;

  case ENDLESS: // cruise without remote at ~12 km/h / 7 mph
    // timeout handling
    connectionCheck();

    if (receiverData.controlMode != CM_PUSH_ASSIST || remPacket.command != SET_CRUISE || secondsSince(cruiseControlStart) > AUTO_CRUISE_TIME || throttle != default_throttle)
    {
      setState(COASTING);
      return;
    }
    
    autoCruise();

    // OLD ENDLESS CODE
    // detect a foot brake /
    /**
    if (true)
    {
      double current = telemetry.getMotorCurrent(); // ~2 amps
      double smoothed = motorCurrent.get();

      // sudden change (> 5 A) after 2 seconds
      if (abs(current - smoothed) > CRUISE_CURRENT_SPIKE && secondsSince(cruiseControlStart) > 2)
      {
        setState(IDLE);
      }

      // switch to coasting after some time
      if (secondsSince(cruiseControlStart) > AUTO_CRUISE_TIME)
      {
        // keep cruise control downhill/uphill
        if (abs(current) <= CRUISE_CURRENT_LOW)
          setState(COASTING);
      }

      motorCurrent.add(current);
    }
    **/
    break;

  case COASTING: // waiting for board to slowdown
    // timeout handling
    connectionCheck();

    if (throttle != default_throttle) {
        setState(CONNECTED);
        return;
    }

    setThrottle(default_throttle);
    // avoid ENDLESS > IDLE > PUSHING loop
    if (telemetry.getSpeed() < PUSHING_SPEED)
      setState(CONNECTED);
    break;

  case CONNECTED: // remote is connected
    // timeout handling
    connectionCheck();

    if (receiverData.controlMode == CM_PUSH_ASSIST && telemetry.getSpeed() > PUSHING_SPEED && remPacket.command == SET_CRUISE && throttle == default_throttle)
    {
      setState(PUSHING);
    }

    break;

  case STOPPING: // emergency brake when remote has

    // start braking from zero throttle
    if (throttle > default_throttle)
    {
      throttle = default_throttle;
    }

    if (secondsSince(lastBrakeTime) > AUTO_BRAKE_INTERVAL)
    {

      // decrease throttle to brake  127 / 5 * 0.1
      float brakeForce = constrain(default_throttle / AUTO_BRAKE_TIME * AUTO_BRAKE_INTERVAL, 0, 10);

      // apply brakes
      if (throttle > brakeForce)
        throttle -= brakeForce;
      else
        throttle = 0;
      setThrottle(throttle);

      lastBrakeTime = millis();
    }

    // check speed
    if (throttle == 0 && !isMoving())
    {
      setState(STOPPED);
    }

    break;

  case STOPPED:

    // release brakes after a few seconds
    if (secondsSince(lastBrakeTime) > AUTO_BRAKE_RELEASE)
      setState(IDLE);
    break;

  case UPDATE:
    break;
  }
}

void setStatus(uint8_t code)
{

  short cycle = 0;

  // switch(code){
  //   case COMPLETE:  cycle = 500;    break;
  //   case FAILED:    cycle = 1400;   break;
  // }
  //
  // currentMillis = millis();
  //
  // if (currentMillis - startCycleMillis >= statusCycleTime){
  //   statusCode = code;
  //   statusCycleTime = cycle;
  //   startCycleMillis = currentMillis;
  // }
}

// Update a single setting value
void updateSetting(uint8_t setting, uint64_t value)
{
  // Map remote setting indexes to receiver settings
  switch (setting)
  {
  case 0:
    setting = 0;
    break; // TriggerMode
  case 7:
    setting = 1;
    break; // ControlMode
  case 11:
    setting = 2;
    break; // Address
  }

  // The address has changed, we need to reinitiate the receiver module
  if (setting == 2)
  {
    // initRadio(radio);
  }
}

/*
void setCruise ( bool cruise, uint16_t setPoint ){
  if ( rxSettings.controlMode == 0 ){

    setThrottle( setPoint );

  }
  else if ( rxSettings.controlMode == 1 ){

    setThrottle( setPoint );

  }
  else if ( rxSettings.controlMode == 2 ){

    // Setpoint not used (PID by VESC)
    UART.nunchuck.lowerButton = cruise;

    // Make sure the motor doesn't begin to spin wrong way under high load (and don't allow cruise backwards)
    // if ( returnData.rpm < 0 ){
    //
    //   UART.nunchuck.lowerButton = false;
    //   UART.nunchuck.valueY = 127;
    //   UART.setNunchuckValues();
    //   UART.setCurrent(0.0);
    //
    // } else{
    //
    //   UART.nunchuck.valueY = 127;
    //   UART.setNunchuckValues();
    //
    // }
  }
} */

void setThrottle(uint16_t value)
{
  // update display
  throttle = value;

// UART
#ifndef FAKE_UART

  /* Old
    UART.nunchuck.valueY = value;
    UART.nunchuck.upperButton = false;
    UART.nunchuck.lowerButton = false;
    UART.setNunchuckValues();
    **/

  if (receiverSettings.vescMode == UART_ONLY)
  {
    chuckPackage.valYJoy = value;
    chuckPackage.valUpperButton = false;
    chuckPackage.valLowerButton = false;
    VescUartSetNunchukValues(chuckPackage);
  }
  else if (receiverSettings.vescMode == UART_ADC)
  {
    dacWrite(ADC_VESC, value);
    digitalWrite(ADC_CRUISE, HIGH);
  }

#endif
  // PPM
  //    digitalWrite(throttlePin, HIGH);
  //    delayMicroseconds(map(throttle, 0, 255, 1000, 2000) );
  //    digitalWrite(throttlePin, LOW);

  // remember throttle for smooth auto stop
  lastThrottle = throttle;
}

void setCruise()
{

// UART
#ifndef FAKE_UART
  /* Old
    UART.nunchuck.valueY = 127;
    UART.nunchuck.upperButton = false;
    UART.nunchuck.lowerButton = true;
    UART.setNunchuckValues();
    **/

  //throttle = default_throttle;

  if (receiverSettings.vescMode == UART_ONLY)
  {
    chuckPackage.valYJoy = 127;
    chuckPackage.valUpperButton = false;
    chuckPackage.valLowerButton = true;
    VescUartSetNunchukValues(chuckPackage);
  }
  else if (receiverSettings.vescMode == UART_ADC)
  {
    dacWrite(ADC_VESC, 127);
    digitalWrite(ADC_CRUISE, LOW);
  }

#endif
}

// void speedControl( uint16_t throttle , bool trigger )
// {
//   // Kill switch
//   if ( rxSettings.triggerMode == 0 ){
//     if ( trigger == true || throttle < 127 ){
//       setThrottle( throttle );
//     }
//     else{
//       setThrottle( default_throttle );
//     }
//   }
//
//   // Cruise control
//   else if ( rxSettings.triggerMode == 1 ){
//     if ( trigger == true ){
//
//       if ( cruising == false ){
//         cruiseThrottle = throttle;
//         cruiseRPM = returnData.rpm;
//         cruising = true;
//       }
//
//       setCruise( true, cruiseThrottle );
//
//     }else{
//       cruising = false;
//       setThrottle( throttle );
//     }
//   }
// }

/*
   Update values used to calculate speed and distance travelled.
*/
void calculateRatios()
{
  // Gearing ratio
  gearRatio = (float)receiverSettings.motorPulley / (float)receiverSettings.wheelPulley;
  // ERPM to Km/h
  ratioRpmSpeed = (gearRatio * 60 * (float)receiverSettings.wheelDiameter * 3.14156) / (((float)receiverSettings.motorPoles / 2) * 1E6);
  // Pulses to km travelled
  ratioPulseDistance = (gearRatio * (float)receiverSettings.wheelDiameter * 3.14156) / (((float)receiverSettings.motorPoles * 3) * 1E6);
}

// rpm to km/h
float rpm2speed(long rpm)
{
  return abs(ratioRpmSpeed * rpm);
}

// km/h to rpm
long speed2rpm(uint8_t speed)
{
  return speed / ratioRpmSpeed;
}

// tachometerAbs to km
float tach2dist(long tachometer)
{
  return ratioPulseDistance * tachometer;
}

void getUartData()
{
  if (millisSince(lastUartPull) >= uartPullInterval)
  {

    lastUartPull = millis();

// debug
#ifdef FAKE_UART
    batterySensor.add(41 + (rand() % 40) / 100.0);
    telemetry.setVoltage(batterySensor.get());
    telemetry.setDistance(rand() % 30);
    telemetry.setSpeed(0);
    telemetry.setMotorCurrent(-21);
    telemetry.setInputCurrent(12);
    telemetry.tempFET = 37;
    telemetry.tempMotor = 60;

    telemetryUpdated = true;
    delay(7);
    return;
#endif

    // Only get what we need
    if (VescUartGet(dataPackage))
    {
      // float dutyCycleNow;
      // float ampHours;
      // float ampHoursCharged;

      // smooth voltage readings
      float voltage = dataPackage.inpVoltage;
      batterySensor.add(voltage);

      if (batteryPackPercentage(voltage) > 0)
      {
        telemetry.setVoltage(batterySensor.get());
      }
      else
      { // ESC is off!
        telemetry.setVoltage(voltage);
      }

      telemetry.setSpeed(rpm2speed(dataPackage.rpm));
      telemetry.setDistance(tach2dist(dataPackage.tachometerAbs));
      telemetry.setMotorCurrent(dataPackage.avgMotorCurrent);
      telemetry.setInputCurrent(dataPackage.avgInputCurrent);

      // temperature
      telemetry.tempFET = round(dataPackage.tempFetFiltered);
      telemetry.tempMotor = round(dataPackage.tempMotorFiltered);
      if (telemetry.tempMotor > 160)
        telemetry.tempMotor = 0; // no sensor

      // safety check
      if (telemetry.getSpeed() > 100)
        return;

      lastDelay = millis() - lastUartPull;
      telemetryUpdated = true;
      telemetryTime = millis();
    }
    else
    {
      // returnData.ampHours       = 0.0;
      // returnData.inpVoltage     = 0.0;
      // returnData.rpm            = 0;
      // returnData.tachometerAbs  = 0;
      debug("No UART data received!");
    }
  }
}

String uint64ToString(uint64_t number)
{
  unsigned long part1 = (unsigned long)((number >> 32)); // Bitwise Right Shift
  unsigned long part2 = (unsigned long)((number));

  if (part1 == 0)
  {
    return String(part2, DEC);
  }

  return String(part1, DEC) + String(part2, DEC);
}

String uint64ToAddress(uint64_t number)
{
  unsigned long part1 = (unsigned long)((number >> 32)); // Bitwise Right Shift
  unsigned long part2 = (unsigned long)((number));

  return String(part1, HEX) + String(part2, HEX);
}

bool pushVescProfile(uint8_t profile)
{
  float tempMaxErpm;
  float tempMaxMinErpm;

  loadProfile(profile, true);

  tempMaxErpm = speed2rpm(vesc_profile[profile].maxSpeed) * (receiverSettings.motorPoles / 2);
  tempMaxMinErpm = speed2rpm(vesc_profile[profile].maxSpeed) * (receiverSettings.motorPoles / 2);

  if (tempMaxErpm > vesc_profile[0].abs_max_erpm)
    tempMaxErpm = vesc_profile[0].abs_max_erpm;
  if (tempMaxMinErpm > vesc_profile[0].abs_max_erpm_reverse)
    tempMaxMinErpm = vesc_profile[0].abs_max_erpm_reverse;

  uartTelemetryAvailable = false;
  delay(500);

  if (VescUartGet(motorConfigPackage))
  {
    delay(100);
    //motorConfigPackage.lo_current_max = 17.1;
    boardConfig.maxRange = (uint8_t)motorConfigPackage.lo_current_max;

    motorConfigPackage.lo_current_max = vesc_profile[profile].motor_current_max;
    motorConfigPackage.lo_current_min = vesc_profile[profile].motor_current_brake;
    motorConfigPackage.lo_in_current_max = vesc_profile[profile].battery_current_max;
    motorConfigPackage.lo_in_current_min = vesc_profile[profile].battery_current_max_regen;
    motorConfigPackage.l_battery_cut_start = vesc_profile[profile].battery_voltage_cutoff_start * receiverSettings.batteryCells;
    motorConfigPackage.l_battery_cut_end = vesc_profile[profile].battery_voltage_cutoff_end * receiverSettings.batteryCells;
    motorConfigPackage.l_max_erpm = tempMaxErpm;
    motorConfigPackage.l_min_erpm = tempMaxMinErpm;

    if (!VescUartSet(motorConfigPackage))
    {
      return false;
    }
  }
  else
  {
    return false;
  }

  //uartTelemetryAvailable = false;
  //delay(500);

  //VescUartGet(motorConfigPackage);
  //delay(100);
  //boardConfig.maxRange = (uint8_t) motorConfigPackage.lo_current_max;
  //motorConfigPackage.lo_in_current_max = 13.3;
  //VescUartSet(motorConfigPackage);

  //uartTelemetryAvailable = true;

  prefs.begin("FFNRD");
  prefs.putUChar("lastProfile", profile);
  prefs.end();

  boardConfig.setMaxSpeed(vesc_profile[profile].maxSpeed);
  boardConfig.setMotorCurrentMax(vesc_profile[profile].motor_current_max);
  boardConfig.setMotorCurrentBrake(vesc_profile[profile].motor_current_brake);
  boardConfig.setBatteryCurrentMax(vesc_profile[profile].battery_current_max);
  boardConfig.setBatteryCurrentMaxRegen(vesc_profile[profile].battery_current_max_regen);

  sendConfig = true;

  receiverData.lastProfile = profile;
  return true;
}

void setControlMode(uint8_t controlMode)
{
  receiverData.controlMode = controlMode;

  prefs.begin("FFNRD");
  prefs.putUChar("lastControlMode", controlMode);
  prefs.end();
}

void loadData()
{
  prefs.begin("FFNRD");

  receiverData.lastProfile = prefs.getUChar("lastProfile", 0);
  receiverData.controlMode = prefs.getUChar("lastControlMode", 0);

  prefs.end();
}

// Settings functions
void loadSettings()
{

  boardID = CPU::getID();
  Serial.println("Board ID: " + String(boardID, HEX));

  /*prefs.begin("FFNRS");

  // General settings
  receiverSettings.maxRange = prefs.getUChar("MAXRANGE", 20);
  receiverSettings.batteryCells = prefs.getUChar("BATTERYCELLS", 10);
  receiverSettings.batteryType = prefs.getUChar("BATTERYTYPE", 0);
  receiverSettings.motorPoles = prefs.getUChar("MOTORPOLES", 14);
  receiverSettings.wheelDiameter = prefs.getUChar("WHEELDIA", 97);
  receiverSettings.wheelPulley = prefs.getUChar("WHEELPULLEY", 36);
  receiverSettings.motorPulley = prefs.getUChar("MOTORPULLEY", 15);
  receiverSettings.vescCount = prefs.getUChar("VESCCOUNT", 1);

  // Estop settings
  receiverSettings.estopMax = prefs.getUChar("ESTOPMAX", 100);
  receiverSettings.estopTime = prefs.getFloat("ESTOPTIME", 10);
  receiverSettings.estopRelease = prefs.getUChar("ESTOPRELEASE", 5);
  receiverSettings.estopInterval = prefs.getFloat("ESTOPINTERVAL", 0.1);

  loadProfile(receiverData.lastProfile, false);

  prefs.end();**/

  // General settings
  receiverSettings.maxRange = 60;
  receiverSettings.batteryCells = 12;
  receiverSettings.batteryType = 0;
  receiverSettings.motorPoles = 14;
  receiverSettings.wheelDiameter = 97;
  receiverSettings.wheelPulley = 36;
  receiverSettings.motorPulley = 14;
  receiverSettings.vescCount = 1;
  receiverSettings.vescMode = UART_ADC;

  // Estop settings
  receiverSettings.estopMax = 100;
  receiverSettings.estopTime = 15;
  receiverSettings.estopRelease = 5;
  receiverSettings.estopInterval = 0.1;

  vesc_profile[0].maxSpeed = 40;
  vesc_profile[0].motor_current_max = 60.0;
  vesc_profile[0].motor_current_brake = 60.0;
  vesc_profile[0].battery_current_max = 50.0;
  vesc_profile[0].battery_current_max_regen = 20.0;
  vesc_profile[0].battery_voltage_cutoff_start = 3.2;
  vesc_profile[0].battery_voltage_cutoff_end = 3.0;
  vesc_profile[0].abs_max_erpm = 50000.0;
  vesc_profile[0].abs_max_erpm_reverse = 50000.0;

  setBoardConfig();
}

void loadProfile(uint8_t profile, bool openprefs)
{
  if (openprefs)
    prefs.begin("FFNRS");

  switch (profile)
  {

  case 0:
    vesc_profile[0].maxSpeed = prefs.getUChar("P1MAXSPEED", 30);
    vesc_profile[0].motor_current_max = prefs.getFloat("P1MOTORMAX", 40);
    vesc_profile[0].motor_current_brake = prefs.getFloat("P1MOTORBRAKE", 40);
    vesc_profile[0].battery_current_max = prefs.getFloat("P1BATTMAX", 20);
    vesc_profile[0].battery_current_max_regen = prefs.getFloat("P1BATTREGEN", 5);
    vesc_profile[0].battery_voltage_cutoff_start = prefs.getFloat("P1BATTCUTST", 3.2);
    vesc_profile[0].battery_voltage_cutoff_end = prefs.getFloat("P1BATTCUTEND", 3);
    vesc_profile[0].abs_max_erpm = prefs.getFloat("P1ABSMAXERPM", 50000);
    vesc_profile[0].abs_max_erpm_reverse = prefs.getFloat("P1ABSMAXERPMR", 50000);
    break;

  case 1:
    vesc_profile[1].maxSpeed = prefs.getUChar("P2MAXSPEED", 30);
    vesc_profile[1].motor_current_max = prefs.getFloat("P2MOTORMAX", 40);
    vesc_profile[1].motor_current_brake = prefs.getFloat("P2MOTORBRAKE", 40);
    vesc_profile[1].battery_current_max = prefs.getFloat("P2BATTMAX", 20);
    vesc_profile[1].battery_current_max_regen = prefs.getFloat("P2BATTREGEN", 5);
    vesc_profile[1].battery_voltage_cutoff_start = prefs.getFloat("P2BATTCUTST", 3.2);
    vesc_profile[1].battery_voltage_cutoff_end = prefs.getFloat("P2BATTCUTEND", 3);
    vesc_profile[1].abs_max_erpm = prefs.getFloat("P2ABSMAXERPM", 50000);
    vesc_profile[1].abs_max_erpm_reverse = prefs.getFloat("P2ABSMAXERPMR", 50000);
    break;

  case 2:
    vesc_profile[2].maxSpeed = prefs.getUChar("P3MAXSPEED", 30);
    vesc_profile[2].motor_current_max = prefs.getFloat("P3MOTORMAX", 40);
    vesc_profile[2].motor_current_brake = prefs.getFloat("P3MOTORBRAKE", 40);
    vesc_profile[2].battery_current_max = prefs.getFloat("P3BATTMAX", 20);
    vesc_profile[2].battery_current_max_regen = prefs.getFloat("P3BATTREGEN", 5);
    vesc_profile[2].battery_voltage_cutoff_start = prefs.getFloat("P3BATTCUTST", 3.2);
    vesc_profile[2].battery_voltage_cutoff_end = prefs.getFloat("P3BATTCUTEND", 3);
    vesc_profile[2].abs_max_erpm = prefs.getFloat("P3ABSMAXERPM", 50000);
    vesc_profile[2].abs_max_erpm_reverse = prefs.getFloat("P3ABSMAXERPMR", 50000);
    break;

  case 3:
    vesc_profile[3].maxSpeed = prefs.getUChar("P4MAXSPEED", 30);
    vesc_profile[3].motor_current_max = prefs.getFloat("P4MOTORMAX", 40);
    vesc_profile[3].motor_current_brake = prefs.getFloat("P4MOTORBRAKE", 40);
    vesc_profile[3].battery_current_max = prefs.getFloat("P4BATTMAX", 20);
    vesc_profile[3].battery_current_max_regen = prefs.getFloat("P4BATTREGEN", 5);
    vesc_profile[3].battery_voltage_cutoff_start = prefs.getFloat("P4BATTCUTST", 3.2);
    vesc_profile[3].battery_voltage_cutoff_end = prefs.getFloat("P4BATTCUTEND", 3);
    vesc_profile[3].abs_max_erpm = prefs.getFloat("P4ABSMAXERPM", 50000);
    vesc_profile[3].abs_max_erpm_reverse = prefs.getFloat("P4ABSMAXERPMR", 50000);
    break;

  case 4:
    vesc_profile[4].maxSpeed = prefs.getUChar("P5MAXSPEED", 30);
    vesc_profile[4].motor_current_max = prefs.getFloat("P5MOTORMAX", 40);
    vesc_profile[4].motor_current_brake = prefs.getFloat("P5MOTORBRAKE", 40);
    vesc_profile[4].battery_current_max = prefs.getFloat("P5BATTMAX", 20);
    vesc_profile[4].battery_current_max_regen = prefs.getFloat("P5BATTREGEN", 5);
    vesc_profile[4].battery_voltage_cutoff_start = prefs.getFloat("P5BATTCUTST", 3.2);
    vesc_profile[4].battery_voltage_cutoff_end = prefs.getFloat("P5BATTCUTEND", 3);
    vesc_profile[4].abs_max_erpm = prefs.getFloat("P5ABSMAXERPM", 50000);
    vesc_profile[4].abs_max_erpm_reverse = prefs.getFloat("P5ABSMAXERPMR", 50000);
    break;

    if (openprefs)
      prefs.end();
  }
}

void setBoardConfig()
{
  boardConfig.maxSpeed = vesc_profile[receiverData.lastProfile].maxSpeed;
  boardConfig.maxRange = receiverSettings.maxRange;
  boardConfig.batteryCells = receiverSettings.batteryCells;
  boardConfig.batteryType = receiverSettings.batteryType;
  boardConfig.setMotorCurrentMax(vesc_profile[receiverData.lastProfile].motor_current_max);
  boardConfig.setMotorCurrentBrake(vesc_profile[receiverData.lastProfile].motor_current_brake);
  boardConfig.setBatteryCurrentMax(vesc_profile[receiverData.lastProfile].battery_current_max);
  boardConfig.setBatteryCurrentMaxRegen(vesc_profile[receiverData.lastProfile].battery_current_max_regen);
}

bool inRange(int val, int minimum, int maximum)
{
  return ((minimum <= val) && (val <= maximum));
}

void debug(String x)
{
  Serial.println(x);
}

void shutdownBoard()
{
  digitalWrite(AS_SWITCH, HIGH);
}

void connectionCheck()
{
  if (millisSince(timeoutTimer) > timeoutMax)
  {
    debug("receiver timeout");

    // No speed is received within the timeout limit.
    connected = false;
    timeoutTimer = millis();

    switch (receiverData.state)
    {
    case PUSHING:
    case ENDLESS:
    case COASTING:
      setState(IDLE);
      break;

    default:
      setState(STOPPING);
      break;
    }

    // use last throttle
    throttle = lastThrottle;
  }
}

void keepConnection()
{
  timeoutTimer = millis();
  connected = true;
}