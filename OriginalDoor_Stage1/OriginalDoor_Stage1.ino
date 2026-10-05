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
 *
 * Black box: the Uno also keeps a log of its recent restarts and motor runs in
 * its EEPROM memory, which survives power cuts. It prints the log to the Serial
 * Monitor every time it starts up, or when you type L and press Send.
 */

#include <EEPROM.h>

// ===== SETTINGS =====

// Light levels. The sensor reads 0-1023, and the number goes DOWN as it gets
// BRIGHTER. To pick new values, open the Serial Monitor (9600 baud) at dawn or
// dusk and note the reading when you'd want the door to move.
//   Past "open" values:  300, 420 (6/21/23), 400 (7/2), 600 (10/19), 700
//   Past "close" values: 600, 900 (6/21/23), 700 (7/2), 820 (10/19), 880
const int BRIGHT_ENOUGH_TO_OPEN = 700;  // open when readings are at or below this
const int DARK_ENOUGH_TO_CLOSE = 880;   // close when readings are at or above this

// The light must be past a level for this many readings in a row. Then the
// door waits one more reading and checks the light and door again before it
// starts the motor.
const int READINGS_IN_A_ROW = 3;
const unsigned long READING_INTERVAL_MS = 5000;  // time between readings (5 seconds)

const int MOTOR_POWER = 50;                    // motor speed, 0-255
const unsigned long MOTOR_TIMEOUT_MS = 17000;  // stop if the switch isn't reached in 17 seconds

// A limit switch must read closed for this long before the motor stops. This
// ignores brief blips of electrical noise from the motor on the switch wires.
const unsigned long SWITCH_CONFIRM_MS = 50;

// When closing, keep the motor running this long after the bottom switch is
// reached, so the door settles fully shut. Adjust it in small steps (50-100):
// too much lets out extra cord, which can start winding onto the spool backwards.
const unsigned long EXTRA_CLOSE_MS = 200;

// While the motor runs, print its progress to the Serial Monitor this often.
const unsigned long MOTOR_PRINT_INTERVAL_MS = 250;

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
  startBlackBox();
  Serial.print("Coop door starting. Opens at ");
  Serial.print(BRIGHT_ENOUGH_TO_OPEN);
  Serial.print(" or lower, closes at ");
  Serial.print(DARK_ENOUGH_TO_CLOSE);
  Serial.println(" or higher.");
}

void loop() {
  delay(READING_INTERVAL_MS);
  printBlackBoxIfAsked();
  readLightSensor();

  // Before moving, wait one more reading and check again, so the door only
  // moves if the light and door position still agree.
  if (shouldOpen()) {
    Serial.println("Bright and the door isn't open. Checking again before opening.");
    delay(READING_INTERVAL_MS);
    readLightSensor();
    if (shouldOpen()) openDoor();
  } else if (shouldClose()) {
    Serial.println("Dark and the door isn't closed. Checking again before closing.");
    delay(READING_INTERVAL_MS);
    readLightSensor();
    if (shouldClose()) closeDoor();
  }
}

bool shouldOpen() {
  return brightReadingsInARow >= READINGS_IN_A_ROW && !isDoorFullyOpen();
}

bool shouldClose() {
  return darkReadingsInARow >= READINGS_IN_A_ROW && !isDoorFullyClosed();
}

// ===== LIGHT SENSOR =====

void readLightSensor() {
  analogRead(LIGHT_SENSOR_PIN);  // throw away one reading: it can be off right after a supply measurement
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
  if (runMotorUntilSwitch(MOTOR_UP, TOP_SWITCH_PIN, 0)) {
    Serial.println("Door opened");
    failedOpensInARow = 0;
  } else {
    Serial.println("Door didn't open: top switch not reached in time");
    failedOpensInARow++;
  }
  pauseIfTooManyFailures();
}

void closeDoor() {
  Serial.println("Closing door");
  if (runMotorUntilSwitch(closeDirection, BOTTOM_SWITCH_PIN, EXTRA_CLOSE_MS)) {
    Serial.println("Door closed");
    failedClosesInARow = 0;
    closeDirection = MOTOR_DOWN;
  } else {
    Serial.println("Door didn't close: bottom switch not reached in time");
    failedClosesInARow++;
    closeDirection = (closeDirection == MOTOR_DOWN) ? MOTOR_UP : MOTOR_DOWN;
  }
  pauseIfTooManyFailures();
}

void pauseIfTooManyFailures() {
  if (failedOpensInARow < FAILED_MOVES_BEFORE_PAUSE && failedClosesInARow < FAILED_MOVES_BEFORE_PAUSE) return;

  Serial.print("Too many failed moves in a row. Pausing for ");
  Serial.print(FAILURE_PAUSE_MS / 60000);
  Serial.println(" minutes.");
  failedOpensInARow = 0;
  failedClosesInARow = 0;
  delay(FAILURE_PAUSE_MS);
}

