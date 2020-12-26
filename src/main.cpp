#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <analogWrite.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

const bool VERBOSE = false;

// NETWORK CONFIG
const char *SSID = "WIFI";
const char *PASSWORD = "qazwsxedc";
const char *HOSTNAME = "ESP32-CLIENT-TEST";
const char *MQTT_HOST = "kaungpi";
const int MQTT_PORT = 1883;
WiFiClient wifiClient;

// RAND CONFIG
const char *ALPHANUMERIC = "abcdefghijklmnopqrstuvwxyz0123456789";
#define LENGTH_OF_RAND 10 // CHANGE EEPROM SIZE ACCORDINGLY

// MQTT CONFIG
#define MQTT_PAIR_TOPIC "devices/getOpenDevices"
#define MQTT_DEVICE_TOPIC_LENGTH 8 + LENGTH_OF_RAND // 10 + 8 = 18
const char *CLIENT_ID = "ESP32-A";
const char *MQTT_TOPIC = "test";
PubSubClient client(wifiClient);
char MQTT_GENERATED_ID[LENGTH_OF_RAND + 1];
char tempTopic[32];
char MQTT_DEVICE_TOPIC[MQTT_DEVICE_TOPIC_LENGTH + 1];
char MQTT_DEVICE_TOPIC_SUBSCRIBE[MQTT_DEVICE_TOPIC_LENGTH + 3];
bool pairingMode = false;
bool awaitingConfirmation = false;

// DEVICE CONFIG
const int PIN_LED_STRIP = 5;
const int PIN_RESET_BUTTON = 19;
int ledStripBrightness = 0;
const char *DEVICE_TYPE = "HOME32 LIGHT STRIP";
// EEPROM CONFIG
//// 0 - 9 stores randomly generated ID
//// 10 - 27 stores new MQTT Topic
#define EEPROM_SIZE LENGTH_OF_RAND + MQTT_DEVICE_TOPIC_LENGTH // 18 + 10 = 28

// Prototypes
void wifi_Init();
void mqtt_Init();
void mqtt_Reconnect();
void callback(const char *topic, byte *payload, unsigned int length);
char *convert_ByteToChar(const byte *b, const unsigned int l);

// Functions
void setup()
{
    if (VERBOSE)
    {
    }
    if (VERBOSE)
    {
        Serial.begin(115200);
    }
    randomSeed(analogRead(1));
    EEPROM.begin(EEPROM_SIZE); // bruh, dont forget this
    pinMode(PIN_LED_STRIP, OUTPUT);
    pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);
    if (VERBOSE)
    {
        Serial.println(digitalRead(PIN_RESET_BUTTON));
    }
    if (digitalRead(PIN_RESET_BUTTON) == 0)
    {
        // Button is pressed, start pairing sequence
        pairingMode = true;
        if (VERBOSE)
        {
            Serial.println("[SYS] Device is in pairing mode");
        }

        // New ID is generated and stored in EEPROM
        for (int i = 0; i < LENGTH_OF_RAND; i++)
        {
            char randChar = ALPHANUMERIC[random(0, 36)];
            MQTT_GENERATED_ID[i] = randChar;
            EEPROM.write(i, (int)randChar);
        }
        EEPROM.commit();
        MQTT_GENERATED_ID[LENGTH_OF_RAND] = '\0';
        if (VERBOSE)
        {
            Serial.print("[Pair] Generated new ID: ");
            Serial.println(MQTT_GENERATED_ID);
        }
    }
    else
    {
        // Button is not pressed, start normal boot up sequence

        // ID Stored in EEPROM is retrieved and put into MQTT_GENERATED_ID

        for (int i = 0; i < LENGTH_OF_RAND; i++)
        {
            MQTT_GENERATED_ID[i] = (char)EEPROM.read(i);
        }

        for (int i = 0; i < MQTT_DEVICE_TOPIC_LENGTH; i++)
        {
            MQTT_DEVICE_TOPIC[i] = MQTT_DEVICE_TOPIC_SUBSCRIBE[i] = (char)EEPROM.read(i + 10);
        }
        MQTT_DEVICE_TOPIC[MQTT_DEVICE_TOPIC_LENGTH] = '\0';
        MQTT_DEVICE_TOPIC_SUBSCRIBE[MQTT_DEVICE_TOPIC_LENGTH] = '/';
        MQTT_DEVICE_TOPIC_SUBSCRIBE[MQTT_DEVICE_TOPIC_LENGTH + 1] = '#';
        MQTT_DEVICE_TOPIC_SUBSCRIBE[MQTT_DEVICE_TOPIC_LENGTH + 2] = '\0';
        MQTT_GENERATED_ID[LENGTH_OF_RAND] = '\0';
        if (VERBOSE)
        {
            Serial.print("[SYS] Device ID: ");
            Serial.println(MQTT_GENERATED_ID);
        }
    }
    analogWriteResolution(8);
    wifi_Init();
    mqtt_Init();
    if (!client.connected())
    {
        mqtt_Reconnect();
    }
}

void loop()
{
    client.loop();
}

