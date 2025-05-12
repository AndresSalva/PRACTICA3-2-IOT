#include "EmotionalServo.h"
#include "aws_iot_config.h" 
#include <Arduino.h>

EmotionalServo::EmotionalServo(int pin) : _pin(pin), _currentAngle(SERVO_NEUTRAL_ANGLE) {

}

void EmotionalServo::attach() {
    _servo.attach(_pin);
    setAngle(_currentAngle); 
}

void EmotionalServo::setHappy() {
  setAngle(SERVO_HAPPY_ANGLE);
}

void EmotionalServo::setSad() {
  setAngle(SERVO_SAD_ANGLE);
}

void EmotionalServo::setNeutral() {
  setAngle(SERVO_NEUTRAL_ANGLE);
}

void EmotionalServo::setAngle(int angle) {
  _currentAngle = constrain(angle, 0, 180);
  _servo.write(_currentAngle);
  Serial.print("Servo angle set to: ");
  Serial.println(_currentAngle);
}

int EmotionalServo::getCurrentAngle() const {
  return _currentAngle;
}