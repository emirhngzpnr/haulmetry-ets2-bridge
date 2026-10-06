#include "scssdk_telemetry.h"
#include "eurotrucks2/scssdk_telemetry_eut2.h"

#include "telemetry-model.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>


// ==================================================
// Raw telemetry received directly from SCS SDK
// ==================================================

struct LiveTelemetry
{
    float speed = 0.0f;
    float rpm = 0.0f;
    float fuel = 0.0f;
    scs_s32_t gear = 0;
};


// ==================================================
// Plugin state
// ==================================================

static LiveTelemetry telemetry;

static scs_log_t gameLog = nullptr;

static auto lastLogTime =
std::chrono::steady_clock::now();

static std::int64_t sequenceNumber = 1;

static constexpr const char* TRUCK_ID =
"TRUCK-001";


// ==================================================
// Forward declarations
// ==================================================

SCSAPI_VOID telemetryFrameEnd(
    const scs_event_t event,
    const void* const eventInfo,
    const scs_context_t context
);

SCSAPI_VOID storeFloat(
    const scs_string_t name,
    const scs_u32_t index,
    const scs_value_t* const value,
    const scs_context_t context
);

SCSAPI_VOID storeS32(
    const scs_string_t name,
    const scs_u32_t index,
    const scs_value_t* const value,
    const scs_context_t context
);

TelemetryData mapTelemetry(
    const LiveTelemetry& source,
    std::int64_t sequence
);


// ==================================================
// Float channel callback
// ==================================================

SCSAPI_VOID storeFloat(
    const scs_string_t name,
    const scs_u32_t index,
    const scs_value_t* const value,
    const scs_context_t context
)
{
    if (value == nullptr || context == nullptr)
    {
        return;
    }

    auto* target =
        static_cast<float*>(context);

    *target = value->value_float.value;
}


// ==================================================
// Signed 32-bit channel callback
// ==================================================

SCSAPI_VOID storeS32(
    const scs_string_t name,
    const scs_u32_t index,
    const scs_value_t* const value,
    const scs_context_t context
)
{
    if (value == nullptr || context == nullptr)
    {
        return;
    }

    auto* target =
        static_cast<scs_s32_t*>(context);

    *target = value->value_s32.value;
}


// ==================================================
// Map raw SCS telemetry to Haulmetry telemetry model
// ==================================================

TelemetryData mapTelemetry(
    const LiveTelemetry& source,
    const std::int64_t sequence
)
{
    TelemetryData mappedTelemetry
    {
        TRUCK_ID,

        // SCS gives speed in m/s.
        // Backend expects km/h and a non-negative speed value.
        std::fabs(
            static_cast<double>(source.speed) * 3.6
        ),

        // SCS RPM is float.
        // Backend TelemetryRequest expects int.
        static_cast<int>(
            std::lround(source.rpm)
        ),

        static_cast<double>(
            source.fuel
        ),

        static_cast<int>(
            source.gear
        ),

        sequence
    };

    return mappedTelemetry;
}


// ==================================================
// Plugin initialization
// ==================================================

SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version,
    const scs_telemetry_init_params_t* const params
)
{
    if (version != SCS_TELEMETRY_VERSION_1_00)
    {
        return SCS_RESULT_unsupported;
    }

    if (params == nullptr)
    {
        return SCS_RESULT_generic_error;
    }

    const auto* versionParams =
        static_cast<
        const scs_telemetry_init_params_v100_t*
        >(params);


    gameLog = versionParams->common.log;


    if (gameLog == nullptr)
    {
        return SCS_RESULT_generic_error;
    }


    gameLog(
        SCS_LOG_TYPE_message,
        "Haulmetry telemetry plugin initialized."
    );


    // ==================================================
    // SPEED
    // ==================================================

    const scs_result_t speedResult =
        versionParams->register_for_channel(
            SCS_TELEMETRY_TRUCK_CHANNEL_speed,
            SCS_U32_NIL,
            SCS_VALUE_TYPE_float,
            SCS_TELEMETRY_CHANNEL_FLAG_none,
            storeFloat,
            &telemetry.speed
        );

    if (speedResult != SCS_RESULT_ok)
    {
        gameLog(
            SCS_LOG_TYPE_error,
            "Haulmetry could not register speed channel."
        );

        return SCS_RESULT_generic_error;
    }


    // ==================================================
    // RPM
    // ==================================================

    const scs_result_t rpmResult =
        versionParams->register_for_channel(
            SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm,
            SCS_U32_NIL,
            SCS_VALUE_TYPE_float,
            SCS_TELEMETRY_CHANNEL_FLAG_none,
            storeFloat,
            &telemetry.rpm
        );

    if (rpmResult != SCS_RESULT_ok)
    {
        gameLog(
            SCS_LOG_TYPE_error,
            "Haulmetry could not register RPM channel."
        );

        return SCS_RESULT_generic_error;
    }


    // ==================================================
    // FUEL
    // ==================================================

    const scs_result_t fuelResult =
        versionParams->register_for_channel(
            SCS_TELEMETRY_TRUCK_CHANNEL_fuel,
            SCS_U32_NIL,
            SCS_VALUE_TYPE_float,
            SCS_TELEMETRY_CHANNEL_FLAG_none,
            storeFloat,
            &telemetry.fuel
        );

    if (fuelResult != SCS_RESULT_ok)
    {
        gameLog(
            SCS_LOG_TYPE_error,
            "Haulmetry could not register fuel channel."
        );

        return SCS_RESULT_generic_error;
    }


    // ==================================================
    // GEAR
    // ==================================================

    const scs_result_t gearResult =
        versionParams->register_for_channel(
            SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear,
            SCS_U32_NIL,
            SCS_VALUE_TYPE_s32,
            SCS_TELEMETRY_CHANNEL_FLAG_none,
            storeS32,
            &telemetry.gear
        );

    if (gearResult != SCS_RESULT_ok)
    {
        gameLog(
            SCS_LOG_TYPE_error,
            "Haulmetry could not register gear channel."
        );

        return SCS_RESULT_generic_error;
    }


    // ==================================================
    // FRAME END EVENT
    // ==================================================

    const scs_result_t frameEndResult =
        versionParams->register_for_event(
            SCS_TELEMETRY_EVENT_frame_end,
            telemetryFrameEnd,
            nullptr
        );

    if (frameEndResult != SCS_RESULT_ok)
    {
        gameLog(
            SCS_LOG_TYPE_error,
            "Haulmetry could not register frame_end event."
        );

        return SCS_RESULT_generic_error;
    }


    gameLog(
        SCS_LOG_TYPE_message,
        "Haulmetry telemetry channels registered successfully."
    );


    return SCS_RESULT_ok;
}


// ==================================================
// Plugin shutdown
// ==================================================

SCSAPI_VOID scs_telemetry_shutdown(void)
{
    if (gameLog != nullptr)
    {
        gameLog(
            SCS_LOG_TYPE_message,
            "Haulmetry telemetry plugin shutting down."
        );
    }

    gameLog = nullptr;
}


// ==================================================
// Frame-end callback
// ==================================================

SCSAPI_VOID telemetryFrameEnd(
    const scs_event_t event,
    const void* const eventInfo,
    const scs_context_t context
)
{
    if (gameLog == nullptr)
    {
        return;
    }


    const auto now =
        std::chrono::steady_clock::now();


    const auto elapsed =
        std::chrono::duration_cast<
        std::chrono::seconds
        >(
            now - lastLogTime
        );


    // SCS may call frame_end many times per second.
    // For now, create one mapped snapshot per second.
    if (elapsed.count() < 1)
    {
        return;
    }


    lastLogTime = now;


    // ==================================================
    // LiveTelemetry -> TelemetryData
    // ==================================================

    const TelemetryData mappedTelemetry =
        mapTelemetry(
            telemetry,
            sequenceNumber
        );


    // ==================================================
    // Log mapped telemetry
    // ==================================================

    char buffer[512];

    std::snprintf(
        buffer,
        sizeof(buffer),

        "Haulmetry MAPPED | "
        "Truck: %s | "
        "Speed: %.1f km/h | "
        "RPM: %d | "
        "Fuel: %.1f L | "
        "Gear: %d | "
        "Sequence: %lld",

        mappedTelemetry.truckId.c_str(),
        mappedTelemetry.speed,
        mappedTelemetry.rpm,
        mappedTelemetry.fuel,
        mappedTelemetry.gear,
        static_cast<long long>(
            mappedTelemetry.sequenceNumber
            )
    );


    gameLog(
        SCS_LOG_TYPE_message,
        buffer
    );


    // Next telemetry snapshot gets the next sequence number.
    sequenceNumber++;
}