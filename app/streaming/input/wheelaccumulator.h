#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

class WheelAccumulator
{
public:
    short take(float delta, double unitsPerStep = 120.0)
    {
        if (!std::isfinite(delta)) return 0;
        const double units = std::clamp(double(delta) * unitsPerStep + m_Remainder,
                                       double(std::numeric_limits<short>::min()),
                                       double(std::numeric_limits<short>::max()));
        const auto whole = static_cast<short>(std::trunc(units));
        m_Remainder = units - whole;
        return whole;
    }
private:
    double m_Remainder = 0.0;
};
