#include <AccelStepper.h>

const int PIN_STEP   = 2;
const int PIN_DIR    = 3;
const int PIN_EN     = 4;   // attivo basso
const int PIN_FC_MIN = 5;   // NC verso GND
const int PIN_FC_MAX = 6;   // NC verso GND

const long  PASSI_GIRO    = 1600;
const float VEL_HOMING    = 800;    // passi/s, validata
const float VEL_LENTA     = 200;    // ri-approccio di precisione
const long  RITRAZIONE    = 800;    // mezzo giro di allontanamento
const long  CORSA_MAX     = PASSI_GIRO * 20L;  // limite software: adatta alla tua corsa

AccelStepper motore(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);

bool minPremuto() { return digitalRead(PIN_FC_MIN) == HIGH; }  // NC: HIGH = premuto o filo rotto
bool maxPremuto() { return digitalRead(PIN_FC_MAX) == HIGH; }

// ---- Homing: cerca il finecorsa MIN e fissa lo zero ----
bool esegueHoming() {
  Serial.println("Homing: discesa verso fc MIN...");

  // Se siamo gia' sul finecorsa, prima allontanati
  if (minPremuto()) {
    motore.setMaxSpeed(VEL_LENTA);
    motore.move(RITRAZIONE);
    while (motore.distanceToGo() != 0) motore.run();
    if (minPremuto()) { Serial.println("ERRORE: fc MIN sempre premuto (filo rotto?)"); return false; }
  }

  // Discesa veloce fino al contatto
  motore.setMaxSpeed(VEL_HOMING);
  motore.move(-CORSA_MAX);
  while (!minPremuto()) {
    if (motore.distanceToGo() == 0) { Serial.println("ERRORE: fc MIN mai raggiunto"); return false; }
    motore.run();
  }
  motore.stop();
  motore.runToPosition();

  // Ritrazione e ri-approccio lento per precisione
  motore.setMaxSpeed(VEL_LENTA);
  motore.move(RITRAZIONE);
  while (motore.distanceToGo() != 0) motore.run();

  motore.move(-RITRAZIONE * 2);
  while (!minPremuto()) {
    if (motore.distanceToGo() == 0) { Serial.println("ERRORE: contatto perso"); return false; }
    motore.run();
  }
  motore.stop();
  motore.runToPosition();

  motore.setCurrentPosition(0);   // <-- questo punto e' lo ZERO
  Serial.println("Homing OK, posizione zero fissata.");
  return true;
}

// ---- Movimento protetto: si ferma se scatta un finecorsa ----
bool muoviProtetto(long destinazione) {
  motore.setMaxSpeed(VEL_HOMING);
  motore.moveTo(destinazione);
  while (motore.distanceToGo() != 0) {
    if (maxPremuto()) { motore.stop(); motore.runToPosition();
      Serial.println("STOP: fc MAX!"); return false; }
    if (minPremuto() && motore.distanceToGo() < 0) { motore.stop(); motore.runToPosition();
      Serial.println("STOP: fc MIN!"); return false; }
    motore.run();
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, LOW);
  pinMode(PIN_FC_MIN, INPUT_PULLUP);
  pinMode(PIN_FC_MAX, INPUT_PULLUP);
  motore.setAcceleration(400);

  // Test statico dei finecorsa prima di muovere
  Serial.print("fc MIN a riposo: "); Serial.println(minPremuto() ? "PREMUTO (!)" : "ok");
  Serial.print("fc MAX a riposo: "); Serial.println(maxPremuto() ? "PREMUTO (!)" : "ok");

  if (!esegueHoming()) { digitalWrite(PIN_EN, HIGH); while (true); }  // blocco di sicurezza
}

void loop() {
  Serial.println("Salgo a 5 giri dallo zero");
  if (!muoviProtetto(PASSI_GIRO * 5L)) { digitalWrite(PIN_EN, HIGH); while (true); }
  delay(1500);

  Serial.println("Torno a 1 giro dallo zero");
  if (!muoviProtetto(PASSI_GIRO * 1L)) { digitalWrite(PIN_EN, HIGH); while (true); }
  delay(1500);
}