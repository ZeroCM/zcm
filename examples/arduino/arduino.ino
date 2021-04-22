#include <zcm-arduino.h>
#include <zcm/transport/third-party/embedded/arduino/transport_arduino_serial.h>

zcm_t zcm;

uint64_t timestamp_now(void* usr) {
  return micros();
}

void setup() {
  zcm_trans_t *trans = zcm_trans_arduino_serial_create(9600, timestamp_now, NULL);
  assert(trans);
  zcm_init_from_trans(&zcm, trans);
}

#define buflen (10)
uint8_t buf[buflen];

void loop() {
  zcm_publish(&zcm, "TEST", buf, buflen);
  delay(500);
}
