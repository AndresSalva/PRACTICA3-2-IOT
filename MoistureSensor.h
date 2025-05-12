#ifndef MoistureSensor_h
#define MoistureSensor_h

#include <Arduino.h>

enum HumidityRange {
    RANGE_UNKNOWN,
    RANGE_VERY_DRY,
    RANGE_DRY,
    RANGE_OPTIMAL,
    RANGE_WET,
    RANGE_VERY_WET
};

class MoistureSensor {
  public:
    MoistureSensor(int pin, int dryValue, int wetValue);
    void update();
    int getRawValue() const;
    int getPercentage() const;
    HumidityRange getCurrentRange() const;
    String getRangeString() const;
    static String rangeToString(HumidityRange range); 
    static HumidityRange stringToRange(const String& rangeStr); 

  private:
    int _pin;
    int _dryValue;
    int _wetValue;
    int _rawValue;
    int _percentage;
    HumidityRange _currentRange;
    void determineRange();
};

#endif