find_package(CURL CONFIG REQUIRED)

add_executable(haulmetry-ets2-bridge
    "haulmetry-ets2-bridge.cpp"
    "haulmetry-ets2-bridge.h"
)
target_link_libraries(haulmetry-ets2-bridge PRIVATE CURL::libcurl)
target_compile_features(haulmetry-ets2-bridge PRIVATE cxx_std_20)
