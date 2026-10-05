# Chicken Coop Door: original model (one light sensor)

A refactored version of `coopDoor2024v1.ino` (on the `history` branch), for the original door's Arduino Uno. The wiring and behavior are the same as before. The code is reorganized so the settings are easy to find and change.

## Changing when the door opens and closes

The two light levels are at the top of `OriginalDoor_Stage1.ino`:

```cpp
const int BRIGHT_ENOUGH_TO_OPEN = 700;  // open when readings are at or below this
const int DARK_ENOUGH_TO_CLOSE = 880;   // close when readings are at or above this
```

**Lower readings mean brighter.**

**Finding good values.**
1. Around dawn or dusk, plug the Uno into your laptop.
2. Open **Tools > Serial Monitor** at **9600 baud**. Every 5 seconds it prints a line like:

   ```text
   Light 652 | bright 3, dark 0 of 4 | door closed
   ```

3. Note the reading at the moment you'd want the door to move.

**Choosing the numbers.** Keep a gap of at least 100 between the two numbers, so a cloudy evening doesn't make the door go back and forth. The comment above the settings lists the values you've used in past seasons.

**Uploading.** Select **Tools > Board > Arduino Uno** and click Upload. When the Uno restarts, the Serial Monitor prints the levels it's using.

The other settings are in the same block:

| Setting | Value | What it controls |
|---|---|---|
| `READINGS_IN_A_ROW` | 4 | How many readings must agree before the door moves |
| `READING_INTERVAL_MS` | 5000 | Time between readings (5 seconds) |
| `MOTOR_POWER` | 50 | Motor speed (0–255) |
| `MOTOR_TIMEOUT_MS` | 17000 | How long the motor runs before giving up if it doesn't reach a switch (17 seconds) |
| `FAILED_MOVES_BEFORE_PAUSE` | 4 | Failed tries in a row before it pauses |
| `FAILURE_PAUSE_MS` | 17 minutes | How long it pauses |

## What it does

- Reads the light sensor every 5 seconds. After 4 bright readings in a row it opens the door; after 4 dark readings in a row it closes it.
- A partly open door is opened if it's bright, or closed if it's dark.
- The motor runs until the door reaches the top or bottom switch, and stops after 17 seconds if it doesn't.
- If a close fails, the next try runs the motor the opposite way. This frees the door if the cord has wound onto the spool backwards.
- After 4 failed opens (or closes) in a row, it waits 17 minutes before trying again.

## Wiring (unchanged)

| Uno pin | Connected to |
|---|---|
| 9 | driver PWM (motor speed) |
| 6 | driver IN1 (HIGH = door up) |
| 7 | driver IN2 (HIGH = door down) |
| 12 | bottom switch (other wire to GND) |
| 13 | top switch (other wire to GND) |
| A0 | light sensor (other leg to GND) |
| Vin / GND | driver 5VO / COM |

## Differences from coopDoor2024v1

The door does the same things. These are the small differences, all checked in a simulation that ran both versions side by side:

- **Motor timeout.** It's now 17 seconds of real time. The old limit was 450 loop passes, and its length depended on how fast the Uno printed to the Serial Monitor. That worked out to about 17.3 seconds.
- **Powering up in daylight.** It waits for 4 real readings (about 20 seconds) before opening. The old code could open after 2, because its empty reading history counted as "bright".
- **Retries.** After a failed move it waits about 20 seconds before trying again, instead of about 10. That gives the motor more rest.
- **Both switches closed at once.** If both switches read closed (a wiring fault), it leaves the motor alone. The old code pulsed the motor every 10 seconds.
- **Pause length.** The pause after repeated failures is 17 minutes. It was 1,000,000 ms, about 16.7 minutes.
- **Serial Monitor.** It prints one easy-to-read line per reading.

## Next: WiFi (stage 2)

A later step will move this door to an ESP8266 (NodeMCU) board. It adds a status page, Open/Close buttons on your phone, and notifications. That version, and a step-by-step upgrade guide, are in [`../OriginalDoor_Stage2_WiFi`](../OriginalDoor_Stage2_WiFi). The door logic stays the same, so carry over the light levels you tune here.
