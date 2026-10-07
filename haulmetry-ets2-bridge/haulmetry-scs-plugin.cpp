#include "scssdk_telemetry.h"
#include "eurotrucks2/scssdk_telemetry_eut2.h"

#include "telemetry-model.h"
#include "telemetry-wire.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>


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

static auto lastSendTime =
std::chrono::steady_clock::now();

static std::int64_t sequenceNumber = 1;

static std::string sessionId;

static bool telemetryPaused = true;

static constexpr const char* TRUCK_ID =
"TRUCK-001";

static constexpr unsigned short UDP_PORT =
49000;


// ==================================================
// UDP state
// ==================================================

static SOCKET udpSocket =
INVALID_SOCKET;

static sockaddr_in bridgeAddress{};


// ==================================================
// Forward declarations
// ==================================================

SCSAPI_VOID telemetryFrameEnd(
    const scs_event_t event,
    const void* const eventInfo,
    const scs_context_t context
);

SCSAPI_VOID telemetryPauseState(
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

std::string generateSessionId();

bool initializeUdp();

void shutdownUdp();

bool sendPayloadToBridge(
    const std::string& payload
);

bool sendTelemetryToBridge(
    const TelemetryData& telemetryData
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
    if (value == nullptr ||
        context == nullptr)
    {
        return;
    }


    auto* target =
        static_cast<float*>(
            context
            );


    *target =
        value->value_float.value;
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
    if (value == nullptr ||
        context == nullptr)
    {
        return;
    }


    auto* target =
        static_cast<scs_s32_t*>(
            context
            );


    *target =
        value->value_s32.value;
}


// ==================================================
// Pause / resume state
// ==================================================

SCSAPI_VOID telemetryPauseState(
    const scs_event_t event,
    const void* const eventInfo,
    const scs_context_t context
)
{
    telemetryPaused =
        event ==
        SCS_TELEMETRY_EVENT_paused;


    if (telemetryPaused)
    {
        sendPayloadToBridge(
            CONTROL_PAUSED
        );


        if (gameLog != nullptr)
        {
            gameLog(
                SCS_LOG_TYPE_message,
                "Haulmetry telemetry paused."
            );
        }
    }
    else
    {
        // Wait a full interval before sending
        // telemetry again after resume.
        lastSendTime =
            std::chrono::steady_clock::now();


        sendPayloadToBridge(
            CONTROL_RESUMED
        );


        if (gameLog != nullptr)
        {
            gameLog(
                SCS_LOG_TYPE_message,
                "Haulmetry telemetry resumed."
            );
        }
    }
}


// ==================================================
// Map SCS telemetry -> Haulmetry model
// ==================================================

TelemetryData mapTelemetry(
    const LiveTelemetry& source,
    const std::int64_t sequence
)
{
    return TelemetryData
    {
        TRUCK_ID,
        sessionId,

        std::fabs(
            static_cast<double>(
                source.speed
            ) * 3.6
        ),

        static_cast<int>(
            std::lround(
                source.rpm
            )
        ),

        static_cast<double>(
            source.fuel
        ),

        static_cast<int>(
            source.gear
        ),

        sequence
    };
}


// ==================================================
// UDP initialization
// ==================================================

std::string generateSessionId()
{
    GUID guid{};
    if (FAILED(CoCreateGuid(&guid)))
    {
        return {};
    }

    char buffer[37]{};
    std::snprintf(
        buffer, sizeof(buffer),
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        static_cast<unsigned int>(guid.Data1),
        static_cast<unsigned int>(guid.Data2),
        static_cast<unsigned int>(guid.Data3),
        static_cast<unsigned int>(guid.Data4[0]),
        static_cast<unsigned int>(guid.Data4[1]),
        static_cast<unsigned int>(guid.Data4[2]),
        static_cast<unsigned int>(guid.Data4[3]),
        static_cast<unsigned int>(guid.Data4[4]),
        static_cast<unsigned int>(guid.Data4[5]),
        static_cast<unsigned int>(guid.Data4[6]),
        static_cast<unsigned int>(guid.Data4[7])
    );
    return buffer;
}


bool initializeUdp()
{
    WSADATA wsaData{};


    const int startupResult =
        WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        );


    if (startupResult != 0)
    {
        return false;
    }


    udpSocket =
        socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );


    if (udpSocket ==
        INVALID_SOCKET)
    {
        WSACleanup();

        return false;
    }


    bridgeAddress = {};

    bridgeAddress.sin_family =
        AF_INET;

    bridgeAddress.sin_port =
        htons(
            UDP_PORT
        );


    const int addressResult =
        InetPtonA(
            AF_INET,
            "127.0.0.1",
            &bridgeAddress.sin_addr
        );


    if (addressResult != 1)
    {
        closesocket(
            udpSocket
        );

        udpSocket =
            INVALID_SOCKET;

        WSACleanup();

        return false;
    }


    return true;
}


// ==================================================
// UDP shutdown
// ==================================================

void shutdownUdp()
{
    if (udpSocket !=
        INVALID_SOCKET)
    {
        closesocket(
            udpSocket
        );

        udpSocket =
            INVALID_SOCKET;
    }


    WSACleanup();
}


// ==================================================
// Generic UDP send
// ==================================================

bool sendPayloadToBridge(
    const std::string& payload
)
{
    if (udpSocket ==
        INVALID_SOCKET)
    {
        return false;
    }


    const int sendResult =
        sendto(
            udpSocket,

            payload.c_str(),

            static_cast<int>(
                payload.size()
                ),

            0,

            reinterpret_cast<
            const sockaddr*
            >(
                &bridgeAddress
                ),

            sizeof(
                bridgeAddress
                )
        );


    return
        sendResult != SOCKET_ERROR;
}


