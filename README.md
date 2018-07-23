# arduino-sensors
remote sensing with arduinos

The sketches here are categorized by wireless module and then sensor type.

If you would like to add a sensor or a wireless module, please use msgpack to send the data, and be aware of how the keys are suffixed with the unit (such as "temp_c") and how each packet also includes a string for "stype" being the sensor type (eg "stype": "bme280").

you can select different output formats for the base station, including sending the data straight to telegraf through a http handler.

### xbee configuration
for configuration of xbees remember to set them to operate with escapes.



all the arduino sketches depend on these libraries:

[Xbee-Arduino](https://github.com/andrewrapp/xbee-arduino)

[ArduinoBufferedStreams](https://github.com/paulo-raca/ArduinoBufferedStreams)

[arduino_msgpack](https://github.com/HEADS-project/arduino_msgpack)

some of them depend on these libraries:

[DFRobot_BME280](https://github.com/DFRobot/DFRobot_BME280)

[DHT-sensor-library](https://github.com/adafruit/DHT-sensor-library)


the base station depends on these external python packages

[digi-xbee](https://github.com/digidotcom/python-xbee)
