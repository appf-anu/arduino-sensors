// use this to print debug messages to Serial
//#define SERIAL_DEBUG
// this will disable communicating with the xbee
//#define NOXBEE
// use this if you have an arduino micro, mega or something with the xbee connected to Serial1
//#define XBEE_SERIAL1
// use this to print data to Serial
//#define SERIAL_DATA

// node id
#define NODE_ID "node01"
// interval in seconds
#define INTERVAL 60
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
   BME280_ADDRESS // I2C address. I2C specific.
);

BME280I2C bme(settings); //I2C

// buffer to store our msgpack message in
LoopbackStream buffer(512);

// create the XBee object
XBee xbee = XBee();
// SH + SL Address of receiving XBee
XBeeAddress64 addr64 = XBeeAddress64(0x0013A200, BASE_SL);
// txstatus for the xbee, this is how we can know if an xbee is actually plugged in
ZBTxStatusResponse txStatus = ZBTxStatusResponse();

int statusLed = 13; // normal led on micro
int errorLed = 17; // rx led on arduino micro

bool IS_BME = true;
char SENSOR_TYPE[7] = "bme280";
unsigned long intervalMillis = INTERVAL*1000;

void setup() {
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);

  Serial.begin(9600);
  if (!Serial){
    delay(1000);
  }

#ifdef XBEE_SERIAL1
  Serial1.begin(9600);
  xbee.setSerial(Serial1);
#else
  xbee.setSerial(Serial);
#endif

  Wire.begin();
  bool addr = false;
  while(!bme.begin()){
    addr = !addr;
    settings.bme280Addr = addr ? 0x77 : 0x76;
    bme.setSettings(settings);
#ifdef SERIAL_DEBUG
     Serial.print("Could not find BM*280 sensor on address: 0x");
     Serial.println(settings.bme280Addr, HEX);
#endif
    flashLed(errorLed, 10, 100);
    delay(1000);
  }
  // technically this should work for bmp280 however I couldnt get it to work with mine.
  switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
#ifdef SERIAL_DEBUG
       Serial.println("Found BME280 sensor! Success.");
#endif
       flashLed(statusLed, 10, 100);
       IS_BME = true;
       break;
     case BME280::ChipModel_BMP280:
       flashLed(statusLed, 15, 100);
       IS_BME = false;
       SENSOR_TYPE[2] = 'p';
#ifdef SERIAL_DEBUG
       Serial.println("Found BMP280 sensor! No Humidity available.");
#endif
       break;
     default:
       flashLed(errorLed, 15, 100);
#ifdef SERIAL_DEBUG
       Serial.println("Found UNKNOWN sensor! Error!");
#endif
  }
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

#ifdef SERIAL_DATA
   for (size_t i = 0; i < sizeof(data); i++) {
     Serial.print(data[i]);
   }
   Serial.println();
#endif
  // send the data over wireless
#ifndef NOXBEE
  sendData(data, s);
#endif
  // delay 10 seconds minus the time it took to run this loop
  unsigned long startMillis = millis();
  while (millis() - startMillis < (intervalMillis-timeElapsed));
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

#ifdef SERIAL_DEBUG
         Serial.println("Send packet. Success");
#endif

      } else {
        // the remote XBee did not receive our packet. is it powered on?
        flashLed(errorLed, 3, 50);
#ifdef SERIAL_DEBUG
        Serial.println("the remote XBee did not receive our packet. is it powered on?");
#endif
      }
    }
  } else if (xbee.getResponse().isError()) {
    flashLed(errorLed, 5, 50);
#ifdef SERIAL_DEBUG
    Serial.print("Error reading packet.  Error code: ");
    Serial.println(xbee.getResponse().getErrorCode());
#endif

  } else {
    // local XBee did not provide a timely TX Status Response -- should not happen
    flashLed(errorLed, 2, 50);
#ifdef SERIAL_DEBUG
    Serial.println("local XBee did not provide a timely TX Status Response, check connections.");
#endif
  }
}


void fillBuffer(float temp, float hum, float pa){
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;

  unsigned int numValues = 9;

  if (pa == -1.0){
    numValues -= 1;
  }
  if (hum == -1.0) {
    numValues -= 5;
  }
  // write header
  msgpck_write_map_header(&buffer, numValues); // enough space to fit the values
  msgpck_write_string(&buffer, "node"); // node id
  msgpck_write_string(&buffer, NODE_ID);
  msgpck_write_string(&buffer, "stype"); // sensor type
  msgpck_write_string(&buffer, SENSOR_TYPE);

  msgpck_write_string(&buffer, "temp_c");
#ifdef SERIAL_DEBUG
  Serial.print("temp_c: ");
  Serial.print(temp);
  Serial.print(" ");
#endif
  msgpck_write_float(&buffer, temp);

  if (pa > 0){
    msgpck_write_string(&buffer, "pa_p");
#ifdef SERIAL_DEBUG
    Serial.print("pa_p: ");
    Serial.print(es);
    Serial.print(" ");
#endif
    msgpck_write_float(&buffer, pa);

  }

  // saturated vapor pressure
  float es = 0.6108 * exp(17.27 * temp / (temp + 237.3));
  msgpck_write_string(&buffer, "es_kPa");
#ifdef SERIAL_DEBUG
  Serial.print("es_kPa: ");
  Serial.print(es);
  Serial.print(" ");
#endif
  msgpck_write_float(&buffer, es);

  if (hum > 0){

    // relative humidity
    msgpck_write_string(&buffer, "rh_pc");
#ifdef SERIAL_DEBUG
    Serial.print("rh_pc: ");
    Serial.print(hum);
    Serial.print(" ");
#endif
    msgpck_write_float(&buffer, hum);

    // dewPoint
    float dewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
    msgpck_write_string(&buffer, "dewP_c");
    msgpck_write_float(&buffer, dewPoint);

    // actual vapor pressure
    float ea = hum / 100.0 * es;
    msgpck_write_string(&buffer, "ea_kPa");
#ifdef SERIAL_DEBUG
    Serial.print("ea_kPa: ");
    Serial.print(ea);
    Serial.print(" ");
#endif
    msgpck_write_float(&buffer, ea);

    // this equation returns a negative value (in kPa), which while technically correct,
    // is invalid in this case because we are talking about a deficit.
    // too big to include
//    float vpd = (ea - es) * -1;
//    msgpck_write_string(&buffer, "vpd_kPa");
//#ifdef SERIAL_DEBUG
//    Serial.print("vpd_kPa: ");
//    Serial.print(vpd);
//    Serial.print(" ");
//#endif
//    msgpck_write_float(&buffer, vpd);

    // absolute humidity (in kg/m³)
    // float ah_kgm3 = ea / (461.5 * (temp + 273.15)) * 1000;
    
    float ah_kgm3 = EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit) /1000;
    msgpck_write_string(&buffer, "ah_kgm3");
#ifdef SERIAL_DEBUG

    Serial.print("ah_kgm3: ");
    Serial.print(ah_kgm3);
    Serial.print(" ");
#endif
    msgpck_write_float(&buffer, ah_kgm3);
  }
}


size_t getData(char data[]) {
  // The sensor can only be read from every 1-2s, and requires a minimum
  // 2s warm-up after power-on.
  // just delay for 2s just to be sure, all values will be 2s late
  delay(2000);
  float pa, temp, hum;
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme.read(pa, temp, hum, tempUnit, presUnit);
//  if (!IS_BME){
//    hum = -1;
//  }

  fillBuffer(temp, hum, pa);

  size_t c = 0;
  while (buffer.available()) {
    data[c] = buffer.read();
    c++;
  }
  return c;
}