// ===== MOTOR AND SWITCHES =====

// Runs the motor until the switch has stayed closed for SWITCH_CONFIRM_MS, then
// keeps it running for extraRunMs more. Gives up after MOTOR_TIMEOUT_MS.
// Returns true if the door reached the switch.
bool runMotorUntilSwitch(int direction, int switchPin, unsigned long extraRunMs) {
  logRunStarted(direction, switchPin == BOTTOM_SWITCH_PIN);
  startMotor(direction);
  unsigned long startMs = millis();
  unsigned long lastPrintMs = startMs;
  unsigned long closedSinceMs = 0;
  bool switchClosed = false;
  unsigned long blips = 0;  // times the switch closed briefly and opened again
  unsigned int lowestSupplyMv = 65535;

  while (true) {
    unsigned int supplyMv = readSupplyMillivolts();
    if (supplyMv < lowestSupplyMv) lowestSupplyMv = supplyMv;
    unsigned long nowMs = millis();

    if (isSwitchClosed(switchPin)) {
      if (!switchClosed) {
        switchClosed = true;
        closedSinceMs = nowMs;
      }
      if (nowMs - closedSinceMs >= SWITCH_CONFIRM_MS) {
        if (extraRunMs > 0) {
          Serial.print("  switch reached, running ");
          Serial.print(extraRunMs);
          Serial.println(" ms more");
          delay(extraRunMs);
        }
        stopMotor();
        unsigned long ranMs = millis() - startMs;
        printMotorStopped("switch reached", ranMs, blips, lowestSupplyMv);
        logRunEnded(false, ranMs, lowestSupplyMv, blips);
        return true;
      }
    } else if (switchClosed) {
      switchClosed = false;
      blips++;
    }

    if (nowMs - startMs >= MOTOR_TIMEOUT_MS) {
      stopMotor();
      printMotorStopped("timed out", nowMs - startMs, blips, lowestSupplyMv);
      logRunEnded(true, nowMs - startMs, lowestSupplyMv, blips);
      return false;
    }

    if (nowMs - lastPrintMs >= MOTOR_PRINT_INTERVAL_MS) {
      lastPrintMs = nowMs;
      printMotorRunning(nowMs - startMs, blips);
    }
  }
}

