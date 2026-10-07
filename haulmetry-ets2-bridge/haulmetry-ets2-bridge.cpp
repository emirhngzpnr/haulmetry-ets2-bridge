#include "telemetry-model.h"
#include "telemetry-wire.h"

#include <curl/curl.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>


static constexpr unsigned short UDP_PORT = 49000;


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
        static_cast<std::string*>(userData);

    response->append(
        contents,
        totalSize
    );

    return totalSize;
}


// ==================================================
// TelemetryData -> backend JSON
// ==================================================

std::string toJson(
    const TelemetryData& telemetry
)
{
    return
        "{"
        "\"truckId\":\"" + telemetry.truckId + "\","
        "\"speed\":" + std::to_string(telemetry.speed) + ","
        "\"rpm\":" + std::to_string(telemetry.rpm) + ","
        "\"fuel\":" + std::to_string(telemetry.fuel) + ","
        "\"gear\":" + std::to_string(telemetry.gear) + ","
        "\"sequenceNumber\":" +
        std::to_string(telemetry.sequenceNumber) +
        "}";
}


// ==================================================
// Send telemetry to Spring Boot
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
        toJson(telemetry);

    std::string responseBody;


    curl_slist* headers = nullptr;

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

    // Backend is local.
    // We intentionally keep the timeouts short.
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
        curl_easy_perform(curl);


    long httpCode = 0;


    if (result == CURLE_OK)
    {
        curl_easy_getinfo(
            curl,
            CURLINFO_RESPONSE_CODE,
            &httpCode
        );
    }


    bool success = false;


    if (result != CURLE_OK)
    {
        std::cerr
            << "HTTP request failed: "
            << curl_easy_strerror(result)
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

        success = true;
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


    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);


    return success;
}


// ==================================================
// Main bridge process
// ==================================================

int main()
{
    // ----------------------------------------------
    // Initialize libcurl
    // ----------------------------------------------

    const CURLcode curlResult =
        curl_global_init(
            CURL_GLOBAL_DEFAULT
        );


    if (curlResult != CURLE_OK)
    {
        std::cerr
            << "libcurl global initialization failed."
            << std::endl;

        return 1;
    }


    // ----------------------------------------------
    // Initialize Winsock
    // ----------------------------------------------

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


    // ----------------------------------------------
    // Create UDP socket
    // ----------------------------------------------

    SOCKET udpSocket =
        socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );


    if (udpSocket == INVALID_SOCKET)
    {
        std::cerr
            << "Could not create UDP socket."
            << std::endl;

        WSACleanup();
        curl_global_cleanup();

        return 1;
    }


    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_port = htons(UDP_PORT);

    InetPtonA(
        AF_INET,
        "127.0.0.1",
        &address.sin_addr
    );


    // ----------------------------------------------
    // Bind to localhost:49000
    // ----------------------------------------------

    const int bindResult =
        bind(
            udpSocket,
            reinterpret_cast<sockaddr*>(
                &address
                ),
            sizeof(address)
        );


    if (bindResult == SOCKET_ERROR)
    {
        std::cerr
            << "Could not bind UDP socket to port "
            << UDP_PORT
            << "."
            << std::endl;

        closesocket(udpSocket);
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


    // ----------------------------------------------
    // Receive telemetry continuously
    // ----------------------------------------------

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


        if (receivedBytes == SOCKET_ERROR)
        {
            std::cerr
                << "UDP receive failed."
                << std::endl;

            continue;
        }


        const std::string payload(
            buffer,
            receivedBytes
        );


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


        sendTelemetry(
            telemetry
        );
    }


    // Normally unreachable in this first version.
    closesocket(udpSocket);

    WSACleanup();

    curl_global_cleanup();


    return 0;
}