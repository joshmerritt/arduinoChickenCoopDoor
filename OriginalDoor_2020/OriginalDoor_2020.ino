
#include <FastLED.h>
#include <Servo.h>

// Define which pins will be used for motor control, switches, LEDs, and the light sensor.
#define powerLevelPin 9
#define input1 6
#define input2 7
#define sensorPin A0
#define bottomSwitchPin 12
#define topSwitchPin 13
#define LED_PIN     3
#define NUM_LEDS    2
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
CRGB leds[NUM_LEDS];
#define UPDATES_PER_SECOND 100

int BRIGHTNESS = 100;
int morningLightLevel = 250;
int nightLightLevel = 800;
int currentLightLevel = 0;
int lightLevel15secAgo = 0;
int lightLevel30secAgo = 0;
int powerLevel = 65;
int bottomSwitch = 0;
int topSwitch = 0;
int runCount = 0;
int timeOut = 150;
boolean flasher = false;
int openAttempts = 0;
int closeAttempts = 0;


// Setup (runs once)
void setup() {
  Serial.begin(9600);
  Serial.println("Booting Up . . .");
  delay(3000);
  //Setup pins for LEDs, motor, door switches, and light sensor
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );
  pinMode(powerLevelPin, OUTPUT);
  pinMode(input1, OUTPUT);
  pinMode(input2, OUTPUT);
  pinMode(bottomSwitchPin, INPUT_PULLUP);
  pinMode(topSwitchPin, INPUT_PULLUP);
  pinMode(sensorPin, INPUT_PULLUP);
  //initial readings
  allOff(); 
  readDoorSwitches(); 
}

// Main Code
// Read light level, then see if door needs to be moved
void loop() {
  readLightLevel();
  checkDoor();
}

// Delays 15 seconds, updates 'past' light level readings and captures current light level reading.
// Updates brightness to scale with light level and negates 'flasher'
void readLightLevel() {
  delay(15000);
  lightLevel30secAgo = lightLevel15secAgo;
  lightLevel15secAgo = currentLightLevel;
  currentLightLevel = analogRead(sensorPin);
  readDoorSwitches();
  Serial.println("readLightLevel : Current : 15secAgo : 30secAgo");
  Serial.println(currentLightLevel);
  Serial.println(lightLevel15secAgo);
  Serial.println(lightLevel30secAgo);
  Serial.println("open attempts : close attempts");
  Serial.println(openAttempts);
  Serial.println(closeAttempts);
  flasher = !flasher;
  BRIGHTNESS = (111 - (currentLightLevel/10));
  FastLED.setBrightness(BRIGHTNESS);
  updateLED();
}

// Checks the light level and door status. If action needed, rechecks light level. 
// Then opens or closes door if past 3 light level readings exceeded the threshold
void checkDoor() {
    Serial.println("Door('closed') function");
    Serial.println(door("Closed"));
    Serial.println("Door('open') function");
    Serial.println(door("Open"));
    Serial.println("readingsExceed morningLightLevel function");
    Serial.println(readingsExceed(morningLightLevel));
    Serial.println("readingsExceed nightLightLevel function");
    Serial.println(readingsExceed(nightLightLevel));
    
    if(door("Closed") && readingsExceed(morningLightLevel)) {
        Serial.println("Bright and door not open, recheck");
        readLightLevel();
        if(door("Closed") && readingsExceed(morningLightLevel)) {     
              runCount = 0;
              powerMotor("Up");
              openAttempts++;  
              while(topSwitch == 1) {
                  flashLights(runCount,"Up");
                  readDoorSwitches();
                  runCount++;
                  Serial.println(runCount);
                  if(runCount > timeOut) break;
                }
              allOff();
              updateLED();
              if(topSwitch == 0) openAttempts = 0;
        }
        allOff();
    }
    if(door("Open") && readingsExceed(nightLightLevel)) {
        Serial.println("Dark and the door is not closed.");
        readLightLevel();
        if(door("Open") && readingsExceed(nightLightLevel)) {
            runCount = 0;
            powerMotor("Down");
            closeAttempts++;
            while(bottomSwitch == 1) {
                flashLights(runCount,"Down");
                readDoorSwitches();
                runCount++;
                Serial.println(runCount);
                if(runCount > timeOut) break;
            }
            allOff();
            updateLED();
            if(bottomSwitch == 0) closeAttempts = 0;
        }
        allOff();
    }
    checkAttempts();
}

