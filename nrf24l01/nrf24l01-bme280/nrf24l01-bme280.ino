

#define INTERVAL 10


#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include <EEPROM.h>


#include <elapsedMillis.h>

#include <Wire.h>
#include <BME280.h>
#include <BME280I2C.h>
#include <EnvironmentCalculations.h>



BME280I2C::Settings settings(
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_Off,
   BME280::SpiEnable_False,
   0x77 // I2C address. I2C specific.
);

BME280I2C bme(settings); //I2C


/**** Configure the nrf24l01 CE and CS pins ****/
RF24 radio(7, 8);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

/**
 * User Configuration: nodeID - A unique identifier for each radio. Allows addressing
 * to change dynamically with physical changes to the mesh.
 *
 * In this example, configuration takes place below, prior to uploading the sketch to the device
 * A unique value from 1-255 must be configured for each node.
 * This will be stored in EEPROM on AVR devices, so remains persistent between further uploads, loss of power, etc.
 *
 **/
#define nodeID 1
bool IS_BME;

int statusLed = 13; // normal led on micro
int errorLed = 17; // rx led on arduino micro


uint32_t displayTimer = 0;

struct payload_t {
  float temp;
  float hum;
  float pa;
  float es;
  float ea;
  float vpd;
  float ah_gm3;
};


void flashLed(int pin, int times, int wait) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(wait);
    digitalWrite(pin, LOW);
    if (i + 1 < times) {
      delay(wait);
    }
  }
}


void setup() {
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);
  
  Serial.begin(9600);
  if(!Serial){
    delay(1000);
  }
  Wire.begin();

  while(!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    flashLed(errorLed, 10, 100);
    delay(1000);
  }
  // technically this should work for bmp280 however I couldnt get it to work with mine.
  switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       flashLed(statusLed, 10, 100);
       IS_BME = true;
       break;
     case BME280::ChipModel_BMP280:
       flashLed(statusLed, 15, 100);
       IS_BME = false;
       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       flashLed(errorLed, 15, 100);
       Serial.println("Found UNKNOWN sensor! Error!");
       
  }
  
  // Set the nodeID manually
  mesh.setNodeID(nodeID);
  // Connect to the mesh
  Serial.println(F("Connecting to the mesh..."));
  mesh.begin();
}


payload_t getData() {
  // write header
  
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
  float pa, temp, hum;
  bme.read(pa, temp, hum, tempUnit, presUnit);  

  unsigned int numValues = 10;
  if (!IS_BME){
    numValues = 5;
   
  }
  
  

  // saturated vapor pressure
  float es = 0.6108 * exp(17.27 * temp / (temp + 237.3));
  float ea, vpd, ah_gm3;
  if (IS_BME){
    float dewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
    // actual vapor pressure
    ea = hum / 100.0 * es;
    
    // this equation returns a negative value (in kPa), which while technically correct,
    // is invalid in this case because we are talking about a deficit.
    vpd = (ea - es) * -1;
    // absolute humidity (in kg/m³)
    float ah_kgm3 = ea / (461.5 * (temp + 273.15));
    // report it as g/m³
    ah_gm3 = ah_kgm3 * 1000;
  }
  payload_t payload = {temp, hum, pa, es, ea, vpd, ah_gm3};
  return payload;
}

void loop() {

  mesh.update();

  // Send to the master node every second
  if (millis() - displayTimer >= 1000) {
      payload_t payload = getData();
    
    if (!mesh.write(&payload, 'M', sizeof(payload))) {

      // If a write fails, check connectivity to the mesh network
      if ( ! mesh.checkConnection() ) {
        //refresh the network address
        Serial.println("Renewing Address");
        mesh.renewAddress();
      } else {
        Serial.println("Send fail, Test OK");
      }
    } else {
      Serial.print("Send OK: "); Serial.println(displayTimer);
    }
  }
  
  
//  while (network.available()) {
//    RF24NetworkHeader header;
//    payload_t payload;
//    network.read(header, &payload, sizeof(payload));
//    Serial.print("Received packet #");
//    Serial.print(payload.counter);
//    Serial.print(" at ");
//    Serial.println(payload.ms);
//  }
}






