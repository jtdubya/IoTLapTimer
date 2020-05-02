# IoTLapTimer
This is an IoT LapTimer that uses ultrasonic sensors and a numeric display and runs on Arduino. The lap timer displays the last lap time and lap count. I know you can just buy digital slot cars but they are expensive and its fun to make stuff!

The timer is built with the a low cost wifi board, ultrasonic sensor, and display. The IoT lap timer communicates with the [LapTimerServer](https://github.com/jtdubya/LapTimerServer) I wrote to conduct races with other lap timers.  

<div align="center">
  <img style='width: 500px' src="media/LapTimerDemo.gif"></img>
</div>

## Components:
1. [ESP8226 board](https://www.amazon.com/gp/product/B010N1SPRK/ref=ppx_yo_dt_b_asin_title_o01_s00?ie=UTF8&psc=1)
1. [HC-SR04 ultrasonic distance sensor](https://www.amazon.com/gp/product/B07SC1YJ21/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
1. [Adafruit 0.56" 4-Digit 7-Segment Display w/I2C Backpack](https://www.adafruit.com/product/879)
4. [Power bank](https://www.microcenter.com/product/615771/inland-5,200mah-power-bank---black)
1. [Mini bread board](https://www.microcenter.com/product/481840/velleman-170-tie-points-mini-breadboards---4-pack)
6. Jumper wires
    * 4 male to female (ultrasonic sensor to ESP8226 board)
    * 4 female to female (display to ESP8226 board)
7. [LapTimerServer](https://github.com/jtdubya/LapTimerServer)

## How it works
Timing is really simple: the distance sensor is used to check for the passing slot car. Lap times and counts are displayed at the end of each lap and sent to the lap timer server for storage. 

Here's a state diagram for the lap timer:
<div align="center">
  <img style='width: 550px' src="media/LapTimerStateDiagram.png"></img>
</div>

### Lap Timer State Behavior
1. Not Registered
    * LED displays all dashes 
    * Timer attempts to register with the server
1. Registered
    * LED displays the letter **L** with the lap timer ID from the server
    * Timer waits for the server to change state to start countdown and gets the time until race start
1. Race Start Countdown
    * LED displays the time until race start 
1. Race in Progress
    * LED displays the lap time while the lap is in progress then flashes the lap time and lap count at the end of each lap
    * The timer sends the lap time to the server at the end of every lap
    * Timer transitions to the race finish state once the number of races laps has been completed
1. Race Finish
    * If not all cars have finished and the race finish countdown is in progress:
        * LED displays all dashes 
    * If all cars have finished or the race finish countdown has expired:
        * LED will cycle through the race results twice then wait for the next race to start
        
### Race Finish Results Display Cycle
Once the race is finished the LED display will cycle through the following twice:
1. The lap timer place preceded with the letter **P** 
1. The letters **OA** then the overall time
1. The letters  **FL** then the fastest lap
1. The letters **FL** then the faster lap number

After the cycle is completed the place will be displayed until the next race starts. 

## Setup
1. Connect the ESP8266, ultrasonic sensor, and display according to [wiring diagram](#Wiring-Diagram) below. You can use the mini bread boards like [these](https://www.microcenter.com/product/481840/velleman-170-tie-points-mini-breadboards---4-pack) to hold everything
1. Update the [NetworkConfiguration.txt](./NetworkConfiguration.txt) file with your network configuration and change the file type to a header file (`NetworkConfiguration.h`)
1. Open [IoTLapTimer.ino](./IoTLapTimer.ino) in Arduino IDE and upload the sketch to the ESP8226 board

## Wiring Diagram

<div align="center">
  <img style='width: 500px' src="media/WiringDiagram.png"></img>
</div>
