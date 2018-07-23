# arduino-sensors
remote sensing with arduinos

The sketches here are categorized by wireless module and sensor type until I work out a way to better modularize the parts.

If you would like to add a sensor or a wireless module, im suing msgpack to send the data. Please be aware of how the keys are suffixed with the unit (such as "temp_c") and how each packet also includes a string for "stype" being the sensor type (eg "stype": "bme280") and a string for the node id.

you can select different output formats for the base station, including sending the data straight to telegraf through a http handler.

### xbee configuration
Make sure xbees are on the same network.

They should be in "API Mode With Escapes [2]".

Remember to write down your base stations address (mine look like 0013A200 40BF137D) the last half of this address should go in BASE_SL with the 0x prefix (like 0x40BF137D).

### general configuration
The NODE_ID define should be changed for each arduino you program, this is just to help you keep track of which node is which.

you can change the interval through the INTERVAL define at the start of each sketch. it is in seconds


## dependencies

all the arduino sketches depend on these libraries:

[Xbee-Arduino](https://github.com/andrewrapp/xbee-arduino)

[ArduinoBufferedStreams](https://github.com/paulo-raca/ArduinoBufferedStreams)

[arduino_msgpack](https://github.com/HEADS-project/arduino_msgpack)

some of them depend on these libraries:

[DFRobot_BME280](https://github.com/DFRobot/DFRobot_BME280)

[DHT-sensor-library](https://github.com/adafruit/DHT-sensor-library)


the base station depends on these external python packages

[digi-xbee](https://github.com/digidotcom/python-xbee)