//Checks to see if the door has tried to open or close more than 3 times without being successful
void checkAttempts() {
   if(openAttempts > 3 || closeAttempts > 3) {
     Serial.println("delay due to attempts");
     openAttempts = 0;
     closeAttempts = 0;
     delay(1000000);
  } 
}

//Turn off motor
void allOff() {
  analogWrite(powerLevelPin, 0);
  digitalWrite(input1, LOW);
  digitalWrite(input2,  LOW);
}

// updateLED() : Checks to see which data to display via flasher (door status or light level) 
// and updates leds as follows: 
// Door : Green == Open, Red == Closed, Purple == in between
// Light level : White == Daytime, Pink = Dawn/Dusk, Blue = Nighttime
void updateLED() {
  if(flasher) {    
      if(currentLightLevel <= morningLightLevel) {
          leds[0] = CRGB::White;
          leds[1] = CRGB::White;
      }
      if(currentLightLevel > morningLightLevel && currentLightLevel < nightLightLevel) {
          leds[0] = CRGB::Yellow;
          leds[1] = CRGB::Yellow;
      }
      if(currentLightLevel >= nightLightLevel) {
          leds[0] = CRGB::CadetBlue;
          leds[1] = CRGB::CadetBlue;
      }
   } else {
      if(bottomSwitch == 0 && topSwitch == 1) {
          leds[0] = CRGB::Red;
          leds[1] = CRGB::Red;
      }
      if(bottomSwitch == 1 && topSwitch == 0) {
          leds[0] = CRGB::Green;
          leds[1] = CRGB::Green;
      }
      if(bottomSwitch == 1 && topSwitch == 1) {
          leds[0] = CRGB::Purple;
          leds[1] = CRGB::Purple;
      }       
  }
  FastLED.show(); 
}

//Checks if the door is in given state (open or closed), according to the switches. Returns true or false. 
bool door(String state) {
   if(bottomSwitch == 1 && topSwitch == 1) return true;
   if (state == "Open" && topSwitch == 0) return true;
   if (state == "Closed" && bottomSwitch == 0) return true;
   return false;
}

//Given run count, flashes LEDs to indicate motor is on.
void flashLights(int runCount, String dir) {
    long colorOdd = 0xFF9999;
    long colorEven = 0xFF0000;
    if(dir = "Up") {
      colorOdd = 0x008000;
      colorEven = 0x90EE90;
    }
    if(runCount % 2) {
      leds[0] = colorEven;
      leds[1] = colorOdd;
    } else {
      leds[0] = colorOdd;
      leds[1] = colorEven;
    }
  FastLED.show(); 
}

//Given direction, turns motor on by setting corresponding pin output to HIGH
void powerMotor(String dir) {
    int powerUp = LOW;
    int powerDown = LOW;
    if(dir == "Up") powerUp = HIGH;
    if(dir == "Down") powerDown = HIGH;  
    digitalWrite(input1, powerUp);
    digitalWrite(input2, powerDown);
    analogWrite(powerLevelPin, powerLevel);
    Serial.println("motor on");
}

//Checks the state of the top and bottom switches. 
void readDoorSwitches() {
    topSwitch = digitalRead(topSwitchPin);
    bottomSwitch = digitalRead(bottomSwitchPin);
    Serial.println("bottomSwitch : topSwitch");
    Serial.println(bottomSwitch);
    Serial.println(topSwitch);
}

bool readingsExceed(int lightLevel) {
   if(lightLevel == nightLightLevel && currentLightLevel >= lightLevel && lightLevel15secAgo >= lightLevel && lightLevel30secAgo >= lightLevel) return true;
   if(lightLevel == morningLightLevel && currentLightLevel <= lightLevel && lightLevel15secAgo <= lightLevel && lightLevel30secAgo <= lightLevel) return true;
   return false;
}



