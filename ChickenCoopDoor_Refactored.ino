/**
 * Automatic Chicken Coop Door Controller
 * 
 * Controls a motorized chicken coop door based on light levels from two sensors:
 * - Morning sensor (Z sensor): Detects when it's bright enough to open
 * - Evening sensor: Detects when it's dark enough to close
 * 
 * Features:
 * - Dual binary light sensors (0 or 1 state)
 * - Top and bottom limit switches for door position
 * - Timeout protection to prevent motor burnout
 * - Retry limit to prevent continuous failed attempts
 * - Debouncing with multiple readings over time
 */

// ===== PIN DEFINITIONS =====
const int MOTOR_POWER_PIN = 9;      // PWM pin for motor speed control
const int MOTOR_DIRECTION_1 = 6;    // Motor direction control 1
const int MOTOR_DIRECTION_2 = 7;    // Motor direction control 2
const int EVENING_SENSOR_PIN = A1;  // Sensor to detect darkness (for closing)
const int MORNING_SENSOR_PIN = A0;  // Sensor to detect brightness (for opening)
const int BOTTOM_SWITCH_PIN = 11;   // Limit switch at bottom (closed position)
const int TOP_SWITCH_PIN = 12;      // Limit switch at top (open position)

// ===== CONFIGURATION =====
const int MOTOR_POWER_LEVEL = 120;         // Motor speed (0-255)
const int MOTOR_TIMEOUT_CYCLES = 1000;      // Max cycles before motor timeout
const int MAX_OPERATION_ATTEMPTS = 3;      // Max attempts before long delay
const int READING_INTERVAL_MS = 15000;      // Delay between light readings (15 seconds)
const long LONG_DELAY_MS = 2000000;     // Delay after repeated failures (~32 minutes)
const int STARTUP_DELAY_MS = 3000;         // Initial startup delay

// ===== STATE VARIABLES =====
// Light sensor readings (0 = dark/no signal, 1 = bright/signal present)
struct SensorReadings {
  int current;
  int reading15secAgo;
  int reading30secAgo;
};

SensorReadings morningSensor = {1, 1, 1};
SensorReadings eveningSensor = {0, 0, 0};

// Door state
bool motorDirectionDown = true;  // Track motor direction for retry logic
int openAttempts = 0;            // Counter for failed open attempts
int closeAttempts = 0;           // Counter for failed close attempts
int motorRunCycles = 0;          // Counter for motor timeout protection

// Switch states (0 = pressed, 1 = not pressed due to pullup)
int bottomSwitchState = 1;
int topSwitchState = 1;

// ===== SETUP =====
void setup() {
  Serial.begin(9600);
  Serial.println("=== Chicken Coop Door Controller Starting ===");
  
  // Configure pins
  pinMode(MOTOR_POWER_PIN, OUTPUT);
  pinMode(MOTOR_DIRECTION_1, OUTPUT);
  pinMode(MOTOR_DIRECTION_2, OUTPUT);
  pinMode(BOTTOM_SWITCH_PIN, INPUT_PULLUP);
  pinMode(TOP_SWITCH_PIN, INPUT_PULLUP);
  pinMode(EVENING_SENSOR_PIN, INPUT_PULLUP);
  pinMode(MORNING_SENSOR_PIN, INPUT_PULLUP);
  
  // Initial state
  delay(STARTUP_DELAY_MS);
  stopMotor();
  updateSwitchStates();
  
  Serial.println("=== Initialization Complete ===");
}

// ===== MAIN LOOP =====
void loop() {
  updateSensorReadings();
  evaluateDoorAction();
}

// ===== SENSOR FUNCTIONS =====
/**
 * Updates all sensor readings with debouncing
 * Stores current, 15 sec ago, and 30 sec ago values
 */
void updateSensorReadings() {
  delay(READING_INTERVAL_MS);
  
  // Shift previous readings
  morningSensor.reading30secAgo = morningSensor.reading15secAgo;
  morningSensor.reading15secAgo = morningSensor.current;
  morningSensor.current = digitalRead(MORNING_SENSOR_PIN);
  
  eveningSensor.reading30secAgo = eveningSensor.reading15secAgo;
  eveningSensor.reading15secAgo = eveningSensor.current;
  eveningSensor.current = digitalRead(EVENING_SENSOR_PIN);
  
  updateSwitchStates();
  printSensorStatus();
}

