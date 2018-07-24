// node id
#define NODE_ID "node01"
// interval in seconds
#define INTERVAL 60

// Serial Low of the base station
#define BASE_SL 0x40BF137D
#define DHTPIN 7
#define DHTTYPE DHT22

#define SENSOR_TYPE "dht22"
#define SEA_LEVEL_PRESSURE  1013.25f // required to calculate altitude

#include <elapsedMillis.h>
#include <LoopbackStream.h>
#include <msgpck.h>

#include <XBee.h>
#include <DHT.h>

DHT dht(DHTPIN, DHTTYPE);

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

unsigned long intervalMillis = INTERVAL*1000;

void setup() {
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);

  Serial.begin(9600);
  // on the nano, we will need to use Serial for xbee
  Serial1.begin(9600);
  xbee.setSerial(Serial1);

  // we have no way of knowing if a dht is actually working on this thing so just leave it
  dht.begin();
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
        flashLed(statusLed, 2, 100);
        // Serial.println("Send packet. Success");
      } else {
        // the remote XBee did not receive our packet. is it powered on?
        flashLed(errorLed, 3, 100);
        // Serial.println("the remote XBee did not receive our packet. is it powered on?");
      }
    }
  } else if (xbee.getResponse().isError()) {
    flashLed(errorLed, 5, 100);
    // Serial.print("Error reading packet.  Error code: ");
    // Serial.println(xbee.getResponse().getErrorCode());

  } else {
    // local XBee did not provide a timely TX Status Response -- should not happen
    flashLed(errorLed, 10, 10);
    // Serial.println("local XBee did not provide a timely TX Status Response");
  }
}

size_t getData(char data[]) {
  // The sensor can only be read from every 1-2s, and requires a minimum
  // 2s warm-up after power-on.
  // just delay for 2s just to be sure, all values will be 2s late
  delay(2000);

  // write header
  msgpck_write_map_header(&buffer, 8); // needs enough space to fit the map
  msgpck_write_string(&buffer, "node"); // node id
  msgpck_write_string(&buffer, NODE_ID);
  msgpck_write_string(&buffer, "stype"); // sensor type
  msgpck_write_string(&buffer, SENSOR_TYPE);


  temp = dht.readTemperature(true);
  msgpck_write_string(&buffer, "temp_c"); // write key
  msgpck_write_float(&buffer, temp); //  write value

  hum = dht.readHumidity(true);
  msgpck_write_string(&buffer, "hum_rh");
  msgpck_write_float(&buffer, hum);

  // saturated vapor pressure
  es = 0.6108 * exp(17.27 * temp / (temp + 237.3));
  msgpck_write_string(&buffer, "es_kPa");
  msgpck_write_float(&buffer, es);

  // actual vapor pressure
  ea = hum / 100.0 * es;
  msgpck_write_string(&buffer, "ea_kPa");
  msgpck_write_float(&buffer, ea);

  // this equation returns a negative value (in kPa), which while technically correct,
  // is invalid in this case because we are talking about a deficit.
  vpd = (ea - es) * -1;
  msgpck_write_string(&buffer, "vpd_kPa");
  msgpck_write_float(&buffer, vpd);

  // mixing ratio
  //w = 621.97 * ea / ((pressure64/10) - ea);
  // saturated mixing ratio
  //ws = 621.97 * es / ((pressure64/10) - es);

  // absolute humidity (in kg/m³)
  ah_kgm3 = es / (461.5 * (temp + 273.15));
  // report it as g/m³
  ah_gm3 = ah_kgm3 * 1000;
  msgpck_write_string(&buffer, "ah_gm3");
  msgpck_write_float(&buffer, ah_gm3);

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
  for (size_t i = 0; i < sizeof(data); i++) {
    Serial.print(data[i]);
  }
  Serial.println();

  // send the data over wireless
  sendData(data, s);

  // delay 10 seconds minus the time it took to run this loop
  delay(intervalMillis-timeElapsed);
}