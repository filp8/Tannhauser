// BTT Octopus V1.1 (STM32F446) - primo test: LED STATUS su PA13
// Ritmo: 3 lampeggi rapidi + pausa (diverso dal firmware di fabbrica)
const uint32_t LED_STATUS = PA13;

void setup() {
  pinMode(LED_STATUS, OUTPUT);
}

void loop() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_STATUS, HIGH); delay(100);
    digitalWrite(LED_STATUS, LOW);  delay(150);
  }
  delay(1000);
}