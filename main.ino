#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include "mbedtls/md.h"
#include <ArduinoJson.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

//-------------CHANGE ME-------------//
#define DIGIT_ON HIGH //HIGH for common anode displays
#define DIGIT_OFF LOW //LOW for common anode displays
#define SEGMENT_ON LOW //Opposite of DIGIT_ON to allow current to flow through LED
#define SEGMENT_OFF HIGH //Opposite of DIGIT_OFF
#define DISPLAY_BRIGHTNESS 750 //Time in us that each segment is kept lit

int digit1 = 32; //Most significant digit / furthest left
int digit2 = 33; //Second left
int digit3 = 25; //Second right
int digit4 = 26; //Least significant digit / furthest right

const char * APIKEY = ""; //INSERT BINANCE APIKEY
const char * APISECRET = ""; //INSERT BINANCE APISECRET

const char * ntpServer = "pool.ntp.org"; //NTP Server
const long gmtOffset_sec = 0; //Keep 0 for UTC / GMT
const int daylightOffset_sec = 3600; //Daylight offset in seconds (3600 = 1 hour)

const char * ssid = ""; //INSERT SSID
const char * password = ""; //INSERT PSK / PASSWORD

int segA = 19; //PIN for each segment
int segB = 18;
int segC = 5;
int segD = 17;
int segE = 16;
int segF = 4;
int segG = 0;
//---------END OF CHANGE ME---------//

boolean duiz = false;
boolean hon = false;

TaskHandle_t screen;
TaskHandle_t apiUpdate;

float futuresBal = 0;
float spotBal = 0;

long getSeconds() { //Gets the current time in miliseconds. (Synced on boot from NTP server)
    struct timeval tnow;
    gettimeofday( & tnow, NULL);
    return (tnow.tv_sec);
}

void calcHMAC(const char * payload,
    const char * key, byte * sig) { //Calculates SHA-256 HMAC using payload and key
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const size_t payloadLength = strlen(payload);
    const size_t keyLength = strlen(key);
    mbedtls_md_init( & ctx);
    mbedtls_md_setup( & ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts( & ctx, (const unsigned char * ) key, keyLength);
    mbedtls_md_hmac_update( & ctx, (const unsigned char * ) payload, payloadLength);
    mbedtls_md_hmac_finish( & ctx, sig);
    mbedtls_md_free( & ctx);
}

void toHex(byte * raw, char * out) { //Gets printable HEX values of byte array
    for (int i = 0; i < 32; i++) {
        sprintf(out + i * 2, "%02x", (int) raw[i]);
    }
}

float getFuturesBal() { //Gets the total futures wallet balance (USDT ONLY)
    HTTPClient http;
    String req = String("timestamp=") + getSeconds() + "000&recvWindow=60000"; //000 added for rough conversion to miliseconds 
    byte raw[32];
    char sign[65];
    calcHMAC(req.c_str(), APISECRET, raw);
    toHex(raw, sign);
    String url = String("https://fapi.binance.com/fapi/v2/account?") + req + "&signature=" + sign;
    http.useHTTP10(true);
    http.begin(url);
    http.addHeader("X-MBX-APIKEY", APIKEY);
    int rc = http.GET();
    delay(100);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getStream());
    http.end();
    float bal = doc["totalWalletBalance"].as < float > ();
    return (bal);
}

float getSpotBal() { //Gets the total spot wallet balance (BTC ONLY)
    HTTPClient http;
    String req = String("timestamp=") + getSeconds() + "000&recvWindow=60000"; //000 added for rough conversion to miliseconds
    byte raw[32];
    char sign[65];
    calcHMAC(req.c_str(), APISECRET, raw);
    toHex(raw, sign);
    String url = String("https://api.binance.com/api/v3/account?") + req + "&signature=" + sign;
    http.useHTTP10(true);
    http.begin(url);
    http.addHeader("X-MBX-APIKEY", APIKEY);
    int rc = http.GET();
    delay(100);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getStream());
    http.end();
    for (int i = 0; i < doc["balances"].size(); i++) {
        if (!strcmp(doc["balances"][i]["asset"].as < const char* > (), "BTC")) {
            float bal = doc["balances"][i]["free"].as < float > ();
            http.begin("https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT");
            rc = http.GET();
            delay(100);
            deserializeJson(doc, http.getStream());
            http.end();
            float rate = doc["price"].as < float > ();
            return (bal * rate);
        }
    }
    return (0);
}

