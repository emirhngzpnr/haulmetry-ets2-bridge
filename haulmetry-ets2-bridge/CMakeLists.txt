find_package(CURL CONFIG REQUIRED)


# ==================================================
# HTTP / UDP bridge executable
# ==================================================

add_executable(
    haulmetry-ets2-bridge

    "haulmetry-ets2-bridge.cpp"
    "haulmetry-ets2-bridge.h"

    "telemetry-model.h"
    "telemetry-wire.h"
)


target_link_libraries(
    haulmetry-ets2-bridge

    PRIVATE

    CURL::libcurl
    ws2_32
)


target_compile_features(
    haulmetry-ets2-bridge

    PRIVATE

    cxx_std_20
)


# ==================================================
# SCS SDK include directory
# ==================================================

get_filename_component(
    SCS_SDK_INCLUDE_DIR

    "${CMAKE_CURRENT_SOURCE_DIR}/../external/scs-sdk/include"

    ABSOLUTE
)


message(
    STATUS

    "SCS SDK include directory: ${SCS_SDK_INCLUDE_DIR}"
)


# ==================================================
# ETS2 SCS plugin DLL
# ==================================================

add_library(
    haulmetry-scs-plugin

    SHARED

    "haulmetry-scs-plugin.cpp"
    "haulmetry-scs-plugin.def"

    "telemetry-model.h"
    "telemetry-wire.h"
)


target_include_directories(
    haulmetry-scs-plugin

    PRIVATE

    "${SCS_SDK_INCLUDE_DIR}"
)


target_link_libraries(
    haulmetry-scs-plugin

    PRIVATE

    ws2_32
)


target_compile_features(
    haulmetry-scs-plugin

    PRIVATE

    cxx_std_20
)