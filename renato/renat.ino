#include <AccelStepper.h>

const int PIN_STEP = 2;
const int PIN_DIR  = 3;
const int PIN_EN   = 4;          // attivo basso

const long PASSI_GIRO = 1600;    // 200 passi x 1/8 microstep

AccelStepper motore(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);

void setup() {
  Serial.begin(115200);

  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, LOW);     // abilita il driver

  motore.setMaxSpeed(800);       // passi/s, valore già validato
  motore.setAcceleration(400);   // passi/s^2

  Serial.println("Test STEP/DIR - avvio");
}

void loop() {
  Serial.println("Un giro in avanti");
  motore.move(PASSI_GIRO);
  motore.runToPosition();        // bloccante: attende fine movimento
  delay(1000);

  Serial.println("Un giro indietro");
  motore.move(-PASSI_GIRO);
  motore.runToPosition();
  delay(1000);
}