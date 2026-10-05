#include "scssdk_telemetry.h"
#include "eurotrucks2/scssdk_telemetry_eut2.h"

#include <chrono>
#include <cstdio>


struct LiveTelemetry
{
    float speed = 0.0f;
    float rpm = 0.0f;
    float fuel = 0.0f;
    scs_s32_t gear = 0;
};


static LiveTelemetry telemetry;

static scs_log_t gameLog = nullptr;

static auto lastLogTime =
std::chrono::steady_clock::now();


// Forward declaration
SCSAPI_VOID telemetryFrameEnd(
    const scs_event_t event,
    const void* const eventInfo,
    const scs_context_t context
);


// Float telemetry deðerlerini ilgili deðiþkene yazar
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


// Signed 32-bit telemetry deðerlerini ilgili deðiþkene yazar
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


// ETS2 plugin'i yüklediðinde çaðrýlýr
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

    gameLog(
        SCS_LOG_TYPE_message,
        "Haulmetry telemetry plugin initialized."
    );


    // SPEED
    versionParams->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_speed,
        SCS_U32_NIL,
        SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_none,
        storeFloat,
        &telemetry.speed
    );


    // RPM
    versionParams->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm,
        SCS_U32_NIL,
        SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_none,
        storeFloat,
        &telemetry.rpm
    );


    // FUEL
    versionParams->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_fuel,
        SCS_U32_NIL,
        SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_none,
        storeFloat,
        &telemetry.fuel
    );


    // GEAR
    versionParams->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear,
        SCS_U32_NIL,
        SCS_VALUE_TYPE_s32,
        SCS_TELEMETRY_CHANNEL_FLAG_none,
        storeS32,
        &telemetry.gear
    );


    // Her frame sonunda callback çaðrýlacak
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


    return SCS_RESULT_ok;
}


// ETS2 plugin'i kapatýrken çaðrýlýr
SCSAPI_VOID scs_telemetry_shutdown(void)
{
    gameLog = nullptr;
}


// Her frame sonunda çaðrýlýr.
// Log spam olmamasý için yaklaþýk saniyede bir çýktý üretir.
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

    if (elapsed.count() < 1)
    {
        return;
    }

    lastLogTime = now;

    char buffer[256];

    std::snprintf(
        buffer,
        sizeof(buffer),
        "Haulmetry LIVE | "
        "Speed: %.1f km/h | "
        "RPM: %.0f | "
        "Fuel: %.1f L | "
        "Gear: %d",

        telemetry.speed * 3.6f,
        telemetry.rpm,
        telemetry.fuel,
        static_cast<int>(telemetry.gear)
    );

    gameLog(
        SCS_LOG_TYPE_message,
        buffer
    );
}