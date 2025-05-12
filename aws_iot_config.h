#ifndef AWS_IOT_CONFIG_H
#define AWS_IOT_CONFIG_H

#include <Arduino.h> 

// ========= CONFIGURACIÓN WIFI =========

extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// ========= CONFIGURACIÓN AWS IOT CORE =========

#define AWS_IOT_ENDPOINT "a2ji0g9lxroj8h-ats.iot.us-east-2.amazonaws.com"
#define THING_NAME "objeto_plantita_feliz"

// ========= CERTIFICADOS =========

extern const char AWS_ROOT_CA[] PROGMEM;
extern const char DEVICE_CERTIFICATE[] PROGMEM;
extern const char DEVICE_PRIVATE_KEY[] PROGMEM;

// ========= PIN CONFIGURATION =========
#define SOIL_MOISTURE_PIN 32
#define SERVO_PIN 18


extern const int SOIL_DRY_VALUE;
extern const int SOIL_WET_VALUE;

// Servo angles
extern const int SERVO_SAD_ANGLE;
extern const int SERVO_HAPPY_ANGLE;
extern const int SERVO_NEUTRAL_ANGLE;

#endif 