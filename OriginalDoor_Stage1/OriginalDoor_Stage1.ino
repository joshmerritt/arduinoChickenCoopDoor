/*
 * Chicken Coop Door: original model (one light sensor)
 *
 * Opens the coop door when it gets light in the morning and closes it when it
 * gets dark. A photoresistor on A0 measures the light, and two magnetic limit
 * switches tell when the door is fully open (top) or shut (bottom). The motor
 * runs through the red XY-15AS driver (PWM, IN1, IN2).
 *
 * Same wiring and behavior as coopDoor2024v1.ino, reorganized so the settings
 * are all in one place. To change when the door opens or closes, edit the
 * light levels below and upload.
 */

// ===== SETTINGS =====

// Light levels. The sensor reads 0-1023, and the number goes DOWN as it gets
// BRIGHTER. To pick new values, open the Serial Monitor (9600 baud) at dawn or
// dusk and note the reading when you'd want the door to move.
//   Past "open" values:  300, 420 (6/21/23), 400 (7/2), 600 (10/19), 700
//   Past "close" values: 600, 900 (6/21/23), 700 (7/2), 820 (10/19), 880
const int BRIGHT_ENOUGH_TO_OPEN = 700;  // open when readings are at or below this
const int DARK_ENOUGH_TO_CLOSE = 880;   // close when readings are at or above this

const int READINGS_IN_A_ROW = 4;                 // readings that must agree before the door moves
const unsigned long READING_INTERVAL_MS = 5000;  // time between readings (5 seconds)

const int MOTOR_POWER = 50;                    // motor speed, 0-255
const unsigned long MOTOR_TIMEOUT_MS = 17000;  // stop if the switch isn't reached in 17 seconds

// After this many failed opens (or closes) in a row, wait before trying again
// so the motor doesn't burn out.
const int FAILED_MOVES_BEFORE_PAUSE = 4;
const unsigned long FAILURE_PAUSE_MS = 17UL * 60 * 1000;  // 17 minutes

// ===== PINS =====
const int MOTOR_PWM_PIN = 9;       // driver PWM: motor speed
const int MOTOR_IN1_PIN = 6;       // driver IN1: HIGH runs the motor up
const int MOTOR_IN2_PIN = 7;       // driver IN2: HIGH runs the motor down
const int BOTTOM_SWITCH_PIN = 12;  // reads LOW when the door is shut
const int TOP_SWITCH_PIN = 13;     // reads LOW when the door is fully open
const int LIGHT_SENSOR_PIN = A0;

// ===== STATE =====
// Motor directions. These are plain numbers rather than an enum because older
// Arduino IDEs can't compile functions that take a type defined in the sketch.
const int MOTOR_UP = 1;
const int MOTOR_DOWN = 2;

int lightReading = 0;
int brightReadingsInARow = 0;
int darkReadingsInARow = 0;
int failedOpensInARow = 0;
int failedClosesInARow = 0;

// When a close fails, the next one runs the motor the other way. That frees
// the door if the cord has wound onto the spool backwards (so "down" lifts it).
int closeDirection = MOTOR_DOWN;

void setup() {
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  stopMotor();
  pinMode(BOTTOM_SWITCH_PIN, INPUT_PULLUP);
  pinMode(TOP_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LIGHT_SENSOR_PIN, INPUT_PULLUP);  // the pull-up and photoresistor form a voltage divider

  Serial.begin(9600);
  Serial.print("Coop door starting. Opens at ");
  Serial.print(BRIGHT_ENOUGH_TO_OPEN);
  Serial.print(" or lower, closes at ");
  Serial.print(DARK_ENOUGH_TO_CLOSE);
  Serial.println(" or higher.");
}

void loop() {
  delay(READING_INTERVAL_MS);
  readLightSensor();

  if (brightReadingsInARow >= READINGS_IN_A_ROW && !isDoorFullyOpen()) {
    openDoor();
  } else if (darkReadingsInARow >= READINGS_IN_A_ROW && !isDoorFullyClosed()) {
    closeDoor();
  }
}

// ===== LIGHT SENSOR =====

