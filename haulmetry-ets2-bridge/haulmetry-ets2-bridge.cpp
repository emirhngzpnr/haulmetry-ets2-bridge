#include <iostream>
#include <string>
#include <curl/curl.h>

using namespace std;

int main()
{
    CURL* curl = curl_easy_init();

    if (!curl) {
        cout << "libcurl initialization failed." << endl;
        return 1;
    }

    string json = R"({
        "truckId": "TRUCK-001",
        "speed": 82.4,
        "rpm": 1450,
        "fuel": 312.8,
        "gear": 8
    })";

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

    CURLcode result = curl_easy_perform(curl);
    long httpCode = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &httpCode
    );

    if (result != CURLE_OK) {
        cout << "Request failed: "
            << curl_easy_strerror(result)
            << endl;
    }
    else if (httpCode >= 200 && httpCode < 300) {
        cout << "Telemetry sent successfully. HTTP "
            << httpCode
            << endl;
    }
    else {
        cout << "Backend returned HTTP "
            << httpCode
            << endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return 0;
}