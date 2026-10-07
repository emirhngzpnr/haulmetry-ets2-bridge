#pragma once

#include <cstdint>
#include <string>

struct TelemetryData
{
    std::string truckId;
    std::string sessionId;

    double speed;
    int rpm;
    double fuel;
    int gear;

    std::int64_t sequenceNumber;
};