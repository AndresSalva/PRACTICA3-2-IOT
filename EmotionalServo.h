#ifndef EmotionalServo_h
#define EmotionalServo_h

#include <ESP32Servo.h>

class EmotionalServo {
  public:
    EmotionalServo(int pin);
    void attach(); 
    void setHappy();
    void setSad();
    void setNeutral();
    void setAngle(int angle);
    int getCurrentAngle() const;
    
  private:
    Servo _servo;
    int _pin;
    int _currentAngle;
};

#endif