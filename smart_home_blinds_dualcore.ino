#ifdef ENABLE_DEBUG
       #define DEBUG_ESP_PORT Serial
       #define NODEBUG_WEBSOCKETS
       #define NDEBUG
#endif 

#include <Arduino.h>
#ifdef ESP8266 
       #include <ESP8266WiFi.h>
#endif 
#ifdef ESP32   
       #include <WiFi.h>
#endif

#include "SinricPro.h"
#include "SinricProBlinds.h"

#define WIFI_SSID         "YOUR_WIFI_NAME"    
#define WIFI_PASS         "YOUR_WIFI_PASSWORD"
#define APP_KEY           "SINRIC_APP_KEY"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "SINRIC_APP_SECRET"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define RIGHT_BLINDS_ID   "SINRIC_BLINDS1_ID"    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define LEFT_BLINDS_ID    "SINRIC_BLINDS2_ID"    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define BAUD_RATE         9600                // Change baudrate to your need

int blindsPosition = 0;
bool powerState = false;

//const int LED_BUILTIN = 2;
bool dir = true;
int step_position_right = 0;
int step_position_left = 0;
const int dir_pin1 = 23;
const int step_pin1 = 22;
const int dir_pin2 = 18;
const int step_pin2 = 19;
int steptime = 1;
const int powerOnPin = 17;
const int powerControlPin = 16;
const int top_steps = 17650;
bool top = true;

TaskHandle_t Task1;

int *step_position;
int step_position_old;
int steps;
bool blind;
bool old_blind;
bool async_running = false;
bool sync_running = false;

void Async( void * pvParameters ){

  for(;;){
    if(async_running == true){
      int steps_loc = steps;
      int step_position_loc = step_position_old;
      bool blind_loc = blind;
      int steps_abs_loc = steps_loc;

      if(steps_loc < 0)
        steps_abs_loc *= -1;

      for (size_t i = 0; i < steps_abs_loc; i++){
        if(steps_loc > 0){    
          digitalWrite(LED_BUILTIN, true);
          
          if(blind_loc){
            digitalWrite(dir_pin1, 1);
            digitalWrite(step_pin1, 1);
            digitalWrite(step_pin1, 0);
          }else{
            digitalWrite(dir_pin2, 1);
            digitalWrite(step_pin2, 1);
            digitalWrite(step_pin2, 0);
          }

          delay(steptime);
        }else{
          digitalWrite(LED_BUILTIN, true);
          
          if(blind_loc){
            digitalWrite(dir_pin1, 0);
            digitalWrite(step_pin1, 1);
            digitalWrite(step_pin1, 0);
          }else{
            digitalWrite(dir_pin2, 0);
            digitalWrite(step_pin2, 1);
            digitalWrite(step_pin2, 0);
          }

          delay(steptime);
        }
      }

      if(!sync_running)
        digitalWrite(powerOnPin, true);

      async_running = false;
    }

    delay(100);
  } 
}

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Device %s power turned %s \r\n", deviceId.c_str(), state?"on":"off");
  powerState = state;
  return true; // request handled properly
}

bool onRangeValue(const String &deviceId, int &position) {
  
  if(async_running)
    delay(100);

  Serial.println("Absolute");

  if(deviceId == "63dc458d22e49e3cb5f7617b"){
    blind = true;
    step_position = &step_position_right;
    
  }else if(deviceId == "63dc45aa22e49e3cb5f761ab"){
    blind = false;
    step_position = &step_position_left;
  }else
   return false;
  
  powerOn();

  steps = map(position, 0, 100, 0, top_steps) - *step_position;
  step_position_old = *step_position;
  *step_position += steps;

  Serial.print("blind: ");
  Serial.println(blind);  

  if(!async_running){
    Serial.println("Starting Async");
    async_running = true;
    old_blind = blind;
    return true;
  }

  sync_running = true;
  while(old_blind == blind && async_running){delay(100);};

  Serial.println("Starting Sync");

  for (size_t i = 0; i < abs(steps); i++){
    if(steps > 0)    
      step(blind, 1);
    else
      step(blind, 0);
  }

  if(!async_running)
    digitalWrite(powerOnPin, true);

  sync_running = false;
  Serial.printf("Device %s set position to %d\r\n", deviceId.c_str(), position);
  return true; // request handled properly
}

