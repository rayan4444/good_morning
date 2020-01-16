#include "Arduino.h"
#include <WiFi.h>

#define DEBUG

// time in between boot cycles
#define uS_TO_S_FACTOR 1000000  //Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  30        //Time ESP32 will go to sleep (in seconds)

//pinout
#define led 13
#define step 16
#define dir 17
#define _ena 4
#define sleep 5
#define lim_1 14
#define lim_2 12
#define light_int 21
#define button 15 //also RTC_GPIO13
#define vbat 35

//flags
volatile bool button_pressed = 0;
volatile bool lim_1_reached = 0; //lim 1 reached means the curtain is fully open
volatile bool lim_2_reached = 0; //lim 2 reached means the curtain is fully closed
//WiFi credentials
const char* ssid = "HAXhome";
const char* password = "Shenzhen2018";

//varialbles for time
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800; //China is UTC+8 so offset: 8*3600 = 28800
const int daylightOffset_sec = 0; // China doesn't do Dailight saving so 0

//curtain open time
struct quand_t {
    int open_hour = 2;
    int open_min = 0;
    int open_late_hour = 10;
    int open_late_min = 30;
    int close_hour = 18;
    int close_min = 0;
} curtain;

// curtain status
typedef enum {
    FULLY_CLOSED = 0,
    FULLY_OPEN = 1,
    IN_BETWEEN = 2,
} status_t;

//counting how many times it boots (testing purposes only)
RTC_DATA_ATTR int bootCount = 0;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    //print bootCount
    // ++bootCount;
    // Serial.print("Boot Count: ");
    // Serial.println(bootCount);

    //check wake up reason 
	print_wakeup_reason();

    //configure GPIO
    pinMode(led, OUTPUT);
    pinMode(step, OUTPUT);
    pinMode(dir, OUTPUT);
    pinMode(_ena, OUTPUT);
    pinMode(sleep, OUTPUT);
    pinMode(lim_1, INPUT);
    pinMode(lim_2, INPUT_PULLUP);
    pinMode(button, INPUT_PULLUP);
    pinMode(vbat, INPUT);

    //interrupts
    attachInterrupt(digitalPinToInterrupt(button), button_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(lim_1), lim_1_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(lim_2), lim_2_isr, FALLING);

    //setup stepper
    digitalWrite(_ena, LOW);
    digitalWrite(dir, HIGH);
    digitalWrite(sleep, HIGH);

    //if battery low, wake up every 5 seconds and blink the light but don't proceed
	// if (vbat_low){
    //     digitalWrite(led, HIGH);
    //     delay(1000);
    //     //Configure ESP wake up timer to 5s
    //     esp_sleep_enable_timer_wakeup(5 * uS_TO_S_FACTOR);
    //     //go back to sleep. the rest of the setup function will not run. 
    //     esp_deep_sleep_start();
    // }
    
    //connect to WiFi
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" CONNECTED");

    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    //handle the curtains
    curtaintime();

    //disconnect WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

	//Configure ESP wake up timer to 5s
	esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
	Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
	" Seconds");

    //Configure GPIO33 as ext0 wake up source for LOW logic level
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_15,0);

	//Go to sleep now
	esp_deep_sleep_start();
}

void loop()
{

}

void button_isr()
{
    // triggers when button is pressed
    button_pressed = 1;
}

void lim_1_isr()
{
    //triggers when limit switch 1 is touched
    lim_1_reached = 1;
}

void lim_2_isr()
{
    //triggers when limit switch 2 is touched
    lim_2_reached = 1;
}

bool vbat_low()
{
    //true if the battery voltage is too low
    /* resistor divider 
    R1= 100k
    R2=34k
    (R2/(R1/R2))= 0.2537
    */

    int adc_raw = analogRead(vbat);
    float vref = 3.3;
    float voltage = vref * adc_raw;
    voltage = voltage / 4096.0;
    voltage = voltage / 0.237;

    // Serial.print("Battery voltage: ");
    // Serial.println(voltage);

    if (voltage < 9.0) {
        Serial.println("Battery low");
        return 1;
    } else {
        return 0;
    }
}

void curtaintime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    int opening_hour, opening_min;
    // check opening time
    if (timeinfo.tm_wday == 0) {
        opening_hour = curtain.open_late_hour;
        opening_min = curtain.open_late_min;
    } else {
        opening_hour = curtain.open_hour;
        opening_min = curtain.open_min;
    }
    //check if is time to open
    if (timeinfo.tm_hour==opening_hour) {
        //check minutes
        if (timeinfo.tm_min >= curtain.open_min) {
            // if the curtain opening time has passed and the curtains are not already fully open, open them
            if (curtain_status() != FULLY_OPEN) {
                open_curtains();
            }
        }
    }
    //check if it is time to close 
    if(timeinfo.tm_hour == curtain.close_hour){
        if (timeinfo.tm_min >= curtain.close_min) {
            // if the curtain closign time has passed and the curtains are not already fully closed, close them
            if (curtain_status() != FULLY_CLOSED){
                close_curtains();
            }
        }
    }
}

status_t curtain_status()
{
    // if limit switch 2 is pressed, the curtain is fully closed
    if (digitalRead(lim_2) == 0) {
        Serial.println("Curtains fully closed");
        return FULLY_CLOSED;
    } else {
        //if limit switch 1 is pressd, the curetain is fully open
        if (digitalRead(lim_1) == 0) {
            Serial.println("Curtains fully open");
            return FULLY_OPEN;
        } else {
            // the curtain is partially open
            Serial.println("Curtains in between");
            return IN_BETWEEN;
        }
    }
}

void open_curtains(){
    Serial.println("curtains opening");
}

void close_curtains(){
    Serial.println("curtains closing");
}

void print_wakeup_reason(){
	esp_sleep_wakeup_cause_t wakeup_reason;
	wakeup_reason = esp_sleep_get_wakeup_cause();
	switch(wakeup_reason)
	{
		case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
		case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
		case 3  : Serial.println("Wakeup caused by timer"); break;
		case 4  : Serial.println("Wakeup caused by touchpad"); break;
		case 5  : Serial.println("Wakeup caused by ULP program"); break;
		default : Serial.println("Wakeup was not caused by deep sleep"); break;
	}
}