#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WifiManager.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#define LED_PIN 16
#define COLOR_ORDER NEO_GRB
#define LED_COUNT 19

#define DELAYVAL 30

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, COLOR_ORDER + NEO_KHZ800);
WebSocketsClient webSocket;
JsonDocument doc;
char *message;

// | Mode | Description                                     |
// | ---- | ----------------------------------------------- |
// | 0    | Default, Off                                    |
// | 1    | Rainbow (Individual)                            |
// | 2    | Solid colour (Primary)                          |
// | 3    | Breathing (Primary, Seconary)                   |
// | 4    | Freaky - Breathing (Red, Purple)                |
// | 5    | Party - Rainbow flashing                        |
// | 6    | Switch - Switches between Primary and Secondary |
// | 7    | Strobe - Switches between Primary and Off       |

struct State
{
  uint8_t primary[3] = {255, 255, 255};
  uint8_t secondary[3] = {0, 0, 0};
  uint8_t mode = 3;
  uint8_t brightness = 50;
  int speed = 1000;
};

int disconnectedTime;
bool disconnected = false;
bool fadeIn = true;
State state;
float animProgress;
State previousState;
int lastUpdate = 0; // millis
int colorOffset = 10;

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case WStype_DISCONNECTED:
    disconnected = millis();
    disconnected = true;
    break;
  case WStype_CONNECTED:
    if (!disconnected || millis() - disconnectedTime > 15000)
    {
      for (int i = 0; i < LED_COUNT; i++)
      {
        strip.setPixelColor(i, strip.Color(255, 0, 255));
        strip.show();
        delay(DELAYVAL);
      }
      // flash();
    }
    disconnected = false;

    webSocket.sendTXT("Connected");
    break;
  case WStype_TEXT:
    // flash();
    // Serial.println("Received: " + String((char *)payload)); // {"primary":[0,0,0],"secondary":[0,0,0],"mode":"2","brightness":100}
    message = (char *)payload;

    deserializeJson(doc, message);
    state.primary[0] = doc["primary"][0];
    state.primary[1] = doc["primary"][1];
    state.primary[2] = doc["primary"][2];
    state.secondary[0] = doc["secondary"][0];
    state.secondary[1] = doc["secondary"][1];
    state.secondary[2] = doc["secondary"][2];
    state.mode = doc["mode"];
    state.brightness = doc["brightness"];
    state.speed = doc["speed"];

    break;
  default:
    break;
  }
}

uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85)
  {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170)
  {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void flash()
{
  for (int j = 0; j < 3; j++)
  {
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(255, 255, 255));
      strip.show();
    }
    delay(DELAYVAL);
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
      strip.show();
    }
    delay(DELAYVAL);
  }
}

void updateLeds()
{
  if (state.mode != previousState.mode || state.primary[0] != previousState.primary[0] || state.primary[1] != previousState.primary[1] || state.primary[2] != previousState.primary[2] || state.secondary[0] != previousState.secondary[0] || state.secondary[1] != previousState.secondary[1] || state.secondary[2] != previousState.secondary[2] || state.brightness != previousState.brightness || state.speed != previousState.speed)
  {
    strip.setBrightness(state.brightness);
    previousState = state;
  }

  animProgress = millis() % state.speed / (float)state.speed;
  // Serial.println(animProgress);
  switch (state.mode)
  {
  case 0:
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    break;
  case 1:
    for (int i = 0; i < LED_COUNT; i++)
    {
      // 0 deg -> 255 0 0, 60 deg -> 255 255 0, 120 deg -> 0 255 0, 180 deg -> 0 255 255, 240 deg -> 0 0 255, 300 deg -> 255 0 255
      // float angle = (animProgress * 360 + colorOffset * i) * PI / 180;
      // strip.setPixelColor(i, strip.Color(255 * cos(angle), 255 * cos(angle + (2 * PI / 3)), 255 * cos(angle - (2 * PI / 3))));

      strip.setPixelColor(i, Wheel((int)(animProgress * 255 + colorOffset * i) % 255));
    }
    break;
  case 2:
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(state.primary[0], state.primary[1], state.primary[2]));
    }
    break;
  case 3:
    // breathing (sine wave)
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(state.primary[0] * sin(animProgress * PI), state.primary[1] * sin(animProgress * PI), state.primary[2] * sin(animProgress * PI)));
    }
    break;
  case 4:
    // freaky breathing (sine wave between 255, 0, 136 and 255, 0, 0)
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(150 + 50 * sin(animProgress * PI), 0, 136 * sin(animProgress * PI)));
    }
  case 5:
    // party - rainbow flashing
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, Wheel((int)(animProgress * 255 + colorOffset * i) % 255));
    }
    break;
  case 6:
    // switch between primary and secondary
    if (animProgress < 0.5)
    {
      for (int i = 0; i < LED_COUNT; i++)
      {
        strip.setPixelColor(i, strip.Color(state.primary[0], state.primary[1], state.primary[2]));
      }
    }
    else
    {
      for (int i = 0; i < LED_COUNT; i++)
      {
        strip.setPixelColor(i, strip.Color(state.secondary[0], state.secondary[1], state.secondary[2]));
      }
    }
    break;
  case 7:
    // strobe - switch between primary and off
    if (animProgress < 0.5)
    {
      for (int i = 0; i < LED_COUNT; i++)
      {
        strip.setPixelColor(i, strip.Color(state.primary[0], state.primary[1], state.primary[2]));
      }
    }
    else
    {
      for (int i = 0; i < LED_COUNT; i++)
      {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
      }
    }
    break;
  }

  // lastUpdate = millis();
  strip.show();
}

void setup()
{
  Serial.begin(115200);

  strip.begin();
  strip.show();
  strip.setBrightness(50);

  for (int i = 0; i < LED_COUNT; i++)
  {
    strip.setPixelColor(i, strip.Color(0, 150, 0));
    strip.show();
    delay(DELAYVAL);
  }

  WiFiManager wm;
  bool res;
  res = wm.autoConnect("lights", "InvalidSE");

  if (!res)
  {
    Serial.println("Failed to connect");
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
      strip.show();
      delay(DELAYVAL);
    }
  }
  else
  {
    Serial.println("Connected");
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(0, 0, 255));
      strip.show();
      delay(DELAYVAL);
    }
  }

  webSocket.begin("invalidse-wifi-lights.host.qrl.nz", 80);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop()
{
  webSocket.loop();
  if (disconnected && millis() - disconnectedTime > 15000)
  {
    for (int i = 0; i < LED_COUNT; i++)
    {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
      strip.show();
      delay(DELAYVAL);
    }
  }
  else
  {
    updateLeds();
  }
}