void startMotor(int direction) {
  digitalWrite(MOTOR_IN1_PIN, direction == MOTOR_UP ? HIGH : LOW);
  digitalWrite(MOTOR_IN2_PIN, direction == MOTOR_DOWN ? HIGH : LOW);
  analogWrite(MOTOR_PWM_PIN, MOTOR_POWER);
  Serial.println(direction == MOTOR_UP ? "  motor on, running up" : "  motor on, running down");
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

// Measures the Uno's own supply voltage in millivolts, using the chip's built-in
// 1.1 V reference. It's only roughly accurate (about 10%), but dips show up clearly.
unsigned int readSupplyMillivolts() {
  const byte MEASURE_REFERENCE = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  if (ADMUX != MEASURE_REFERENCE) {
    ADMUX = MEASURE_REFERENCE;
    delay(2);  // the reference needs a moment to settle after switching
  }
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {}
  unsigned int reading = ADC;
  if (reading == 0) return 0;
  return 1125300UL / reading;
}

// ===== SERIAL MONITOR =====

// One line per reading, for example: "Light 652 | bright 2, dark 0 of 3 | door closed"
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

// While the motor runs, for example: "  motor running 1250 ms | at top: no | at bottom: no"
void printMotorRunning(unsigned long elapsedMs, unsigned long blips) {
  Serial.print("  motor running ");
  Serial.print(elapsedMs);
  Serial.print(" ms | at top: ");
  Serial.print(isDoorFullyOpen() ? "yes" : "no");
  Serial.print(" | at bottom: ");
  Serial.print(isDoorFullyClosed() ? "yes" : "no");
  printBlips(blips);
  Serial.println();
}

// For example: "  motor stopped after 9850 ms (switch reached) | lowest supply 4.12 V"
void printMotorStopped(const char* reason, unsigned long elapsedMs, unsigned long blips, unsigned int lowestSupplyMv) {
  Serial.print("  motor stopped after ");
  Serial.print(elapsedMs);
  Serial.print(" ms (");
  Serial.print(reason);
  Serial.print(") | lowest supply ");
  printVolts(lowestSupplyMv);
  printBlips(blips);
  Serial.println();
}

// Switch blips are short bursts of electrical noise that were ignored.
void printBlips(unsigned long blips) {
  if (blips == 0) return;
  Serial.print(" | switch blips ignored: ");
  Serial.print(blips);
}

// Prints millivolts as volts, for example 4123 as "4.12 V".
void printVolts(unsigned int millivolts) {
  Serial.print(millivolts / 1000);
  Serial.print('.');
  unsigned int hundredths = (millivolts % 1000) / 10;
  if (hundredths < 10) Serial.print('0');
  Serial.print(hundredths);
  Serial.print(" V");
}

// ===== BLACK BOX LOG =====
// The last LOG_SLOTS events are kept in EEPROM, oldest overwritten first. Each
// event takes LOG_RECORD_SIZE bytes:
//   0-1 sequence number (0xFFFF = empty slot)   2 event type   3 flags
//   4-5, 6-7, 8-9 three values (meaning depends on the type)   10-11 seconds since restart

const byte LOG_MARKER[4] = {'C', 'D', 'B', '1'};  // bytes 0-3: says the log area is ours
const int LOG_START = 4;
const int LOG_RECORD_SIZE = 12;
const int LOG_SLOTS = (E2END + 1 - LOG_START) / LOG_RECORD_SIZE;
const unsigned int LOG_EMPTY = 0xFFFF;

// Event types
const byte LOG_RESTART = 1;      // values: restart number, supply mV
const byte LOG_RUN_STARTED = 2;  // values: supply mV, light reading
const byte LOG_RUN_ENDED = 3;    // values: ms the motor ran, lowest supply mV, switch blips

// Flag bits
const byte FLAG_CLOSING = 1;     // run started: closing the door (otherwise opening)
const byte FLAG_MOTOR_UP = 2;    // run started: motor ran up (otherwise down)
const byte FLAG_TIMED_OUT = 4;   // run ended: timed out (otherwise the switch was reached)
const byte FLAG_AT_TOP = 8;      // top switch read closed
const byte FLAG_AT_BOTTOM = 16;  // bottom switch read closed

unsigned int restartNumber = 0;
int logNextSlot = 0;
unsigned int logNextSeq = 0;

// Called once at startup: prints the log, then records this restart.
void startBlackBox() {
  if (!logMarkerPresent()) clearLog();

  int newest = findNewestLogSlot();
  if (newest >= 0) {
    logNextSlot = (newest + 1) % LOG_SLOTS;
    logNextSeq = nextLogSeq(logSeqAt(newest));
    restartNumber = lastRestartNumber(newest);
  }
  printBlackBox();

  restartNumber++;
  writeLogEvent(LOG_RESTART, 0, restartNumber, readSupplyMillivolts(), 0);
}

void printBlackBoxIfAsked() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'L' || c == 'l') printBlackBox();
  }
}

void logRunStarted(int direction, bool closing) {
  byte flags = switchFlags();
  if (closing) flags |= FLAG_CLOSING;
  if (direction == MOTOR_UP) flags |= FLAG_MOTOR_UP;
  writeLogEvent(LOG_RUN_STARTED, flags, readSupplyMillivolts(), lightReading, 0);
}

void logRunEnded(bool timedOut, unsigned long ranMs, unsigned int lowestSupplyMv, unsigned long blips) {
  byte flags = switchFlags();
  if (timedOut) flags |= FLAG_TIMED_OUT;
  writeLogEvent(LOG_RUN_ENDED, flags, capAt65535(ranMs), lowestSupplyMv, capAt65535(blips));
}

byte switchFlags() {
  byte flags = 0;
  if (isDoorFullyOpen()) flags |= FLAG_AT_TOP;
  if (isDoorFullyClosed()) flags |= FLAG_AT_BOTTOM;
  return flags;
}

unsigned int capAt65535(unsigned long value) {
  return value > 65535UL ? 65535 : value;
}

void writeLogEvent(byte type, byte flags, unsigned int a, unsigned int b, unsigned int c) {
  int address = LOG_START + logNextSlot * LOG_RECORD_SIZE;
  // Mark the slot empty first, so a power cut mid-write can't leave a half-written event.
  writeLogWord(address, LOG_EMPTY);
  writeLogByte(address + 2, type);
  writeLogByte(address + 3, flags);
  writeLogWord(address + 4, a);
  writeLogWord(address + 6, b);
  writeLogWord(address + 8, c);
  writeLogWord(address + 10, capAt65535(millis() / 1000));
  writeLogWord(address, logNextSeq);

  logNextSlot = (logNextSlot + 1) % LOG_SLOTS;
  logNextSeq = nextLogSeq(logNextSeq);
}

