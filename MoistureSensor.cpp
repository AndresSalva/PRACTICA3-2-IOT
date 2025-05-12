#include "MoistureSensor.h"

MoistureSensor::MoistureSensor(int pin, int dryValue, int wetValue)
  : _pin(pin), _dryValue(dryValue), _wetValue(wetValue),
    _rawValue(0), _percentage(0), _currentRange(RANGE_UNKNOWN) {
    pinMode(_pin, INPUT);
}

void MoistureSensor::update() {
  _rawValue = analogRead(_pin);
  _percentage = map(_rawValue, _dryValue, _wetValue, 0, 100);
  _percentage = constrain(_percentage, 0, 100);
  determineRange();
}

int MoistureSensor::getRawValue() const { return _rawValue; }
int MoistureSensor::getPercentage() const { return _percentage; }
HumidityRange MoistureSensor::getCurrentRange() const { return _currentRange; }

void MoistureSensor::determineRange() {
    if (_percentage <= 20) {
        _currentRange = RANGE_VERY_DRY;
    } else if (_percentage <= 40) {
        _currentRange = RANGE_DRY;
    } else if (_percentage <= 70) {
        _currentRange = RANGE_OPTIMAL;
    } else if (_percentage <= 90) {
        _currentRange = RANGE_WET;
    } else {
        _currentRange = RANGE_VERY_WET;
    }
}
String MoistureSensor::getRangeString() const {
    return MoistureSensor::rangeToString(_currentRange);
}

String MoistureSensor::rangeToString(HumidityRange range) {
    switch (range) {
        case RANGE_VERY_DRY: return "MUY_SECO";
        case RANGE_DRY: return "SECO";
        case RANGE_OPTIMAL: return "OPTIMO";
        case RANGE_WET: return "HUMEDO";
        case RANGE_VERY_WET: return "MUY_HUMEDO";
        default: return "DESCONOCIDO";
    }
}

HumidityRange MoistureSensor::stringToRange(const String& rangeStr) {
    if (rangeStr == "MUY_SECO") return RANGE_VERY_DRY;
    if (rangeStr == "SECO") return RANGE_DRY;
    if (rangeStr == "OPTIMO") return RANGE_OPTIMAL;
    if (rangeStr == "HUMEDO") return RANGE_WET;
    if (rangeStr == "MUY_HUMEDO") return RANGE_VERY_WET;
    return RANGE_UNKNOWN;
}