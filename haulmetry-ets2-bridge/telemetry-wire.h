#pragma once

#include "telemetry-model.h"

#include <sstream>
#include <string>


inline constexpr const char* CONTROL_PAUSED =
"CONTROL|PAUSED";

inline constexpr const char* CONTROL_RESUMED =
"CONTROL|RESUMED";

inline constexpr const char* CONTROL_STOPPED =
"CONTROL|STOPPED";


// ==================================================
// TelemetryData -> UDP payload
// ==================================================

inline std::string serializeTelemetry(
    const TelemetryData& telemetry
)
{
    return
        telemetry.truckId + "|" +
        telemetry.sessionId + "|" +
        std::to_string(telemetry.speed) + "|" +
        std::to_string(telemetry.rpm) + "|" +
        std::to_string(telemetry.fuel) + "|" +
        std::to_string(telemetry.gear) + "|" +
        std::to_string(telemetry.sequenceNumber);
}


// ==================================================
// UDP payload -> TelemetryData
// ==================================================

inline bool deserializeTelemetry(
    const std::string& payload,
    TelemetryData& telemetry
)
{
    std::istringstream stream(payload);

    std::string truckId;
    std::string sessionId;
    std::string speed;
    std::string rpm;
    std::string fuel;
    std::string gear;
    std::string sequenceNumber;


    if (!std::getline(stream, truckId, '|') ||
        !std::getline(stream, sessionId, '|') ||
        !std::getline(stream, speed, '|') ||
        !std::getline(stream, rpm, '|') ||
        !std::getline(stream, fuel, '|') ||
        !std::getline(stream, gear, '|') ||
        !std::getline(stream, sequenceNumber))
    {
        return false;
    }


    if (truckId.empty() ||
        sessionId.empty())
    {
        return false;
    }


    try
    {
        telemetry.truckId =
            truckId;

        telemetry.sessionId =
            sessionId;

        telemetry.speed =
            std::stod(speed);

        telemetry.rpm =
            std::stoi(rpm);

        telemetry.fuel =
            std::stod(fuel);

        telemetry.gear =
            std::stoi(gear);

        telemetry.sequenceNumber =
            std::stoll(sequenceNumber);
    }
    catch (...)
    {
        return false;
    }


    return true;
}