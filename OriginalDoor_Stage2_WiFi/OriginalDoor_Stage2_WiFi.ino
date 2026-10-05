/*
 * Chicken Coop Door: original model (one light sensor), stage 2 with WiFi
 * (stage 1, the Uno-only version, is ../OriginalDoor_Stage1)
 *
 * Opens the coop door when it gets light in the morning and closes it when it
 * gets dark. A photoresistor measures the light, and two magnetic limit
 * switches tell when the door is fully open (top) or shut (bottom).
 *
 * Runs on either board:
 *   - Arduino Uno: door control only, with the original wiring.
 *   - ESP8266 (Wemos D1 mini or NodeMCU): door control plus WiFi, with a status
 *     page, Open/Close buttons, and phone notifications (see the wifi tab).
 *
 * Wiring and setup steps are in README.md and UPGRADE_GUIDE.md in this folder.
 */

// ===== SETTINGS =====

// The light sensor reads 0-1023, and the number goes DOWN as it gets BRIGHTER.
// To tune these, watch the readings at dawn and dusk (Serial Monitor at
// 115200 baud, or the status page). The two boards read the same light a bit
// differently, so each has its own levels.
#if defined(ESP8266)
// Starting points converted from the Uno's 700/880 for a 33k pull-up resistor.
const int BRIGHT_ENOUGH_TO_OPEN = 690;  // open when readings are at or below this
const int DARK_ENOUGH_TO_CLOSE = 840;   // close when readings are at or above this
#else
const int BRIGHT_ENOUGH_TO_OPEN = 700;
const int DARK_ENOUGH_TO_CLOSE = 880;
#endif
const int READINGS_IN_A_ROW = 4;        // readings that must agree before the door moves
const unsigned long READING_INTERVAL_MS = 5000;

const int MOTOR_POWER = 50;                    // motor speed, 0-255
const unsigned long MOTOR_TIMEOUT_MS = 17000;  // give up if the switch isn't reached by then

// After this many failed opens (or closes) in a row, stop moving the door for
// a while so the motor doesn't burn out.
const int FAILED_MOVES_BEFORE_PAUSE = 4;
const unsigned long FAILURE_PAUSE_MS = 17UL * 60 * 1000;  // 17 minutes

// After Open or Close is pressed on the status page, the light sensor leaves
// the door alone for this long. Otherwise it would undo the move right away.
const unsigned long MANUAL_PAUSE_MS = 30UL * 60 * 1000;  // 30 minutes

// ===== PINS =====
// Motor driver: SPEED is the PWM (enable) input. INPUT1 HIGH runs the motor up,
// INPUT2 HIGH runs it down. Each limit switch connects its pin to GND when the
// door's magnet is next to it.
#if defined(ESP8266)  // Wemos D1 mini / NodeMCU
const int MOTOR_SPEED_PIN = D1;
const int MOTOR_INPUT1_PIN = D2;
const int MOTOR_INPUT2_PIN = D5;
const int BOTTOM_SWITCH_PIN = D6;
const int TOP_SWITCH_PIN = D7;
#else                 // Arduino Uno
const int MOTOR_SPEED_PIN = 9;
const int MOTOR_INPUT1_PIN = 6;
const int MOTOR_INPUT2_PIN = 7;
const int BOTTOM_SWITCH_PIN = 12;
const int TOP_SWITCH_PIN = 13;
#endif
const int LIGHT_SENSOR_PIN = A0;

// ===== STATE =====
// Named numbers rather than enums because older Arduino IDEs can't compile
// functions that take a type defined in the sketch.
const int MOTOR_UP = 1;            // motor directions
const int MOTOR_DOWN = 2;
const int BY_LIGHT_SENSOR = 1;     // what moved the door
const int FROM_STATUS_PAGE = 2;
const int NO_COMMAND = 0;          // buttons pressed on the status page
const int OPEN_COMMAND = 1;
const int CLOSE_COMMAND = 2;

int lightReading = 0;
int brightReadingsInARow = 0;
int darkReadingsInARow = 0;
unsigned long lastReadingMs = 0;

int failedOpensInARow = 0;
int failedClosesInARow = 0;

