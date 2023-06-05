//I normally look down upon using Arduino at a college level
//I mainly chose to use it here because I needed something quick and dirty that fit on a half size breadboard
//Also, look how simple this code is

#define KEYER_PIN 2

void setup() {
  Serial.begin(9600);
  pinMode(KEYER_PIN, OUTPUT);
  digitalWrite(KEYER_PIN, 0); //key down
}

void loop() {
  char serial_in;

  if (Serial.available() > 0) {
    serial_in = Serial.read();
    if (serial_in == '1') {
      digitalWrite(KEYER_PIN, 1); //key up radio
    }
    else if (serial_in == '0') {
      digitalWrite(KEYER_PIN, 0); //key down
    }
  }
}