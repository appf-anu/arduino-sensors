# arduino-sensors
remote sensing with arduinos

The sketches here are categorized by wireless module and then sensor type.

If you would like to add a sensor or a wireless module, please use msgpack to send the data, and be aware of how the keys are suffixed with the unit (such as "temp_c") and how each packet also includes a string for "stype" being the sensor type (eg "stype": "bme280").

you can select some different output formats for the base station.



example wirings:

![Example wiring diagram image](resources/micro-xbee-bme280_bb)

![Example wiring diagram image](resources/micro-xbee-am2302_bb)