/**
 * Checks if morning sensor consistently indicates it's bright (all readings = 1)
 */
bool isBrightEnoughToOpen() {
  return (morningSensor.current == 0 && 
          morningSensor.reading15secAgo == 0 && 
          morningSensor.reading30secAgo == 0);
}

/**
 * Checks if evening sensor consistently indicates it's dark (all readings = 1)
 * Note: Adjust logic if your sensor returns 0 for dark instead
 */
bool isDarkEnoughToClose() {
  return (eveningSensor.current == 1 && 
          eveningSensor.reading15secAgo == 1 && 
          eveningSensor.reading30secAgo == 1);
}

// ===== DOOR CONTROL FUNCTIONS =====
/**
 * Main decision logic for door operation
 */
void evaluateDoorAction() {
  printDoorStatus();
  
  // Check if door should open (closed and bright)
  if (isDoorClosed() && isBrightEnoughToOpen()) {
    Serial.println(">> Condition met: Door closed and bright - Opening door");
    
    // Double-check with fresh readings
    updateSensorReadings();
    if (isDoorClosed() && isBrightEnoughToOpen()) {
      openDoor();
    }
  }
  
  // Check if door should close (open and dark)
  if (isDoorOpen() && isDarkEnoughToClose()) {
    Serial.println(">> Condition met: Door open and dark - Closing door");
    
    // Double-check with fresh readings
    updateSensorReadings();
    if (isDoorOpen() && isDarkEnoughToClose()) {
      closeDoor();
    }
  }
  
  checkAttemptLimit();
}

/**
 * Opens the door by running motor upward until top switch is hit or timeout
 */
void openDoor() {
  motorRunCycles = 0;
  openAttempts++;
  
  Serial.println(">> OPENING DOOR");
  runMotor("UP");
  
  // Run until top switch is pressed or timeout
  while (topSwitchState == 1) {
    updateSwitchStates();
    motorRunCycles++;
    
    if (motorRunCycles % 10 == 0) {  // Print every 10 cycles for less spam
      Serial.print("Opening... cycle: ");
      Serial.println(motorRunCycles);
    }
    
    if (motorRunCycles > MOTOR_TIMEOUT_CYCLES) {
      Serial.println(">> TIMEOUT - Stopping motor");
      break;
    }
  }
  
  stopMotor();
  
  // Reset attempts if successful
  if (topSwitchState == 0) {
    Serial.println(">> Door opened successfully");
    openAttempts = 0;
  } else {
    Serial.println(">> Failed to open door completely");
  }
}

/**
 * Closes the door by running motor until bottom switch is hit or timeout
 */
void closeDoor() {
  Serial.println("CLOSING DOOR AFTER DELAY");
  delay(LONG_DELAY_MS);
  
  motorRunCycles = 0;
  closeAttempts++;
  
  Serial.println(">> CLOSING DOOR");
  
  // Use alternating direction if previous attempts failed
  if (motorDirectionDown) {
    runMotor("DOWN");
  } else {
    runMotor("UP");  // Try opposite direction if stuck
  }
  
  // Run until bottom switch is pressed or timeout
  while (bottomSwitchState == 1) {
    updateSwitchStates();
    motorRunCycles++;
    
    if (motorRunCycles % 10 == 0) {  // Print every 10 cycles
      Serial.print("Closing... cycle: ");
      Serial.println(motorRunCycles);
    }
    
    if (motorRunCycles > MOTOR_TIMEOUT_CYCLES) {
      Serial.println(">> TIMEOUT - Stopping motor");
      break;
    }
  }
  
  stopMotor();
  
  // Handle success or failure
  if (bottomSwitchState == 0) {
    Serial.println(">> Door closed successfully");
    closeAttempts = 0;
    motorDirectionDown = true;  // Reset to default direction
  } else {
    Serial.println(">> Failed to close door completely");
    // Toggle direction for next attempt if stuck
    if (bottomSwitchState == 1 || topSwitchState == 0) {
      motorDirectionDown = !motorDirectionDown;
    }
  }
}

