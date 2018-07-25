

// node id
#define NODE_ID "node01"
// interval in seconds
#define INTERVAL 10
// Serial Low of the base station
#define BASE_SL 0x40BF137D

// there is a pullup/down for changing the sensor to 0x76, change it here if you want
#define BME280_ADDRESS 0x77


#define SEA_LEVEL_PRESSURE  1013.25f // required to calculate altitude

#include <elapsedMillis.h>
#include <LoopbackStream.h>
#include <msgpck.h>

#include <XBee.h>

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

// buffer to store our msgpack message in
LoopbackStream buffer(512);

// floats for our data values
float temp, pa, hum, alt, es, ea, vpd, ah_kgm3, ah_gm3;

// create the XBee object
XBee xbee = XBee();
// SH + SL Address of receiving XBee
XBeeAddress64 addr64 = XBeeAddress64(0x0013A200, BASE_SL);
// txstatus for the xbee, this is how we can know if an xbee is actually plugged in
ZBTxStatusResponse txStatus = ZBTxStatusResponse();

int statusLed = 13; // normal led on micro
int errorLed = 17; // rx led on arduino micro

bool IS_BME = true;


unsigned long intervalMillis = INTERVAL*1000;

void setup() {
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);

//  Serial.begin(9600);
  // on the nano, we will need to use Serial for xbee
  Serial.begin(9600);
  xbee.setSerial(Serial);

  Wire.begin();
  bme.begin();
//  while(!bme.begin())
//  {
////    Serial.println("Could not find BME280 sensor!");
//    flashLed(errorLed, 10, 100);
//    delay(1000);
//  }
  // technically this should work for bmp280 however I couldnt get it to work with mine.
  switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
//       Serial.println("Found BME280 sensor! Success.");
       flashLed(statusLed, 10, 100);
       IS_BME = true;
       break;
     case BME280::ChipModel_BMP280:
       flashLed(statusLed, 15, 100);
       IS_BME = false;
//       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       flashLed(errorLed, 15, 100);
//       Serial.println("Found UNKNOWN sensor! Error!");
       
  }
}

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

void sendData(char t[], size_t s) {
  ZBTxRequest zbTx = ZBTxRequest(addr64, t, s);
  xbee.send(zbTx);

  // flash TX indicator
  flashLed(statusLed, 1, 100);


  // after sending a tx request, we expect a status response
  // wait up to half second for the status response
  // this is why we need to delay 10s minus
  xbee.readPacket(500);

  if (xbee.getResponse().isAvailable()) {

    // got a response!
    // should be a znet tx status
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);

      // get the delivery status, the fifth byte
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        // success.  time to celebrate
        flashLed(statusLed, 1, 50);
//         Serial.println("Send packet. Success");
      } else {
        // the remote XBee did not receive our packet. is it powered on?
        flashLed(errorLed, 3, 50);
//         Serial.println("the remote XBee did not receive our packet. is it powered on?");
      }
    }
  } else if (xbee.getResponse().isError()) {
    flashLed(errorLed, 5, 50);
//     Serial.print("Error reading packet.  Error code: ");
//     Serial.println(xbee.getResponse().getErrorCode());

  } else {
    // local XBee did not provide a timely TX Status Response -- should not happen
    flashLed(errorLed, 2, 50);
//     Serial.println("local XBee did not provide a timely TX Status Response, check connections.");
  }
}

size_t getData(char data[]) {
  // write header
  
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
  bme.read(pa, temp, hum, tempUnit, presUnit);  

  unsigned int numValues = 10;
  
  msgpck_write_map_header(&buffer, numValues); // enough space to fit the values
  
  msgpck_write_string(&buffer, "node"); // node id
  msgpck_write_string(&buffer, NODE_ID);
  msgpck_write_string(&buffer, "stype"); // sensor type
  msgpck_write_string(&buffer, "bme280");

  msgpck_write_string(&buffer, "temp_c"); 
  msgpck_write_float(&buffer, temp);
  msgpck_write_string(&buffer, "pa_p");
  msgpck_write_float(&buffer, pa);

  // saturated vapor pressure
  es = 0.6108 * exp(17.27 * temp / (temp + 237.3));
  msgpck_write_string(&buffer, "es_kPa");
  msgpck_write_float(&buffer, es);


  msgpck_write_string(&buffer, "rh_per");
  msgpck_write_float(&buffer, hum);  
  float dewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
  msgpck_write_string(&buffer, "dewP_c");
  msgpck_write_float(&buffer, dewPoint);
  
  // actual vapor pressure
  ea = hum / 100.0 * es;
  msgpck_write_string(&buffer, "ea_kPa");
  msgpck_write_float(&buffer, ea);

  // this equation returns a negative value (in kPa), which while technically correct,
  // is invalid in this case because we are talking about a deficit.
  vpd = (ea - es) * -1;
  msgpck_write_string(&buffer, "vpd_kPa");
  msgpck_write_float(&buffer, vpd);   
  // absolute humidity (in kg/m³)
  ah_kgm3 = ea / (461.5 * (temp + 273.15)) * 1000;
  msgpck_write_string(&buffer, "ah_kgm3");
  msgpck_write_float(&buffer, ah_kgm3);
//  
  size_t c = 0;
  while (buffer.available()) {
    data[c] = buffer.read();
    c++;
  }
  return c;
}

void loop() {
  // recorde how long it takes for this loop to run.
  elapsedMillis timeElapsed = 0;

  // buffer of 512 bytes
  char dataBuffer[512];
  // fill the initial buffer with data
  size_t s = getData(dataBuffer);

  // make a new bytearray of the correct length
  char data[s];
  for (size_t i = 0; i < s; i++) {
    data[i] = dataBuffer[i];
  }

  // send data to serial, including a newline (otherwise we dont really know when to end)
  // dont do this for nanos or other arduinos without 2x serial outs
//    for (size_t i = 0; i < sizeof(data); i++) {
//      Serial.print(data[i]);
//    }
//    Serial.println();

  // send the data over wireless
  sendData(data, s);

  // delay 10 seconds minus the time it took to run this loop
  delay(intervalMillis-timeElapsed);
}
