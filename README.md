# Good_morning
Scheduled curtain control: opens and closes at specific times

# Mechanical
### Assets 
* Fusion 360 link: https://a360.co/35GVD2y
* See release for STEP/STL files

### Improvements 
* Add stiffness/ribs to reduce outer frame deflection 
* smaller PCB mounting holes (changed diametr from 3mm to 2.7mm)
* larger chamfer to match curtain rail I have at home
* tighter tolerance on bearing holder 
* choose a lighter/smaller motor bracket
* choos weaker/smaller mechanical end stops

# Electronics

### Description
Features:
* ESP32 WROOM32 module
* 1x button
* 1x programmable LED
* 1x VEML6030 ambient light sensor 
* 1x CP2104 USB-UART bridge
* 1x DRV8825 stepper motor driver with controllable STEP, DIR, /SLEEP and /ENA 

Power:
* 3S Lipo battery, charged externally - not by this board
* LDO for USB power during programming 
* 12V-3.3V 1A switching regulator 
* resistor divider to monitor battery voltage

>find out how long it lasts on 1 full charge

### Pinout 
|Signal|Pin|Notes|
|:---:|:----:|:---:|
|Stepper_step|16||
|Stepper_direction|17||
|Stepper_enable|4|active low|
|Stepper_sleep|5|active low|
|Limit_switch_1|14|has external pullup (R23)|
|Limit_switch_2|12|no external pullup (R22), enable internal pullup|
|I2C_SCL|22||
|I2C_SDA|23||
|Ambient_light_sensor_int|21||
|LED|13||
|Button|15|enable internal pullup|
|Battery_voltage|I35, A1_7|ADC1 channel 7|


### Errata
* Remove R22: the external pullup messes with communication with flash when programming. 
* Add 10k pull up resistor on the nENA line so that when the MCU is asleep the stepper is not enabled. 

# Firmware

### Structure
* Wake up from sleep 
* configure pins
* check battery level: if it is low blink the light and get stuck in this mode until the battery is recharged
* else, connect to wifi 
* get time from NTP server
* check if it time to open/close the curtains 
* open/close the curtains
* turn Wi-Fi OFF
* set wake up timer 
* set wake up button 
* go into deep sleep 

> NOTE: this firmware version doesn't use the end stops to check the curtain status because the end stops I have are too strong and don't work. It will be updated once I find new end stops. 

### Personal notes / improvements to do
* have a seperate RTC chip with less drift ? 
* you can retain some simple things in RTC memory 
```cpp
RTC_DATA_ATTR int bootCount = 0;
```
* change open/close times over wifi (from M5 stack core maybe?)
* "wireless wake up"?
* gather data from ambient light sensor over a week or so and see if there are any interesting patterns 

### reference links
* [deep sleep](https://lastminuteengineers.com/esp32-deep-sleep-wakeup-sources/)
* [NTP server](https://lastminuteengineers.com/esp32-ntp-server-date-time-tutorial/)
* [stepper driving with timers]()