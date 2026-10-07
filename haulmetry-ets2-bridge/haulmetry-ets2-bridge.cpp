#include "telemetry-model.h"
#include "telemetry-wire.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <curl/curl.h>

#include <chrono>
#include <iostream>
#include <string>


static constexpr unsigned short UDP_PORT =
49000;

static constexpr int RECEIVE_TIMEOUT_MS =
1000;

static constexpr int SOURCE_TIMEOUT_SECONDS =
5;


// ==================================================
// HTTP response callback
// ==================================================

size_t writeCallback(
    char* contents,
    size_t size,
    size_t nmemb,
    void* userData
)
{
    const size_t totalSize =
        size * nmemb;


    auto* response =
        static_cast<std::string*>(
            userData
            );


    response->append(
        contents,
        totalSize
    );


    return totalSize;
}


// ==================================================
// TelemetryData -> JSON
// ==================================================

std::string toJson(
    const TelemetryData& telemetry
)
{
    return
        "{"
        "\"truckId\":\"" +
        telemetry.truckId +
        "\","

        "\"speed\":" +
        std::to_string(
            telemetry.speed
        ) +
        ","

        "\"rpm\":" +
        std::to_string(
            telemetry.rpm
        ) +
        ","

        "\"fuel\":" +
        std::to_string(
            telemetry.fuel
        ) +
        ","

        "\"gear\":" +
        std::to_string(
            telemetry.gear
        ) +
        ","

        "\"sequenceNumber\":" +
        std::to_string(
            telemetry.sequenceNumber
        ) +

        "}";
}


// ==================================================
// HTTP -> Spring Boot
// ==================================================

bool sendTelemetry(
    const TelemetryData& telemetry
)
{
    CURL* curl =
        curl_easy_init();


    if (curl == nullptr)
    {
        std::cerr
            << "libcurl initialization failed."
            << std::endl;

        return false;
    }


    const std::string json =
        toJson(
            telemetry
        );

    std::string responseBody;


    curl_slist* headers =
        nullptr;


    headers =
        curl_slist_append(
            headers,
            "Content-Type: application/json"
        );


    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        "http://localhost:8080/api/telemetry"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers
    );

    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDS,
        json.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDSIZE,
        static_cast<long>(
            json.size()
            )
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        writeCallback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &responseBody
    );

    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        2L
    );


    const CURLcode result =
        curl_easy_perform(
            curl
        );


    long httpCode = 0;


    if (result ==
        CURLE_OK)
    {
        curl_easy_getinfo(
            curl,
            CURLINFO_RESPONSE_CODE,
            &httpCode
        );
    }


    bool success =
        false;


    if (result !=
        CURLE_OK)
    {
        std::cerr
            << "HTTP request failed: "
            << curl_easy_strerror(
                result
            )
            << std::endl;
    }
    else if (
        httpCode >= 200 &&
        httpCode < 300
        )
    {
        std::cout
            << "HTTP "
            << httpCode
            << " | "
            << telemetry.truckId
            << " | speed="
            << telemetry.speed
            << " km/h"
            << " | rpm="
            << telemetry.rpm
            << " | fuel="
            << telemetry.fuel
            << " | gear="
            << telemetry.gear
            << " | sequence="
            << telemetry.sequenceNumber
            << std::endl;


        success =
            true;
    }
    else
    {
        std::cerr
            << "Backend returned HTTP "
            << httpCode
            << " | sequence="
            << telemetry.sequenceNumber
            << std::endl;


        if (!responseBody.empty())
        {
            std::cerr
                << "Response: "
                << responseBody
                << std::endl;
        }
    }


    curl_slist_free_all(
        headers
    );

    curl_easy_cleanup(
        curl
    );


    return success;
}


// ==================================================
// Main bridge
// ==================================================