// When a close fails, the next one runs the motor the other way. That frees
// the door if the cord has wound onto the spool backwards (so "down" lifts it).
int closeDirection = MOTOR_DOWN;

// While paused, the light sensor doesn't move the door.
bool automationPaused = false;
unsigned long pauseStartMs = 0;
unsigned long pauseLengthMs = 0;
const char* pauseReason = "";

int pendingCommand = NO_COMMAND;  // set by the status page's buttons

String lastEvent;  // most recent event, shown on the status page
unsigned long lastEventMs = 0;

void setup() {
  pinMode(MOTOR_SPEED_PIN, OUTPUT);
  pinMode(MOTOR_INPUT1_PIN, OUTPUT);
  pinMode(MOTOR_INPUT2_PIN, OUTPUT);
  stopMotor();
  pinMode(BOTTOM_SWITCH_PIN, INPUT_PULLUP);
  pinMode(TOP_SWITCH_PIN, INPUT_PULLUP);
#if defined(ESP8266)
  analogWriteRange(255);  // same 0-255 motor speed scale as the Uno
  analogWriteFreq(490);   // same PWM frequency as the Uno's pin 9
#else
  pinMode(LIGHT_SENSOR_PIN, INPUT_PULLUP);  // the pull-up and photoresistor form a voltage divider
#endif

  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Coop door starting"));
  setupWifi();
  reportEvent(String("Coop door started. Door is ") + doorPositionText() + ".", false);
}

void loop() {
  handleWifi();  // serves the status page (ESP8266 only)

  if (pendingCommand != NO_COMMAND) {
    runPendingCommand();
  }

  if (millis() - lastReadingMs >= READING_INTERVAL_MS) {
    lastReadingMs = millis();
    readLightSensor();
    if (!isAutomationPaused()) {
      moveDoorForLight();
    }
  }
}

// ===== LIGHT SENSOR =====

void readLightSensor() {
  lightReading = analogRead(LIGHT_SENSOR_PIN);
  brightReadingsInARow = countInARow(brightReadingsInARow, lightReading <= BRIGHT_ENOUGH_TO_OPEN);
  darkReadingsInARow = countInARow(darkReadingsInARow, lightReading >= DARK_ENOUGH_TO_CLOSE);
  printReading();
}

// Adds one to the count if this reading qualifies; otherwise starts over.
int countInARow(int count, bool readingQualifies) {
  if (!readingQualifies) return 0;
  if (count < READINGS_IN_A_ROW) return count + 1;
  return count;
}

// Opens or closes the door once enough readings in a row agree.
void moveDoorForLight() {
  if (brightReadingsInARow >= READINGS_IN_A_ROW && !isDoorAtTop()) {
    openDoor(BY_LIGHT_SENSOR);
  } else if (darkReadingsInARow >= READINGS_IN_A_ROW && !isDoorAtBottom()) {
    closeDoor(BY_LIGHT_SENSOR);
  }
}

// ===== DOOR =====

void openDoor(int trigger) {
  if (runMotorUntilSwitch(MOTOR_UP, TOP_SWITCH_PIN)) {
    failedOpensInARow = 0;
    reportEvent(trigger == FROM_STATUS_PAGE ? "Door opened from the status page" : "Door opened", false);
  } else {
    failedOpensInARow++;
    reportEvent(String("Door didn't open: top switch not reached in ") + MOTOR_TIMEOUT_MS / 1000 + " s", true);
  }
  afterMoveAttempt();
}

void closeDoor(int trigger) {
  if (runMotorUntilSwitch(closeDirection, BOTTOM_SWITCH_PIN)) {
    failedClosesInARow = 0;
    closeDirection = MOTOR_DOWN;
    reportEvent(trigger == FROM_STATUS_PAGE ? "Door closed from the status page" : "Door closed", false);
  } else {
    failedClosesInARow++;
    closeDirection = (closeDirection == MOTOR_DOWN) ? MOTOR_UP : MOTOR_DOWN;
    reportEvent(String("Door didn't close: bottom switch not reached in ") + MOTOR_TIMEOUT_MS / 1000 + " s", true);
  }
  afterMoveAttempt();
}

