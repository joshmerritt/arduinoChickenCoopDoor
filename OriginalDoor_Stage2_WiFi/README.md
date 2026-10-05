# Chicken Coop Door, stage 2: original model with WiFi

> **Not in use yet.** Stage 1, the sketch running on the door today, is [`../OriginalDoor_Stage1`](../OriginalDoor_Stage1). This folder is the next step: the same door logic on an ESP8266 board, plus WiFi.

This sketch runs on two boards:

- **ESP8266** (NodeMCU or Wemos D1 mini): door control plus WiFi, with a status page, Open/Close buttons and phone notifications.
- **Arduino Uno**: door control only, with the same wiring as stage 1.

**To move the door from the Uno to the NodeMCU,** follow [UPGRADE_GUIDE.md](UPGRADE_GUIDE.md) step by step.

## What it does

- **Light:** it reads the light sensor every 5 seconds. After 4 bright readings in a row it opens the door; after 4 dark readings in a row it closes it.
- **Switches:** it runs the motor until the door reaches the top or bottom switch. If the door doesn't get there within 17 seconds, it stops the motor.
- **Failed close:** if a close fails, the next try runs the motor the opposite way.
- **Repeated failures:** after 4 failed opens (or closes) in a row, it stops trying for 17 minutes.
- **Phone buttons (ESP8266 only):** pressing Open or Close on the status page pauses the light sensor for 30 minutes, so it doesn't undo the move right away.

These numbers are settings at the top of `OriginalDoor_Stage2_WiFi.ino`.

## Wiring

The motor driver is the red XY-15AS. Its orange signal plug is labeled 5VO, PWM, IN1, IN2, COM.

| Connection | Arduino Uno | ESP8266 (NodeMCU / D1 mini) |
|---|---|---|
| Driver **PWM** (motor speed) | 9 | D1 |
| Driver **IN1** (on = door up) | 6 | D2 |
| Driver **IN2** (on = door down) | 7 | D5 |
| Bottom switch (other wire to GND) | 12 | D6 |
| Top switch (other wire to GND) | 13 | D7 |
| Light sensor (other leg to GND) | A0 | A0, plus a 33 kΩ resistor from A0 to **3V3** |
| Driver **COM** and sensor ground | GND | GND |
| Driver **5VO** (power for the board) | Vin | Vin (NodeMCU) or 5V (D1 mini), never 3V3 |

On the ESP8266:

- **The light sensor needs the resistor.** The Uno used its built-in pull-up on A0, but the ESP8266's A0 doesn't have one.
  - 33 kΩ is best. Three 10 kΩ resistors soldered in a row (30 kΩ) also work.
  - Solder it to the sensor wire (guide, Part 5). If it comes loose, the board reads "bright" and won't close the door.
- **Check the driver's 5VO voltage before you use it for power** (guide, Part 3). Use it only if it reads 4.5–5.5 V. Otherwise, power the board from a USB phone charger.
- [wiring-esp8266.svg](wiring-esp8266.svg) shows every connection on the NodeMCU.

## Setup (ESP8266)

[UPGRADE_GUIDE.md](UPGRADE_GUIDE.md), Part 2, has the full steps. In short:

1. **Add the ESP8266 boards to the Arduino IDE.** Under **File > Preferences**, add this *Additional boards manager URL*: `https://arduino.esp8266.com/stable/package_esp8266com_index.json`. Then install **esp8266** from the Boards Manager.
2. **Select your board:** **NodeMCU 1.0 (ESP-12E Module)**, or **LOLIN(WEMOS) D1 R2 & mini** for a D1 mini.
3. **Create `arduino_secrets.h`** from `arduino_secrets.example.h`. Fill in your WiFi details (2.4 GHz networks only), your notification topic and a password for the buttons. Don't upload this file to GitHub.
4. **Set up notifications:** install the **ntfy** phone app and subscribe to your topic.
5. **Upload,** then open the Serial Monitor at **115200 baud**. It prints the status page address: `http://coopdoor.local`, or the IP address if your phone doesn't support `.local` names.

The door keeps working if WiFi is down; only the status page and notifications stop.

## Light levels

Readings go **down** as it gets **brighter**. The two boards read the same light slightly differently, so each has its own levels:

| Board | Opens at | Closes at |
|---|---|---|
| Uno | 700 or lower | 880 or higher |
| ESP8266 (starting values) | 690 or lower | 840 or higher |

If you tune the Uno levels in stage 1, copy them into the Uno lines here before you upload.

The ESP8266 starting values are converted from the Uno's, assuming a 33 kΩ resistor. Fine-tune them by watching the status page at dawn and dusk (guide, Part 7).

**Watch for the "Door closed" notification every evening.** The board doesn't warn you if it never gets dark enough to close. A missing notification means you should check the door.