int main()
{
    const CURLcode curlResult =
        curl_global_init(
            CURL_GLOBAL_DEFAULT
        );


    if (curlResult !=
        CURLE_OK)
    {
        std::cerr
            << "libcurl global initialization failed."
            << std::endl;

        return 1;
    }


    WSADATA wsaData{};


    const int wsaResult =
        WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        );


    if (wsaResult != 0)
    {
        std::cerr
            << "WSAStartup failed: "
            << wsaResult
            << std::endl;

        curl_global_cleanup();

        return 1;
    }


    SOCKET udpSocket =
        socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );


    if (udpSocket ==
        INVALID_SOCKET)
    {
        std::cerr
            << "Could not create UDP socket."
            << std::endl;

        WSACleanup();
        curl_global_cleanup();

        return 1;
    }


    // Wake recvfrom() every second so the bridge
    // can detect a missing telemetry source.
    const DWORD receiveTimeout =
        RECEIVE_TIMEOUT_MS;


    if (
        setsockopt(
            udpSocket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<
            const char*
            >(
                &receiveTimeout
                ),
            sizeof(
                receiveTimeout
                )
        ) == SOCKET_ERROR
        )
    {
        std::cerr
            << "Could not configure UDP receive timeout."
            << std::endl;

        closesocket(
            udpSocket
        );

        WSACleanup();
        curl_global_cleanup();

        return 1;
    }


    sockaddr_in address{};

    address.sin_family =
        AF_INET;

    address.sin_port =
        htons(
            UDP_PORT
        );


    InetPtonA(
        AF_INET,
        "127.0.0.1",
        &address.sin_addr
    );


    const int bindResult =
        bind(
            udpSocket,

            reinterpret_cast<
            sockaddr*
            >(
                &address
                ),

            sizeof(
                address
                )
        );


    if (bindResult ==
        SOCKET_ERROR)
    {
        std::cerr
            << "Could not bind UDP socket to port "
            << UDP_PORT
            << "."
            << std::endl;

        closesocket(
            udpSocket
        );

        WSACleanup();
        curl_global_cleanup();

        return 1;
    }


    std::cout
        << "Haulmetry bridge started."
        << std::endl;

    std::cout
        << "Listening on 127.0.0.1:"
        << UDP_PORT
        << std::endl;

    std::cout
        << "Waiting for ETS2 telemetry..."
        << std::endl;


    bool sourcePaused =
        false;

    bool sourceInterrupted =
        false;

    bool hasReceivedTelemetry =
        false;


    auto lastTelemetryTime =
        std::chrono::steady_clock::now();


    while (true)
    {
        char buffer[1024]{};


        const int receivedBytes =
            recvfrom(
                udpSocket,
                buffer,
                sizeof(buffer),
                0,
                nullptr,
                nullptr
            );


        // ==================================================
        // No UDP packet received
        // ==================================================

        if (receivedBytes ==
            SOCKET_ERROR)
        {
            const int socketError =
                WSAGetLastError();


            if (socketError ==
                WSAETIMEDOUT)
            {
                if (
                    hasReceivedTelemetry &&
                    !sourcePaused &&
                    !sourceInterrupted
                    )
                {
                    const auto now =
                        std::chrono::
                        steady_clock::now();


                    const auto silence =
                        std::chrono::
                        duration_cast<
                        std::chrono::seconds
                        >(
                            now -
                            lastTelemetryTime
                        );


                    if (
                        silence.count() >=
                        SOURCE_TIMEOUT_SECONDS
                        )
                    {
                        sourceInterrupted =
                            true;


                        std::cerr
                            << "Telemetry source interrupted. "
                            << "No data received for "
                            << SOURCE_TIMEOUT_SECONDS
                            << " seconds."
                            << std::endl;
                    }
                }


                continue;
            }


            std::cerr
                << "UDP receive failed. Error: "
                << socketError
                << std::endl;


            continue;
        }


        const std::string payload(
            buffer,
            receivedBytes
        );


        // ==================================================
        // PAUSE control message
        // ==================================================

        if (payload ==
            CONTROL_PAUSED)
        {
            sourcePaused =
                true;

            sourceInterrupted =
                false;


            std::cout
                << "Telemetry source paused."
                << std::endl;


            continue;
        }


        // ==================================================
        // RESUME control message
        // ==================================================

        if (payload ==
            CONTROL_RESUMED)
        {
            sourcePaused =
                false;

            sourceInterrupted =
                false;


            lastTelemetryTime =
                std::chrono::
                steady_clock::now();


            std::cout
                << "Telemetry source resumed."
                << std::endl;


            continue;
        }
        // ==================================================
        // STOPPED control message
        // ==================================================

        if (payload ==
            CONTROL_STOPPED)
        {
            sourcePaused =
                false;

            sourceInterrupted =
                true;

            hasReceivedTelemetry =
                false;


            std::cout
                << "Telemetry source stopped."
                << std::endl;


            continue;
        }

        // ==================================================
        // Telemetry packet
        // ==================================================

        TelemetryData telemetry{};


        if (!deserializeTelemetry(
            payload,
            telemetry
        ))
        {
            std::cerr
                << "Invalid telemetry packet: "
                << payload
                << std::endl;


            continue;
        }


        const auto now =
            std::chrono::
            steady_clock::now();


        if (sourceInterrupted)
        {
            std::cout
                << "Telemetry source restored."
                << std::endl;
        }


        sourcePaused =
            false;

        sourceInterrupted =
            false;

        hasReceivedTelemetry =
            true;

        lastTelemetryTime =
            now;


        sendTelemetry(
            telemetry
        );
    }


    closesocket(
        udpSocket
    );

    WSACleanup();

    curl_global_cleanup();


    return 0;
}