void afterMoveAttempt() {
  // Count fresh readings before the next automatic move, so a failed move
  // isn't retried until READINGS_IN_A_ROW new readings agree.
  brightReadingsInARow = 0;
  darkReadingsInARow = 0;

  if (failedOpensInARow >= FAILED_MOVES_BEFORE_PAUSE || failedClosesInARow >= FAILED_MOVES_BEFORE_PAUSE) {
    failedOpensInARow = 0;
    failedClosesInARow = 0;
    pauseAutomation(FAILURE_PAUSE_MS, "too many failed moves");
    reportEvent(String(FAILED_MOVES_BEFORE_PAUSE) + " failed moves in a row. Pausing for " + FAILURE_PAUSE_MS / 60000 + " min.", true);
  }
}

// Runs an Open or Close that was pressed on the status page.
void runPendingCommand() {
  int command = pendingCommand;
  pendingCommand = NO_COMMAND;
  pauseAutomation(MANUAL_PAUSE_MS, "door moved from the status page");
  if (command == OPEN_COMMAND) {
    openDoor(FROM_STATUS_PAGE);
  } else {
    closeDoor(FROM_STATUS_PAGE);
  }
}

void pauseAutomation(unsigned long lengthMs, const char* reason) {
  automationPaused = true;
  pauseStartMs = millis();
  pauseLengthMs = lengthMs;
  pauseReason = reason;
}

// Time left before the light sensor takes over again (0 = not paused).
unsigned long pauseTimeLeftMs() {
  if (!automationPaused) return 0;
  unsigned long elapsedMs = millis() - pauseStartMs;
  if (elapsedMs >= pauseLengthMs) {
    automationPaused = false;
    return 0;
  }
  return pauseLengthMs - elapsedMs;
}

bool isAutomationPaused() {
  return pauseTimeLeftMs() > 0;
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
    delay(5);  // on the ESP8266 this also lets WiFi run, which avoids a watchdog reset
  }
  stopMotor();
  return true;
}

void startMotor(int direction) {
  digitalWrite(MOTOR_INPUT1_PIN, direction == MOTOR_UP ? HIGH : LOW);
  digitalWrite(MOTOR_INPUT2_PIN, direction == MOTOR_DOWN ? HIGH : LOW);
  analogWrite(MOTOR_SPEED_PIN, MOTOR_POWER);
  Serial.println(direction == MOTOR_UP ? F("Motor running up") : F("Motor running down"));
}

void stopMotor() {
  analogWrite(MOTOR_SPEED_PIN, 0);
  digitalWrite(MOTOR_INPUT1_PIN, LOW);
  digitalWrite(MOTOR_INPUT2_PIN, LOW);
}

// A limit switch reads LOW when the door's magnet is next to it.
bool isSwitchClosed(int pin) {
  return digitalRead(pin) == LOW;
}

bool isDoorAtTop() {
  return isSwitchClosed(TOP_SWITCH_PIN);
}

bool isDoorAtBottom() {
  return isSwitchClosed(BOTTOM_SWITCH_PIN);
}

const char* doorPositionText() {
  if (isDoorAtTop() && isDoorAtBottom()) return "unknown (both switches closed)";
  if (isDoorAtTop()) return "open";
  if (isDoorAtBottom()) return "closed";
  return "partly open";
}

// ===== REPORTING =====

// Logs an event. On the ESP8266 it also shows on the status page and is sent
// to your phone.
void reportEvent(const String& message, bool isProblem) {
  Serial.println(message);
  lastEvent = message;
  lastEventMs = millis();
  sendPhoneNotification(message, isProblem);
}

void printReading() {
  Serial.print(F("Light "));
  Serial.print(lightReading);
  Serial.print(F(" | bright "));
  Serial.print(brightReadingsInARow);
  Serial.print(F(", dark "));
  Serial.print(darkReadingsInARow);
  Serial.print(F(" of "));
  Serial.print(READINGS_IN_A_ROW);
  Serial.print(F(" | door "));
  Serial.print(doorPositionText());
  if (isAutomationPaused()) Serial.print(F(" | paused"));
  Serial.println();
}
