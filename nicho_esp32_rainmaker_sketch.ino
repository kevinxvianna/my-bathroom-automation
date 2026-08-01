#include <Arduino.h>
#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"

constexpr uint8_t PIN_R = 4;
constexpr uint8_t PIN_G = 13;
constexpr uint8_t PIN_B = 12;
constexpr uint8_t PIN_W = 16; // RX2

constexpr uint8_t PIN_SWITCH = 27;

constexpr uint8_t CH_R = 0;
constexpr uint8_t CH_G = 1;
constexpr uint8_t CH_B = 2;
constexpr uint8_t CH_W = 3;

constexpr uint32_t PWM_FREQ = 5000;
constexpr uint8_t PWM_RESOLUTION = 8;
constexpr uint32_t SWITCH_DEBOUNCE_MS = 50;

const char *serviceName = "PROV_NICHO_RGBW";
const char *pop = "";

bool powerState = false;
int brightness = 100;
int hue = 0;
int saturation = 100;

int lastSwitchReading = HIGH;
int stableSwitchReading = HIGH;
uint32_t switchChangedAt = 0;

static LightBulb nicheLight("Nicho RGBW", nullptr, false);

void writePwm(uint8_t channel, int value) {
  ledcWrite(channel, constrain(value, 0, 255));
}

void hsvToRgb(int h, int s, int v, int &r, int &g, int &b) {
  h = constrain(h, 0, 360);
  s = constrain(s, 0, 100);
  v = constrain(v, 0, 100);

  if (h == 360) {
    h = 0;
  }

  if (s == 0) {
    r = 0;
    g = 0;
    b = 0;
    return;
  }

  const float hf = h / 60.0f;
  const float sf = s / 100.0f;
  const float vf = v / 100.0f;
  const int sector = static_cast<int>(hf);
  const float fraction = hf - sector;
  const float p = vf * (1.0f - sf);
  const float q = vf * (1.0f - sf * fraction);
  const float t = vf * (1.0f - sf * (1.0f - fraction));

  float rf = 0;
  float gf = 0;
  float bf = 0;

  switch (sector % 6) {
    case 0: rf = vf; gf = t;  bf = p;  break;
    case 1: rf = q;  gf = vf; bf = p;  break;
    case 2: rf = p;  gf = vf; bf = t;  break;
    case 3: rf = p;  gf = q;  bf = vf; break;
    case 4: rf = t;  gf = p;  bf = vf; break;
    default: rf = vf; gf = p; bf = q;  break;
  }

  r = static_cast<int>(rf * 255.0f + 0.5f);
  g = static_cast<int>(gf * 255.0f + 0.5f);
  b = static_cast<int>(bf * 255.0f + 0.5f);
}

void applyColor() {
  if (!powerState) {
    writePwm(CH_R, 0);
    writePwm(CH_G, 0);
    writePwm(CH_B, 0);
    writePwm(CH_W, 0);
    return;
  }

  int r = 0;
  int g = 0;
  int b = 0;
  int w = 0;

  if (saturation == 0) {
    w = brightness * 255 / 100;
  } else {
    hsvToRgb(hue, saturation, brightness, r, g, b);
    w = 0;
  }

  writePwm(CH_R, r);
  writePwm(CH_G, g);
  writePwm(CH_B, b);
  writePwm(CH_W, w);

  Serial.printf(
    "HSV(%d,%d,%d) -> PWM R=%d G=%d B=%d W=%d\n",
    hue, saturation, brightness, r, g, b, w
  );
}

void rainMakerWriteCallback(Device *device, 
                            Param *param,
                            const param_val_t value, 
                            void *privateData,
                            write_ctx_t *ctx) {
  const char *paramName = param->getParamName();

  if (strcmp(paramName, ESP_RMAKER_DEF_POWER_NAME) == 0) {
    powerState = value.val.b;
  } else if (strcmp(paramName, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
    brightness = constrain(value.val.i, 0, 100);
  } else if (strcmp(paramName, ESP_RMAKER_DEF_HUE_NAME) == 0) {
    hue = constrain(value.val.i, 0, 360);
  } else if (strcmp(paramName, ESP_RMAKER_DEF_SATURATION_NAME) == 0) {
    saturation = constrain(value.val.i, 0, 100);
  }

  applyColor();
  param->updateAndReport(value);

  Serial.printf("RainMaker: %s atualizado\n", paramName);
}

void serviceSwitch() {
  const uint32_t now = millis();
  const int reading = digitalRead(PIN_SWITCH);

  if (reading != lastSwitchReading) {
    lastSwitchReading = reading;
    switchChangedAt = now;
  }

  if ((now - switchChangedAt >= SWITCH_DEBOUNCE_MS) &&
      (reading != stableSwitchReading)) {
    stableSwitchReading = reading;

    powerState = !powerState;
    applyColor();

    nicheLight.updateAndReportParam(
      ESP_RMAKER_DEF_POWER_NAME,
      powerState
    );

    Serial.printf(
      "Interruptor: %s\n",
      powerState ? "ligado" : "desligado"
    );
  }
}

void systemEvent(arduino_event_t *event) {
  switch (event->event_id) {
    case ARDUINO_EVENT_PROV_START:
      Serial.println("Provisionamento BLE iniciado");
      Serial.printf("Dispositivo: %s\n", serviceName);
      Serial.printf("PoP: %s\n", pop);
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("Wi-Fi conectado ao roteador");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      WiFi.setSleep(false);
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Serial.printf("Sinal Wi-Fi (RSSI): %d dBm\n", WiFi.RSSI());
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf(
        "Wi-Fi desconectado. Motivo: %d\n",
        event->event_info.wifi_sta_disconnected.reason
      );
      break;

    default:
      break;
  }
}

void setupPwm() {
  ledcSetup(CH_R, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_G, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_B, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_W, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(PIN_R, CH_R);
  ledcAttachPin(PIN_G, CH_G);
  ledcAttachPin(PIN_B, CH_B);
  ledcAttachPin(PIN_W, CH_W);

  applyColor();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  setupPwm();

  pinMode(PIN_SWITCH, INPUT_PULLUP);
  lastSwitchReading = digitalRead(PIN_SWITCH);
  stableSwitchReading = lastSwitchReading;
  switchChangedAt = millis();

  WiFi.onEvent(systemEvent);

  Node node = RMaker.initNode("ESP32 Nicho RGBW");

  nicheLight.addCb(rainMakerWriteCallback);
  nicheLight.addBrightnessParam(brightness);
  nicheLight.addHueParam(hue);
  nicheLight.addSaturationParam(saturation);

  node.addDevice(nicheLight);

  RMaker.start();

  WiFiProv.beginProvision(
    WIFI_PROV_SCHEME_BLE,
    WIFI_PROV_SCHEME_HANDLER_FREE_BTDM,
    WIFI_PROV_SECURITY_1,
    pop,
    serviceName
  );

  Serial.println("RainMaker iniciado");
}

void loop() {
  serviceSwitch();
  delay(5);
}