void readLightSensor() {
  lightReading = analogRead(LIGHT_SENSOR_PIN);
  brightReadingsInARow = countInARow(brightReadingsInARow, lightReading <= BRIGHT_ENOUGH_TO_OPEN);
  darkReadingsInARow = countInARow(darkReadingsInARow, lightReading >= DARK_ENOUGH_TO_CLOSE);
  printStatus();
}

// Adds one to the count if this reading qualifies; otherwise starts over.
int countInARow(int count, bool readingQualifies) {
  if (!readingQualifies) return 0;
  if (count < READINGS_IN_A_ROW) return count + 1;
  return count;
}

// ===== DOOR =====

void openDoor() {
  Serial.println("Opening door");
  if (runMotorUntilSwitch(MOTOR_UP, TOP_SWITCH_PIN)) {
    Serial.println("Door opened");
    failedOpensInARow = 0;
  } else {
    Serial.println("Door didn't open: top switch not reached in time");
    failedOpensInARow++;
  }
  afterMoveAttempt();
}

void closeDoor() {
  Serial.println("Closing door");
  if (runMotorUntilSwitch(closeDirection, BOTTOM_SWITCH_PIN)) {
    Serial.println("Door closed");
    failedClosesInARow = 0;
    closeDirection = MOTOR_DOWN;
  } else {
    Serial.println("Door didn't close: bottom switch not reached in time");
    failedClosesInARow++;
    closeDirection = (closeDirection == MOTOR_DOWN) ? MOTOR_UP : MOTOR_DOWN;
  }
  afterMoveAttempt();
}

void afterMoveAttempt() {
  // Count fresh readings before the next move, so a failed move isn't retried
  // until READINGS_IN_A_ROW new readings agree.
  brightReadingsInARow = 0;
  darkReadingsInARow = 0;

  if (failedOpensInARow >= FAILED_MOVES_BEFORE_PAUSE || failedClosesInARow >= FAILED_MOVES_BEFORE_PAUSE) {
    Serial.print("Too many failed moves in a row. Pausing for ");
    Serial.print(FAILURE_PAUSE_MS / 60000);
    Serial.println(" minutes.");
    failedOpensInARow = 0;
    failedClosesInARow = 0;
    delay(FAILURE_PAUSE_MS);
  }
}

// ===== MOTOR AND SWITCHES =====

// Runs the motor until the switch closes, or gives up after MOTOR_TIMEOUT_MS.
// Returns true if the door reached the switch.
bool runMotorUntilSwitch(int direction, int switchPin) {
  startMotor(direction);
  unsigned long startMs = millis();
  while (!isSwitchClosed(switchPin)) {
    if (millis() - startMs >= MOTOR_TIMEOUT_MS) {
      stopMotor();
      return false;
    }
  }
  stopMotor();
  return true;
}

void startMotor(int direction) {
  digitalWrite(MOTOR_IN1_PIN, direction == MOTOR_UP ? HIGH : LOW);
  digitalWrite(MOTOR_IN2_PIN, direction == MOTOR_DOWN ? HIGH : LOW);
  analogWrite(MOTOR_PWM_PIN, MOTOR_POWER);
}

// Both inputs LOW makes the driver brake the motor.
void stopMotor() {
  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, LOW);
}

// A limit switch reads LOW when the door's magnet is next to it.
bool isSwitchClosed(int pin) {
  return digitalRead(pin) == LOW;
}

bool isDoorFullyOpen() {
  return isSwitchClosed(TOP_SWITCH_PIN);
}

bool isDoorFullyClosed() {
  return isSwitchClosed(BOTTOM_SWITCH_PIN);
}

// ===== SERIAL MONITOR =====

// One line per reading, for example: "Light 652 | bright 3, dark 0 of 4 | door closed"
void printStatus() {
  Serial.print("Light ");
  Serial.print(lightReading);
  Serial.print(" | bright ");
  Serial.print(brightReadingsInARow);
  Serial.print(", dark ");
  Serial.print(darkReadingsInARow);
  Serial.print(" of ");
  Serial.print(READINGS_IN_A_ROW);
  Serial.print(" | door ");
  Serial.println(doorPositionText());
}

const char* doorPositionText() {
  if (isDoorFullyOpen() && isDoorFullyClosed()) return "unknown (both switches closed)";
  if (isDoorFullyOpen()) return "open";
  if (isDoorFullyClosed()) return "closed";
  return "partly open";
}
