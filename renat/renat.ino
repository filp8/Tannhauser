/*
 * ping_pong_raddoppio.ino
 * Ping-pong tra i finecorsa: a ogni inversione la velocita' raddoppia.
 * Dopo N_INVERSIONI il motore si porta al centro della corsa e si ferma.
 *
 * Sequenza:
 *   1. Homing su fc MIN -> zero sul punto di trigger
 *   2. Ping-pong: a ogni finecorsa stop secco, raddoppio velocita', inversione
 *      (alla prima toccata di fc MAX viene misurata la corsa)
 *   3. Raggiunte N inversioni -> movimento al centro (corsa/2) e stop
 *
 * Hardware (configurazione validata):
 *   D2 = STEP, D3 = DIR, D4 = EN (attivo basso)
 *   D5 = fc MIN, D6 = fc MAX (OMRON D2F-L, NC verso GND, INPUT_PULLUP)
 *   TMC2209 standalone 1/8 microstep -> 1600 step/giro
 *
 * Logica NC: pin LOW = riposo, pin HIGH = finecorsa premuto (o filo interrotto).
 */

#include <AccelStepper.h>

// --- Pin ---
const uint8_t PIN_STEP   = 2;
const uint8_t PIN_DIR    = 3;
const uint8_t PIN_EN     = 4;   // attivo basso
const uint8_t PIN_FC_MIN = 5;
const uint8_t PIN_FC_MAX = 6;

// --- Parametri ---
const int   N_INVERSIONI = 3;      // numero di inversioni prima di fermarsi al centro
const float VEL_BASE     = 1000.0;  // velocita' iniziale (step/s)
const float VEL_MAX      = 4000.0; // tetto: oltre ~4000 step/s l'Uno non regge
const float ACCEL        = 1600.0; // step/s^2
const long  BACKOFF      = 400;    // step per liberare lo switch dopo il trigger
const long  CORSA_LUNGA  = 100000L;

AccelStepper motore(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);

int   direzione  = +1;   // +1 = verso fc MAX, -1 = verso fc MIN
int   inversioni = 0;
float vel        = VEL_BASE;
long  posMax     = -1;   // posizione del trigger di fc MAX (misurata al primo passaggio)

enum Fase { PING_PONG, FINE };
Fase fase = PING_PONG;

bool fcMinPremuto() { return digitalRead(PIN_FC_MIN) == HIGH; }
bool fcMaxPremuto() { return digitalRead(PIN_FC_MAX) == HIGH; }

// Stop immediato mantenendo la coordinata corrente
void stopSecco() { motore.setCurrentPosition(motore.currentPosition()); }

void setup() {
  Serial.begin(115200);

  pinMode(PIN_FC_MIN, INPUT_PULLUP);
  pinMode(PIN_FC_MAX, INPUT_PULLUP);
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, LOW);  // driver abilitato

  motore.setMaxSpeed(VEL_BASE);
  motore.setAcceleration(ACCEL);

  Serial.println(F("=== PING-PONG CON RADDOPPIO VELOCITA' ==="));
  Serial.print(F("Inversioni previste: "));
  Serial.println(N_INVERSIONI);

  // --- Homing su fc MIN ---
  Serial.println(F("Homing verso fc MIN..."));
  if (!fcMinPremuto()) {
    motore.move(-CORSA_LUNGA);
    while (!fcMinPremuto()) motore.run();
  }
  stopSecco();
  motore.setCurrentPosition(0);   // zero sul punto di trigger di fc MIN
  motore.move(BACKOFF);           // libera lo switch
  motore.runToPosition();
  Serial.println(F("Homing OK, via al ping-pong"));

  // --- Partenza verso fc MAX ---
  direzione = +1;
  vel = VEL_BASE;
  motore.setMaxSpeed(vel);
  motore.move(CORSA_LUNGA);
}

void vaiAlCentro() {
  long centro = posMax / 2;
  Serial.print(F("Corsa misurata: "));
  Serial.print(posMax);
  Serial.print(F(" step. Vado al centro: "));
  Serial.println(centro);

  motore.setMaxSpeed(VEL_BASE * 2);  // ritorno a velocita' tranquilla
  motore.moveTo(centro);
  motore.runToPosition();

  delay(300);
  digitalWrite(PIN_EN, HIGH);  // disabilita il driver: niente corrente di mantenimento
  Serial.println(F("=== FERMO AL CENTRO, DRIVER DISABILITATO ==="));
  fase = FINE;
}

void loop() {
  if (fase != PING_PONG) return;

  // Controlla solo il finecorsa verso cui stiamo andando
  bool trigger = (direzione > 0 && fcMaxPremuto()) ||
                 (direzione < 0 && fcMinPremuto());

  if (trigger) {
    stopSecco();

    // Alla prima toccata di fc MAX misuriamo la corsa
    if (direzione > 0 && posMax < 0) posMax = motore.currentPosition();

    inversioni++;
    if (inversioni >= N_INVERSIONI) {
      vaiAlCentro();
      return;
    }

    vel = min(vel * 2, VEL_MAX);
    Serial.print(F("Inversione "));
    Serial.print(inversioni);
    Serial.print(F("/"));
    Serial.print(N_INVERSIONI);
    Serial.print(F(" -> velocita': "));
    Serial.println(vel);

    direzione = -direzione;
    motore.setMaxSpeed(vel);
    motore.move(direzione * BACKOFF);   // libera lo switch
    motore.runToPosition();
    motore.move(direzione * CORSA_LUNGA);
    return;
  }

  motore.run();
}