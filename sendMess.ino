#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

ESP8266WiFiMulti WiFiMulti;

// constants won't change. They're used here to set pin numbers:
const int rainPin = 4; // the number of the pushbutton pin
const int ledPin = 2;  // the number of the LED pin

// variables will change:
int rainState = 0; // variable for reading the pushbutton status
int lastState = 0;
String key = "";
String message = "C%E1%BA%A3nh%20b%C3%A1o!%0AC%E1%BB%ADa%20c%E1%BB%A7a%20b%E1%BA%A1n%20%C4%91ang%20b%E1%BB%8B%20x%C3%A2m%20nh%E1%BA%ADp!%0AVui%20l%C3%B2ng%20ki%E1%BB%83m%20tra%20email!%0A";

void setup()
{

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(rainPin, INPUT);

  for (uint8_t t = 4; t > 0; t--)
  {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("KemShop-55LTT", "");
}

void loop()
{
  // read the state of the pushbutton value:
  rainState = digitalRead(rainPin);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (rainState == 0 && lastState == 0)
  {
    Serial.printf("RAIN\n");
    lastState = 1;
    // turn LED on:
    digitalWrite(ledPin, HIGH);
    // wait for WiFi connection
    if ((WiFiMulti.run() == WL_CONNECTED))
    {

      std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

      //    client->setFingerprint(fingerprint);
      client->setInsecure();

      HTTPClient https;

      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, "https://taymay.herokuapp.com/send/?key=" + key + "&message=" + message))
      { // HTTPS

        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();

        // httpCode will be negative on error
        if (httpCode > 0)
        {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
          {
            String payload = https.getString();
            Serial.println(payload);
          }
        }
        else
        {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }

        https.end();
      }
      else
      {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
    }
  }
  else if (rainState == 1 && lastState == 1)
  {
    lastState = 0;
    // turn LED off:
    digitalWrite(ledPin, LOW);
  }

  delay(5000);
}