void printBlackBox() {
  int newest = findNewestLogSlot();
  int count = countLogEvents(newest);
  Serial.println();
  Serial.print(F("=== Black box: last "));
  Serial.print(count);
  Serial.println(F(" events, oldest first ==="));
  for (int i = count - 1; i >= 0; i--) {
    printLogEvent((newest - i + LOG_SLOTS) % LOG_SLOTS);
  }
  Serial.println(F("=== End of black box ==="));
  Serial.println();
}

void printLogEvent(int slot) {
  int address = LOG_START + slot * LOG_RECORD_SIZE;
  byte type = EEPROM.read(address + 2);
  byte flags = EEPROM.read(address + 3);
  unsigned int a = readLogWord(address + 4);
  unsigned int b = readLogWord(address + 6);
  unsigned int c = readLogWord(address + 8);
  unsigned int seconds = readLogWord(address + 10);

  if (type == LOG_RESTART) {
    Serial.print(F("RESTART #"));
    Serial.print(a);
    Serial.print(F(" | supply "));
    printVolts(b);
    Serial.println();
    return;
  }

  Serial.print(F("  "));
  Serial.print(seconds);
  Serial.print(F(" s after restart: "));
  if (type == LOG_RUN_STARTED) {
    Serial.print((flags & FLAG_CLOSING) ? F("CLOSE started, motor ") : F("OPEN started, motor "));
    Serial.print((flags & FLAG_MOTOR_UP) ? F("up") : F("down"));
    Serial.print(F(" | supply "));
    printVolts(a);
    Serial.print(F(" | light "));
    Serial.print(b);
  } else if (type == LOG_RUN_ENDED) {
    Serial.print(F("stopped after "));
    Serial.print(a);
    Serial.print((flags & FLAG_TIMED_OUT) ? F(" ms (timed out)") : F(" ms (switch reached)"));
    Serial.print(F(" | lowest supply "));
    printVolts(b);
    Serial.print(F(" | switch blips ignored: "));
    Serial.print(c);
  } else {
    Serial.print(F("unknown event"));
  }
  Serial.print((flags & FLAG_AT_TOP) ? F(" | at top: yes") : F(" | at top: no"));
  Serial.println((flags & FLAG_AT_BOTTOM) ? F(" | at bottom: yes") : F(" | at bottom: no"));
}

// The newest event is the one whose next slot doesn't continue the sequence.
// Returns -1 if the log is empty.
int findNewestLogSlot() {
  for (int slot = 0; slot < LOG_SLOTS; slot++) {
    unsigned int seq = logSeqAt(slot);
    if (seq == LOG_EMPTY) continue;
    if (logSeqAt((slot + 1) % LOG_SLOTS) != nextLogSeq(seq)) return slot;
  }
  return -1;
}

// Counts the unbroken run of events leading up to the newest one.
int countLogEvents(int newest) {
  if (newest < 0) return 0;
  int count = 1;
  int slot = newest;
  while (count < LOG_SLOTS) {
    int previous = (slot + LOG_SLOTS - 1) % LOG_SLOTS;
    unsigned int previousSeq = logSeqAt(previous);
    if (previousSeq == LOG_EMPTY || nextLogSeq(previousSeq) != logSeqAt(slot)) break;
    slot = previous;
    count++;
  }
  return count;
}

// The restart number of the most recent restart in the log (0 if none).
unsigned int lastRestartNumber(int newest) {
  int count = countLogEvents(newest);
  for (int i = 0; i < count; i++) {
    int address = LOG_START + ((newest - i + LOG_SLOTS) % LOG_SLOTS) * LOG_RECORD_SIZE;
    if (EEPROM.read(address + 2) == LOG_RESTART) return readLogWord(address + 4);
  }
  return 0;
}

unsigned int nextLogSeq(unsigned int seq) {
  return seq >= 0xFFFE ? 0 : seq + 1;  // skips 0xFFFF, which means "empty"
}

unsigned int logSeqAt(int slot) {
  return readLogWord(LOG_START + slot * LOG_RECORD_SIZE);
}

bool logMarkerPresent() {
  for (int i = 0; i < 4; i++) {
    if (EEPROM.read(i) != LOG_MARKER[i]) return false;
  }
  return true;
}

void clearLog() {
  for (int address = LOG_START; address <= E2END; address++) writeLogByte(address, 0xFF);
  for (int i = 0; i < 4; i++) writeLogByte(i, LOG_MARKER[i]);
}

unsigned int readLogWord(int address) {
  return EEPROM.read(address) | ((unsigned int)EEPROM.read(address + 1) << 8);
}

void writeLogWord(int address, unsigned int value) {
  writeLogByte(address, value & 0xFF);
  writeLogByte(address + 1, value >> 8);
}

// Only writes bytes that change, to spare the EEPROM (each byte lasts ~100,000 writes).
void writeLogByte(int address, byte value) {
  if (EEPROM.read(address) != value) EEPROM.write(address, value);
}