void setup() {
    WiFi.begin(ssid, password);
    pinMode(segA, OUTPUT);
    pinMode(segB, OUTPUT);
    pinMode(segC, OUTPUT);
    pinMode(segD, OUTPUT);
    pinMode(segE, OUTPUT);
    pinMode(segF, OUTPUT);
    pinMode(segG, OUTPUT);

    pinMode(digit1, OUTPUT);
    pinMode(digit2, OUTPUT);
    pinMode(digit3, OUTPUT);
    pinMode(digit4, OUTPUT);
    Serial.begin(115200);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("LOADED");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(2000);
    xTaskCreatePinnedToCore(
        doScreen,
        "Screen",
        10000,
        NULL,
        5,
        &screen,
        0);
    xTaskCreatePinnedToCore(
        updateBals,
        "apiUpdate",
        10000,
        NULL,
        6,
        &apiUpdate,
        1);
}

void doScreen(void * pvParameters) { //Writes spotBal and futuresBal to segment display
    int i;
    for (;;) {
        TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE; //Feeds the watchdog
        TIMERG0.wdt_feed = 1;
        TIMERG0.wdt_wprotect = 0;
        yield();
        i = spotBal;
        duiz = false;
        hon = false;
        for (int k = 0; k < 2000; k++) {
            if (k > 1000) i = futuresBal;
            int figure = i;
            for (int digit = 1; digit < 5; digit++) { //Places the number in the right digit
                //Turn on the digit for a short amount of time
                switch (digit) {
                case 1:
                    if (figure > 999) {
                        digitalWrite(digit1, DIGIT_ON);
                        lightNumber(figure / 1000);
                        figure %= 1000;
                        delayMicroseconds(DISPLAY_BRIGHTNESS);
                        yield();
                        if (figure < 100) {
                            duiz = true;
                            if (figure < 10) {
                                hon = true;
                            }
                        } else duiz = false;
                    }
                    break;

                case 2:
                    if (duiz == true) {
                        digitalWrite(digit2, DIGIT_ON);
                        lightNumber(0);
                        delayMicroseconds(DISPLAY_BRIGHTNESS);
                        yield();
                    }
                    if (hon == true) break;

                    if (figure > 99 && figure < 1000) {
                        digitalWrite(digit2, DIGIT_ON);
                        lightNumber(figure / 100);
                        figure %= 100;
                        delayMicroseconds(DISPLAY_BRIGHTNESS);
                        yield();
                        if (figure < 10) {
                            hon = true;
                        } else hon = false;
                    }
                    break;

                case 3:
                    if (hon == true) {
                        digitalWrite(digit3, DIGIT_ON);
                        lightNumber(0);
                        delayMicroseconds(DISPLAY_BRIGHTNESS);
                        yield();
                        break;
                    }
                    if (figure > 9 && figure < 100) {
                        digitalWrite(digit3, DIGIT_ON);
                        lightNumber(figure / 10);
                        figure %= 10;
                        delayMicroseconds(DISPLAY_BRIGHTNESS);
                        yield();
                    }
                    break;

                case 4:
                    if (figure < 10) {
                        digitalWrite(digit4, DIGIT_ON);
                        lightNumber(figure);
                        delayMicroseconds(DISPLAY_BRIGHTNESS);
                        break;
                    }
                }
                //Turn off all segments
                lightNumber(10);

                //Turn off all digits
                digitalWrite(digit1, DIGIT_OFF);
                digitalWrite(digit2, DIGIT_OFF);
                digitalWrite(digit3, DIGIT_OFF);
                digitalWrite(digit4, DIGIT_OFF);
            }
        }
    }
}

void updateBals(void * pvParameters) { //Updates balances every 15 seconds
    TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
    TIMERG0.wdt_feed = 1;
    TIMERG0.wdt_wprotect = 0;
    for (;;) {
        spotBal = getSpotBal();
        futuresBal = getFuturesBal();
        delay(15000);
    }
}

void loop() {
    vTaskDelete(NULL);
    yield();
}

void lightNumber(int numberToDisplay) {
    switch (numberToDisplay) {
    case 0:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_ON);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_OFF);
        break;

    case 1:
        digitalWrite(segA, SEGMENT_OFF);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_OFF);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_OFF);
        break;

    case 2:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_OFF);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_ON);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_ON);
        break;

    case 3:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_ON);
        break;

    case 4:
        digitalWrite(segA, SEGMENT_OFF);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_OFF);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;

    case 5:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_OFF);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;

    case 6:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_OFF);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_ON);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;

    case 7:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_OFF);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_OFF);
        break;

    case 8:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_ON);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;

    case 9:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;

    case 10:
        digitalWrite(segA, SEGMENT_OFF);
        digitalWrite(segB, SEGMENT_OFF);
        digitalWrite(segC, SEGMENT_OFF);
        digitalWrite(segD, SEGMENT_OFF);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_OFF);
        break;
    }
}