bool onAdjustRangeValue(const String &deviceId, int &positionDelta) {

  Serial.println("Delta");

  int *step_position;
  bool blind;
  if(deviceId == "63dc458d22e49e3cb5f7617b"){
    blind = true;
    step_position = &step_position_right;
    
  }else if(deviceId == "63dc45aa22e49e3cb5f761ab"){
    blind = false;
    step_position = &step_position_left;
  }else
   return false;
  
  powerOn();

  int steps = map(positionDelta, 0, 100, 0, top_steps);

  for (size_t i = 0; i < abs(steps); i++){
    if(steps > 0)    
      step(blind, 1);
    else
      step(blind, 0);
  }
  
  *step_position += steps;
  digitalWrite(powerOnPin, true);

  blindsPosition += positionDelta;
  Serial.printf("Device %s position changed about %i to %d\r\n", deviceId.c_str(), positionDelta, blindsPosition);
  positionDelta = blindsPosition; // calculate and return absolute position
  return true; // request handled properly
}


// setup function for WiFi connection
void setupWiFi() {
  Serial.printf("\r\n[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  IPAddress localIP = WiFi.localIP();
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
}

void setupSinricProRight() {
  // get a new Blinds device from SinricPro
  SinricProBlinds &myBlinds = SinricPro[RIGHT_BLINDS_ID];
  myBlinds.onPowerState(onPowerState);
  myBlinds.onRangeValue(onRangeValue);
  myBlinds.onAdjustRangeValue(onAdjustRangeValue);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setupSinricProLeft() {
  // get a new Blinds device from SinricPro
  SinricProBlinds &myBlinds = SinricPro[LEFT_BLINDS_ID];
  myBlinds.onPowerState(onPowerState);
  myBlinds.onRangeValue(onRangeValue);
  myBlinds.onAdjustRangeValue(onAdjustRangeValue);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setupBlinds(){
  pinMode(dir_pin1, OUTPUT);
  pinMode(step_pin1, OUTPUT);
  pinMode(dir_pin2, OUTPUT);
  pinMode(step_pin2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(powerOnPin, OUTPUT);
  pinMode(powerControlPin, INPUT_PULLUP);

  digitalWrite(powerOnPin, 1);
  digitalWrite(step_pin1, 0);
  digitalWrite(step_pin2, 0);
  digitalWrite(LED_BUILTIN, false);
}

void powerOn(){
  digitalWrite(powerOnPin, false);
  while (!digitalRead(powerControlPin)){
    digitalWrite(LED_BUILTIN, false);}
}

void step(bool blind, bool dir){
  
  digitalWrite(LED_BUILTIN, true);
  
  if(blind){
     digitalWrite(dir_pin1, dir);
     digitalWrite(step_pin1, 1);
     digitalWrite(step_pin1, 0);
  }else{
    digitalWrite(dir_pin2, dir);
    digitalWrite(step_pin2, 1);
    digitalWrite(step_pin2, 0);
  }

  delay(steptime);
}

void setup() {
  Serial.begin(BAUD_RATE); Serial.printf("\r\n\r\n");
  setupWiFi();
  setupSinricProRight();
  setupSinricProLeft();
  setupBlinds();

  xTaskCreatePinnedToCore(
    Async,   /* Task function. */
    "Task1",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task1,      /* Task handle to keep track of created task */
    0);          /* pin task to core 0 */         

  delay(500);
}

void loop() {
  SinricPro.handle();
}