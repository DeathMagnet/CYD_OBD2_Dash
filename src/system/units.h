#pragma once

#include "labels.h"

namespace units {

constexpr float kKphPerMph = 1.609344F;
constexpr float kKpaPerPsi = 6.89476F;
constexpr float kPsiPerKpa = 0.145038F;
constexpr float kInHgPerKpa = 0.2953F;

inline float celsiusFromFahrenheit(float f) {
    return (f - 32.0F) * 5.0F / 9.0F;
}

inline float kphFromMph(float mph) {
    return mph * kKphPerMph;
}

inline float mphFromKph(float kph) {
    return kph / kKphPerMph;
}

inline float kpaFromPsi(float psi) {
    return psi * kKpaPerPsi;
}

inline float psiFromKpa(float kpa) {
    return kpa * kPsiPerKpa;
}

inline float inHgFromKpa(float kpa) {
    return kpa * kInHgPerKpa;
}

// Snapshot/settings store speed in mph and temp in °F natively; pressure
// natively in kPa. These convert to whichever unit system `metric` selects and
// return the matching label, so call sites never hardcode a unit string.

inline float displaySpeed(float speedMph, bool metric) {
    return metric ? kphFromMph(speedMph) : speedMph;
}

inline const char* speedUnitLabel(bool metric) {
    return metric ? labels::kUnitKph : labels::kUnitMph;
}

inline float displayTemp(float tempF, bool metric) {
    return metric ? celsiusFromFahrenheit(tempF) : tempF;
}

inline const char* tempUnitLabel(bool metric) {
    return metric ? labels::kUnitCelsius : labels::kUnitFahrenheit;
}

inline float displayPressureFromKpa(float kpa, bool metric) {
    return metric ? kpa : psiFromKpa(kpa);
}

inline const char* pressureUnitLabel(bool metric) {
    return metric ? labels::kUnitKpa : labels::kUnitPsi;
}

} // namespace units