void wifi_Init()
{
    WiFi.mode(WIFI_MODE_STA);
    /*
    Available Modes
    WIFI_MODE_OFF - WiFi off
    WIFI_MODE_STA - WiFi station (hotspot)
    WIFI_MODE_AP - WiFi Access Point (client)
    WIFI_MODE_APSTA - Both AP and STA (self-explanitory)
    */
    WiFi.begin(SSID, PASSWORD);
    WiFi.setHostname(HOSTNAME);
    if (VERBOSE)
    {
        Serial.print("[SYS] Connecting to WiFi at ");
        Serial.print(SSID);
    }
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    if (VERBOSE)
    {
        Serial.println();
        Serial.print("[SYS] Connected to ");
        Serial.println(SSID);
    }
}

void mqtt_Init()
{
    client.setServer(MQTT_HOST, MQTT_PORT);
    client.setCallback(callback);
    if (client.connect(CLIENT_ID))
    {
        if (VERBOSE)
        {
            Serial.print("[SYS] Connected to MQTT Broker at ");
            Serial.print(MQTT_HOST);
            Serial.print(":");
            Serial.print(MQTT_PORT);
            Serial.println();
        }
    }
    if (pairingMode)
    {
        client.subscribe(MQTT_PAIR_TOPIC);
    }
    else
    {
        client.subscribe(MQTT_DEVICE_TOPIC_SUBSCRIBE);
        if (VERBOSE)
        {
            Serial.print("[SYS] Subscribed to ");
            Serial.println(MQTT_DEVICE_TOPIC_SUBSCRIBE);
        }
    }
}

void mqtt_Reconnect()
{
    if (VERBOSE)
    {
        Serial.print("[SYS] Connection to server lost. Reconnecting");
    }
    while (!client.connected())
    {
        Serial.print(".");
        if (client.connect(CLIENT_ID))
        {
            if (VERBOSE)
            {
                Serial.println();
                Serial.println("[SYS] Reconnected");
            }
            if (pairingMode)
            {
                client.subscribe(MQTT_PAIR_TOPIC);
            }
            else
            {
                client.subscribe(MQTT_DEVICE_TOPIC_SUBSCRIBE);
            }
        }
        else
        {
            if (VERBOSE)
            {
                Serial.println();
                Serial.print("[SYS] Failed. Reason = ");
                Serial.print(client.state());
                Serial.println("Retrying in 10 seconds...");
            }
            delay(3000);
        }
    }
}

void callback(const char *topic, byte *payload, unsigned int length)
{
    char *payload_char = convert_ByteToChar(payload, length);
    if (VERBOSE)
    {
        Serial.print("[");
        Serial.print(topic);
        Serial.print("] ");
        Serial.println(payload_char);
    }
    // LED Strip Adjustment
    // Generating topic for LED Strip control
    char ledTopic[MQTT_DEVICE_TOPIC_LENGTH + 5];
    strcpy(ledTopic, MQTT_DEVICE_TOPIC);
    strcat(ledTopic, "/led");
    if (strcmp(topic, ledTopic) == 0)
    {
        ledStripBrightness = atoi(payload_char);
        if (ledStripBrightness < 255 && ledStripBrightness >= 0)
        {
            if (VERBOSE)
            {
                Serial.print("[LED] Brightness set to ");
                Serial.println(ledStripBrightness);
            }
            analogWrite(PIN_LED_STRIP, ledStripBrightness);
        }
    }

    // Responding with device type and id when pairing
    if (strcmp(topic, MQTT_PAIR_TOPIC) == 0 && strcmp(payload_char, "check") == 0 && pairingMode) //  && !awaitingConfirmation
    {
        if (VERBOSE)
        {
            Serial.println("[Pair] Pair request received, responding");
        }
        StaticJsonDocument<JSON_OBJECT_SIZE(2)> doc;
        const char *constMQTTID = MQTT_GENERATED_ID; // ArduinoJSON is into consts
        doc["device_type"] = DEVICE_TYPE;
        doc["device_id"] = constMQTTID;
        char output[128];
        serializeJson(doc, output);
        client.publish(MQTT_PAIR_TOPIC, output);
        strcpy(tempTopic, "devices/");
        strcat(tempTopic, MQTT_GENERATED_ID);
        client.subscribe(tempTopic);
        if (VERBOSE)
        {
            Serial.print("[Pair] Subscribed to ");
            Serial.print(tempTopic);
            Serial.println(" and waiting for confirmation");
        }
        awaitingConfirmation = true;
    }

    // Confirming pairing and retrieving new mqtt channel for communication
    // First arg strncmp because different sizes of char array
    if (strcmp(topic, tempTopic) == 0 && strcmp(payload_char, "confirm") == 0 && pairingMode && awaitingConfirmation)
    {
        // Write new MQTT Topic to EEPROM
        for (int i = 0; i < MQTT_DEVICE_TOPIC_LENGTH; i++)
        {
            EEPROM.write(i + 10, (int)tempTopic[i]);
        }
        EEPROM.commit();
        if (VERBOSE)
        {
            Serial.println("[Pair] Device has been paired, please power cycle this device");
        }
    }
    free(payload_char);
}

char *convert_ByteToChar(const byte *b, const unsigned int l)
{
    char *temp;
    temp = (char *)malloc((l + 1) * sizeof(char));
    int i;
    for (i = 0; i < l; i++)
    {
        temp[i] = (char)b[i];
    }
    temp[i] = '\0';
    return temp;
}