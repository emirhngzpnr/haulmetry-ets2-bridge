#include <iostream>
#include <string>
#include <curl/curl.h>
#include <cstdint>

struct TelemetryData
{
    std::string truckId;
    double speed;
    int rpm;
    double fuel;
    int gear;
    std::int64_t sequenceNumber;
};

size_t writeCallback(
    char* contents,
    size_t size,
    size_t nmemb,
    void* userData
)
{
    const size_t totalSize = size * nmemb;

    auto* response = static_cast<std::string*>(userData);
    response->append(contents, totalSize);

    return totalSize;
}

std::string toJson(const TelemetryData& telemetry)
{
    return
        "{"
        "\"truckId\":\"" + telemetry.truckId + "\","
        "\"speed\":" + std::to_string(telemetry.speed) + ","
        "\"rpm\":" + std::to_string(telemetry.rpm) + ","
        "\"fuel\":" + std::to_string(telemetry.fuel) + ","
        "\"gear\":" + std::to_string(telemetry.gear) + ","
        "\"sequenceNumber\":" + std::to_string(telemetry.sequenceNumber) +
        "}";
}
bool sendTelemetry(const TelemetryData& telemetry)
{
    CURL* curl = curl_easy_init();

    if (!curl)
    {
        std::cerr << "libcurl initialization failed." << std::endl;
        return false;
    }

    const std::string json = toJson(telemetry);
    std::string responseBody;

    curl_slist* headers = nullptr;
    headers = curl_slist_append(
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
        static_cast<long>(json.size())
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
        5L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        10L
    );

    std::cout
        << "Sending JSON: "
        << json
        << std::endl;

    const CURLcode result = curl_easy_perform(curl);

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
            << "Request failed: "
            << curl_easy_strerror(result)
            << std::endl;
    }
    else if (httpCode >= 200 && httpCode < 300)
    {
        std::cout
            << "Telemetry sent successfully. HTTP "
            << httpCode
            << std::endl;

        success = true;
    }
    else
    {
        std::cerr
            << "Backend returned HTTP "
            << httpCode
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

int main()
{
    const CURLcode globalInitResult =
        curl_global_init(CURL_GLOBAL_DEFAULT);

    if (globalInitResult != CURLE_OK)
    {
        std::cerr
            << "libcurl global initialization failed."
            << std::endl;

        return 1;
    }

    std::int64_t sequenceNumber = 1;

    for (int i = 0; i < 3; i++)
    {
        const TelemetryData telemetry{
            "TRUCK-001",
            82.4,
            1450,
            312.8,
            8,
            sequenceNumber
        };

        const bool success =
            sendTelemetry(telemetry);

        if (!success)
        {
            curl_global_cleanup();
            return 1;
        }

        sequenceNumber++;
    }

    curl_global_cleanup();

    return 0;
}   