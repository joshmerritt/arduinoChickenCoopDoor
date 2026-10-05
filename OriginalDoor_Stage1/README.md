# Chicken Coop Door: original model (one light sensor)

A refactored version of `coopDoor2024v1.ino` (on the `history` branch), for the original door's Arduino Uno. The wiring and behavior are the same as before. The code is reorganized so the settings are easy to find and change.

## Changing when the door opens and closes

The two light levels are at the top of `OriginalDoor_Stage1.ino`:

```cpp
const int BRIGHT_ENOUGH_TO_OPEN = 700;  // open when readings are at or below this
const int DARK_ENOUGH_TO_CLOSE = 880;   // close when readings are at or above this
```

**Lower readings mean brighter.**

To find good values:

1. Around dawn or dusk, plug the Uno into your laptop.
2. Open **Tools > Serial Monitor** at **9600 baud**. Every 5 seconds it prints a line like:

   ```text
   Light 652 | bright 2, dark 0 of 3 | door closed
   ```

3. Note the reading at the moment you'd want the door to move.

Keep a gap of at least 100 between the two numbers, so a cloudy evening doesn't make the door go back and forth. The comment above the settings lists the values you've used in past seasons.

To upload, select **Tools > Board > Arduino Uno** and click Upload. When the Uno restarts, it prints the levels it's using.

The other settings are in the same block:

| Setting | Value | What it controls |
|---|---|---|
| `READINGS_IN_A_ROW` | 3 | Readings in a row that must be past a level. After that, it waits one more reading and checks again before the motor starts. |
| `READING_INTERVAL_MS` | 5000 | Time between readings (5 seconds) |
| `MOTOR_POWER` | 50 | Motor speed (0–255) |
| `MOTOR_TIMEOUT_MS` | 17000 | How long the motor runs before giving up if it doesn't reach a switch (17 seconds) |
| `SWITCH_CONFIRM_MS` | 50 | How long a limit switch must read closed before the motor stops. Shorter blips of electrical noise are ignored. |
| `MOTOR_PRINT_INTERVAL_MS` | 250 | How often the Serial Monitor shows the motor's progress while it runs |
| `FAILED_MOVES_BEFORE_PAUSE` | 4 | Failed tries in a row before it pauses |
| `FAILURE_PAUSE_MS` | 17 minutes | How long it pauses |

## What it does

- **Reading the light:** it reads the light sensor every 5 seconds.
- **Opening and closing:** after 3 bright readings in a row, it waits 5 more seconds and checks the light and the door again. If both still agree, it opens the door. Closing works the same way with dark readings.
- **Partly open door:** it's opened if it's bright, or closed if it's dark.
- **Stopping the motor:** the motor runs until the door reaches the top or bottom switch. If the door doesn't get there within 17 seconds, it stops anyway. A switch has to read closed for 50 ms in a row, so a brief burst of electrical noise from the motor can't stop it early.
- **Failed close:** if a close fails, the next try runs the motor the opposite way. This frees the door if the cord has wound onto the spool backwards.
- **Repeated failures:** after 4 failed opens (or closes) in a row, it waits 17 minutes before trying again. Between failed tries there's a break of about 10 seconds, the same as before.

## Serial Monitor

At 9600 baud you'll see one line per light reading. Whenever the door moves, you'll also see:

```text
Dark and the door isn't closed. Checking again before closing.
Light 905 | bright 0, dark 3 of 3 | door open
Closing door
  motor on, running down
  motor running 250 ms | at top: yes | at bottom: no
  motor running 500 ms | at top: no | at bottom: no
  ...
  motor stopped after 9850 ms (switch reached)
Door closed
```

If a run shows **switch blips ignored**, electrical noise from the motor is reaching the switch wires. It's harmless now, but it's worth knowing about. If the line **Coop door starting** shows up right after the motor starts, the Uno is restarting when the motor turns on. That points to a power problem rather than the code.

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

The door does the same things. These small differences were all checked in a simulation that ran both versions side by side:

- **Motor timeout:** it's now 17 seconds of real time. The old limit was 450 loop passes, and its length depended on how fast the Uno printed to the Serial Monitor. That worked out to about 17.3 seconds.
- **Switch noise:** a switch must read closed for 50 ms before the motor stops. The old code printed so much while the motor ran that it only checked the switches about every 38 ms. That accidentally made it ignore most noise; this setting does the same job on purpose.
- **Powering up in daylight:** it waits for 3 real readings plus the extra check before opening. The old code's empty reading history counted as "bright", so it could open sooner after power-up.
- **Both switches closed at once:** if both switches read closed (a wiring fault), it leaves the motor alone. The old code pulsed the motor every 10 seconds.
- **Failure pause:** the pause after repeated failures is 17 minutes. It was 1,000,000 ms, about 16.7 minutes.
- **Serial Monitor output:** it's easier to read: one line per reading, and the motor's progress while it runs.

## Next: WiFi (stage 2)

A later step will move this door to an ESP8266 (NodeMCU) board. It adds a status page, Open/Close buttons on your phone, and notifications. That version, and a step-by-step upgrade guide, are in [`../OriginalDoor_Stage2_WiFi`](../OriginalDoor_Stage2_WiFi). The door logic stays the same, so carry over the light levels you tune here.