// ==================================================
// Send telemetry -> bridge
// ==================================================

bool sendTelemetryToBridge(
    const TelemetryData& telemetryData
)
{
    return sendPayloadToBridge(
        serializeTelemetry(
            telemetryData
        )
    );
}


// ==================================================
// Plugin initialization
// ==================================================

SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version,
    const scs_telemetry_init_params_t* const params
)
{
    if (version !=
        SCS_TELEMETRY_VERSION_1_00)
    {
        return
            SCS_RESULT_unsupported;
    }


    if (params == nullptr)
    {
        return
            SCS_RESULT_generic_error;
    }


    const auto* versionParams =
        static_cast<
        const scs_telemetry_init_params_v100_t*
        >(
            params
            );


    gameLog =
        versionParams->common.log;


    if (gameLog == nullptr)
    {
        return
            SCS_RESULT_generic_error;
    }


    gameLog(
        SCS_LOG_TYPE_message,
        "Haulmetry telemetry plugin initialized."
    );


    sessionId = generateSessionId();
    if (sessionId.empty())
    {
        gameLog(
            SCS_LOG_TYPE_error,
            "Haulmetry could not generate telemetry session ID."
        );
        return SCS_RESULT_generic_error;
    }

    sequenceNumber = 1;
    telemetry = {};
    telemetryPaused = true;
    lastSendTime = std::chrono::steady_clock::now();

    char sessionBuffer[256]{};
    std::snprintf(
        sessionBuffer, sizeof(sessionBuffer),
        "Haulmetry telemetry session created: %s", sessionId.c_str()
    );
    gameLog(SCS_LOG_TYPE_message, sessionBuffer);

    if (!initializeUdp())
    {
        gameLog(
            SCS_LOG_TYPE_error,
            "Haulmetry could not initialize UDP transport."
        );

        return
            SCS_RESULT_generic_error;
    }


    gameLog(
        SCS_LOG_TYPE_message,
        "Haulmetry UDP transport initialized."
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


    if (speedResult !=
        SCS_RESULT_ok)
    {
        shutdownUdp();

        return
            SCS_RESULT_generic_error;
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


    if (rpmResult !=
        SCS_RESULT_ok)
    {
        shutdownUdp();

        return
            SCS_RESULT_generic_error;
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


    if (fuelResult !=
        SCS_RESULT_ok)
    {
        shutdownUdp();

        return
            SCS_RESULT_generic_error;
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


    if (gearResult !=
        SCS_RESULT_ok)
    {
        shutdownUdp();

        return
            SCS_RESULT_generic_error;
    }


    // ==================================================
    // FRAME END
    // ==================================================

    const scs_result_t frameEndResult =
        versionParams->register_for_event(
            SCS_TELEMETRY_EVENT_frame_end,
            telemetryFrameEnd,
            nullptr
        );


    if (frameEndResult !=
        SCS_RESULT_ok)
    {
        shutdownUdp();

        return
            SCS_RESULT_generic_error;
    }


    // ==================================================
    // PAUSED
    // ==================================================

    const scs_result_t pausedResult =
        versionParams->register_for_event(
            SCS_TELEMETRY_EVENT_paused,
            telemetryPauseState,
            nullptr
        );


    if (pausedResult !=
        SCS_RESULT_ok)
    {
        shutdownUdp();

        return
            SCS_RESULT_generic_error;
    }


    // ==================================================
    // STARTED
    // ==================================================

    const scs_result_t startedResult =
        versionParams->register_for_event(
            SCS_TELEMETRY_EVENT_started,
            telemetryPauseState,
            nullptr
        );


    if (startedResult !=
        SCS_RESULT_ok)
    {
        shutdownUdp();

        return
            SCS_RESULT_generic_error;
    }


    gameLog(
        SCS_LOG_TYPE_message,
        "Haulmetry telemetry channels and events registered successfully."
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


    sendPayloadToBridge(CONTROL_STOPPED);

    shutdownUdp();


    gameLog =
        nullptr;
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


    if (telemetryPaused)
    {
        return;
    }


    const auto now =
        std::chrono::steady_clock::now();


    const auto elapsed =
        std::chrono::duration_cast<
        std::chrono::seconds
        >(
            now -
            lastSendTime
        );


    if (elapsed.count() < 1)
    {
        return;
    }


    lastSendTime =
        now;


    if (
        telemetry.speed == 0.0f &&
        telemetry.rpm == 0.0f &&
        telemetry.fuel == 0.0f &&
        telemetry.gear == 0
        )
    {
        return;
    }


    const TelemetryData mappedTelemetry =
        mapTelemetry(
            telemetry,
            sequenceNumber
        );


    if (!sendTelemetryToBridge(
        mappedTelemetry
    ))
    {
        gameLog(
            SCS_LOG_TYPE_error,
            "Haulmetry failed to send telemetry to bridge."
        );

        return;
    }


    char buffer[512];


    std::snprintf(
        buffer,
        sizeof(buffer),

        "Haulmetry SENT | "
        "Truck: %s | "
        "Session: %s | "
        "Speed: %.1f km/h | "
        "RPM: %d | "
        "Fuel: %.1f L | "
        "Gear: %d | "
        "Sequence: %lld",

        mappedTelemetry.truckId.c_str(),
        mappedTelemetry.sessionId.c_str(),
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


    sequenceNumber++;
}