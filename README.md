## Arduino Powered Chicken Coop Door Opener

## Overview

Using an arduino, a motor shield, and a car door window motor, I created a door that opens and closes automatically based upon a photoresistor's regular readings. 

## Motivation

Chickens are inherently vulnerable in the dark and I'm not always able to close the coop door as soon as they're done free-ranging for the day. Additionally, the sun comes up earlier than I usually make it out to let the chickens free range for the day, so I wanted to let them out earlier to allow for more foraging as well, they say the early bird gets the worm.

## Components

- 1 - Arduino Uno
- 1 - Motor
- 1 - Motor Shield
- 1 - Photoresistor
- 2 - Magnetic Switches
- 1 - "Guillotine" style coop door

## Tools

- Soldering iron
- Solder
- Waterproof heatshrink tubing
- Screw gun
- Circular saw

## My journey (aka brief instructions)

This first started more than 10 years ago, when I made my first automatic coop door opener. I rigged that one using a two-valve hose water timer, two buckets, and a series of pulleys, which it worked great until winter time came around. So when we moved to our property and got a new flock of chickens, I knew a 'smart coop' was on the agenda. I did my usual googling and found a fair number of people creating their own DIY solutions and even a small business selling one for <$100. But, I wanted the future potential expansion that the arduino offered, and I thought I could do it cheaper (I was right, sort of). 

I bought my new arduino and got to work testing out motor control code, photoresistors, and motor shields. After some trial and error, I learned about stall force, and the impact that has on the specifications you need for your motor and motor shield. Essentially, because V is always equal to I * R, there's often a momentary spike when the motor is turned on, leading to potential overloading or overheating of your components. While the stall force issue was a tricky problem for me to solve, the plus side was that I improved/fail-safed basically every other component of the system in the process. 

Once I had the basics figured out and my initial proof of concept spinning away on my workbench, it was time to right the program to put it all together. I detailed out some requirements and set out writing code to fulfil them. At each step, I manually tested the program and used the serial monitor to output runtime messages. The final piece was to install the motor and components into the chicken coop. This will be highly variable based upon the motor you use and the layout of your coop so I won't go into much detail, but you can see the final product in the images.

## Requirements

- It should read the brightness outside multiple times per minute
- It should open the door when it's bright enough and the door is not open
- It should close the door when it's dark and the door is not closed
- It should take 3 separate readings before determining to open/close the door
- All 3 readings should be able to the applicable threshold
- It should alternatively display the current door status (red/green) and the sky color (light/dark).=
- It should indicate when the door is opening or closing
- It should go into a 15 minute timeout when it attempt to open or close more than 3 times without succeeding
- It should run until it reaches the open or closed switch or reaches its timeout limit