// ===== MOTOR CONTROL =====
/**
 * Runs motor in specified direction
 * @param direction "UP" or "DOWN"
 */
void runMotor(String direction) {
  int pin1State = LOW;
  int pin2State = LOW;
  
  if (direction == "UP") {
    pin1State = HIGH;
  } else if (direction == "DOWN") {
    pin2State = HIGH;
  }
  
  digitalWrite(MOTOR_DIRECTION_1, pin1State);
  digitalWrite(MOTOR_DIRECTION_2, pin2State);
  analogWrite(MOTOR_POWER_PIN, MOTOR_POWER_LEVEL);
  
  Serial.print("Motor running: ");
  Serial.print(direction);
  Serial.print(" at power level: ");
  Serial.println(MOTOR_POWER_LEVEL);
}

/**
 * Stops the motor completely
 */
void stopMotor() {
  analogWrite(MOTOR_POWER_PIN, 0);
  digitalWrite(MOTOR_DIRECTION_1, LOW);
  digitalWrite(MOTOR_DIRECTION_2, LOW);
}

// ===== DOOR POSITION DETECTION =====
/**
 * Updates the state of limit switches
 */
void updateSwitchStates() {
  topSwitchState = digitalRead(TOP_SWITCH_PIN);
  bottomSwitchState = digitalRead(BOTTOM_SWITCH_PIN);
}

/**
 * Checks if door is in open position
 */
bool isDoorOpen() {
  // Door is open if top switch is pressed (0) or both switches unpressed
  return (topSwitchState == 0) || 
         (bottomSwitchState == 1 && topSwitchState == 1);
}

/**
 * Checks if door is in closed position
 */
bool isDoorClosed() {
  // Door is closed if bottom switch is pressed (0) or both switches unpressed
  return (bottomSwitchState == 0) || 
         (bottomSwitchState == 1 && topSwitchState == 1);
}

// ===== SAFETY FEATURES =====
/**
 * Checks if too many failed attempts and delays if necessary
 */
void checkAttemptLimit() {
  if (openAttempts > MAX_OPERATION_ATTEMPTS || 
      closeAttempts > MAX_OPERATION_ATTEMPTS) {
    Serial.println(">> ERROR: Too many failed attempts!");
    Serial.println(">> Entering extended delay for safety...");
    
    openAttempts = 0;
    closeAttempts = 0;
    delay(LONG_DELAY_MS);
    
    Serial.println(">> Resuming operation after safety delay");
  }
}

// ===== DEBUG OUTPUT =====
/**
 * Prints current sensor readings
 */
void printSensorStatus() {
  Serial.println("\n--- Sensor Readings ---");
  Serial.print("Morning Sensor (for opening): ");
  Serial.print(morningSensor.current);
  Serial.print(" | 15s ago: ");
  Serial.print(morningSensor.reading15secAgo);
  Serial.print(" | 30s ago: ");
  Serial.println(morningSensor.reading30secAgo);
  
  Serial.print("Evening Sensor (for closing): ");
  Serial.print(eveningSensor.current);
  Serial.print(" | 15s ago: ");
  Serial.print(eveningSensor.reading15secAgo);
  Serial.print(" | 30s ago: ");
  Serial.println(eveningSensor.reading30secAgo);
  
  Serial.print("Switches - Bottom: ");
  Serial.print(bottomSwitchState);
  Serial.print(" | Top: ");
  Serial.println(topSwitchState);
}

/**
 * Prints door and decision status
 */
void printDoorStatus() {
  Serial.println("\n--- Door Status ---");
  Serial.print("Door is closed: ");
  Serial.println(isDoorClosed() ? "YES" : "NO");
  Serial.print("Door is open: ");
  Serial.println(isDoorOpen() ? "YES" : "NO");
  Serial.print("Bright enough to open: ");
  Serial.println(isBrightEnoughToOpen() ? "YES" : "NO");
  Serial.print("Dark enough to close: ");
  Serial.println(isDarkEnoughToClose() ? "YES" : "NO");
  Serial.print("Open attempts: ");
  Serial.print(openAttempts);
  Serial.print(" | Close attempts: ");
  Serial.println(closeAttempts);
}
