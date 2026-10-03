
//#include <FastLED.h>
//#include <Servo.h>

// Define which pins will be used for motor control, switches, LEDs, and the light sensor.
#define powerLevelPin 9
#define input1 6
#define input2 7
#define sensorPin A0
#define bottomSwitchPin 12
#define topSwitchPin 13

// previous morning: 300, 6/21/23 @420, 7/2 @400, 10/19@600
// evening: 600, 6/21/23 @900, 7/2@700, 10/19@820
int BRIGHTNESS = 100;
int morningLightLevel = 600;
int nightLightLevel = 820;
int currentLightLevel = 0;
int lightLevel15secAgo = 0;
int lightLevel30secAgo = 0;
int powerLevel = 50;
int bottomSwitch = 0;
int topSwitch = 0;
int runCount = 0;
int timeOut = 420;
int openAttempts = 0;
int closeAttempts = 0;
int readLightLevelDelay = 5000;
int topSwitchOnCount = 0;
bool motorDown = true;

// Setup (runs once)
void setup() {
  Serial.begin(9600);
  Serial.println("Booting Up . . .");
  delay(3000);
  //Setup pins for LEDs, motor, door switches, and light sensor
  //FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  //FastLED.setBrightness(  BRIGHTNESS );
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
  delay(readLightLevelDelay);
  lightLevel30secAgo = lightLevel15secAgo;
  lightLevel15secAgo = currentLightLevel;
  currentLightLevel = analogRead(sensorPin);
  readDoorSwitches();
  Serial.println("readLightLevel : Current : 15secAgo : 30secAgo");
  Serial.println(currentLightLevel);
  Serial.println(lightLevel15secAgo);
  Serial.println(lightLevel30secAgo);
}

// Checks the light level and door status. If action needed, rechecks light level. 
// Then opens or closes door if past 3 light level readings exceeded the threshold
void checkDoor() {
    Serial.println("Door('closed') function");
    Serial.println(door("Closed"));
    Serial.println("Door('open') function");
    Serial.println(door("Open"));
    Serial.println("readingsExceed morningLightLevel function");
    Serial.println(readingsExceed("morningLightLevel"));
    Serial.println("readingsExceed nightLightLevel function");
    Serial.println(readingsExceed("nightLightLevel"));
    
    if(door("Closed") && readingsExceed("morningLightLevel")) {
        Serial.println("Bright and door not open, recheck");
        readLightLevel();
        if(door("Closed") && readingsExceed("morningLightLevel")) {     
              runCount = 0;
              powerMotor("Up");
              openAttempts++;  
              while(topSwitch == 1) {
                  readDoorSwitches();
                  runCount++;
                  Serial.println(runCount);
                  if(runCount > timeOut) break;
                }
              allOff();
              if(topSwitch == 0) openAttempts = 0;
        }
        allOff();
    }
    if(door("Open") && readingsExceed("nightLightLevel")) {
        Serial.println("Dark and the door is not closed.");
        readLightLevel();
        if(door("Open") && readingsExceed("nightLightLevel")) {
            runCount = 0;
            powerMotor("Down");
            closeAttempts++;
            while(bottomSwitch == 1) {
                readDoorSwitches();
                runCount++;
                if(topSwitch == 0) topSwitchOnCount++;
                Serial.println("topSwitchOnCount :: runCount");
                Serial.println(topSwitchOnCount);
                Serial.println(runCount);
                if(runCount > timeOut) break;
                if(topSwitchOnCount == 25) {
                  Serial.println("switch");
                  if(motorDown) {
                    powerMotor("Up");
                  } else powerMotor("Down");
                  topSwitchOnCount = 0;
                  motorDown = !motorDown; 
                }
            }
            allOff();
            if(bottomSwitch == 0) closeAttempts = 0;
            topSwitchOnCount = 0;
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

//Checks if the door is in given state (open or closed), according to the switches. Returns true or false. 
bool door(String state) {
   if(bottomSwitch == 1 && topSwitch == 1) return true;
   if (state == "Open" && topSwitch == 0) return true;
   if (state == "Closed" && bottomSwitch == 0) return true;
   return false;
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

bool readingsExceed(String lightLevel) {
   if(lightLevel == "nightLightLevel") {
     if(currentLightLevel >= nightLightLevel && lightLevel15secAgo >= nightLightLevel && lightLevel30secAgo >= nightLightLevel) return true;
     return false;
   }
   if(lightLevel == "morningLightLevel"){
      if(currentLightLevel <= morningLightLevel && lightLevel15secAgo <= morningLightLevel && lightLevel30secAgo <= morningLightLevel) return true;
     return false;
   }
   return false;
}
