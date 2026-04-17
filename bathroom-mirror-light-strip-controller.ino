const int pinR = 11;
const int pinG = 9;
const int pinB = 10;
const int pinW = 6;

const int buttonPin = 8;

int mode = 0;
int lastState = HIGH;

void setup() {
  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinW, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
}

void loop() {
  int state = digitalRead(buttonPin);

  if (lastState == HIGH && state == LOW) {
    mode++;
    if (mode > 3) mode = 0;
    delay(200);
  }

  lastState = state;

  switch (mode) {
    case 0: setColor(0, 5, 255, 160); break;
    case 1: setColor(0, 3, 153, 96); break;
    case 2: setColor(0, 0, 0, 180); break;
    case 3: setColor(0, 0, 0, 89); break;
  }
}

void setColor(int r, int g, int b, int w) {
  analogWrite(pinR, r);
  analogWrite(pinG, g);
  analogWrite(pinB, b);
  analogWrite(pinW, w);
}