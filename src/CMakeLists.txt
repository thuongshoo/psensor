# Include directories for compilation
include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/lib
    ${GTK3_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${XEXT_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${LIBMICROHTTPD_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
    ${UDISKS2_INCLUDE_DIRS}
    ${ATASMART_INCLUDE_DIRS}
    ${APPINDICATOR3_INCLUDE_DIRS}
)

# Source files
file(GLOB PSENSOR_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/lib/*.c"
)

# Server sources (separate executable)
file(GLOB SERVER_SOURCES
    "${CMAKE_SOURCE_DIR}/src/server/*.c"
    "${CMAKE_SOURCE_DIR}/src/lib/*.c"
)

# Optional gating of features: these variables are set at top-level
if(NOT GTOP_INCLUDE_DIRS)
    list(REMOVE_ITEM PSENSOR_SOURCES "${CMAKE_SOURCE_DIR}/src/lib/pgtop2.c")
    list(REMOVE_ITEM SERVER_SOURCES "${CMAKE_SOURCE_DIR}/src/lib/pgtop2.c")
else()
    include_directories(${GTOP_INCLUDE_DIRS})
    add_compile_definitions(HAVE_GTOP=1)
endif()

if(NOT UDISKS2_INCLUDE_DIRS)
    list(REMOVE_ITEM PSENSOR_SOURCES "${CMAKE_SOURCE_DIR}/src/lib/pudisks2.c")
    list(REMOVE_ITEM SERVER_SOURCES "${CMAKE_SOURCE_DIR}/src/lib/pudisks2.c")
else()
    include_directories(${UDISKS2_INCLUDE_DIRS})
    add_compile_definitions(HAVE_LIBUDISKS2=1)
endif()

if(APPINDICATOR3_LIBRARIES)
    add_compile_definitions(HAVE_APPINDICATOR=1)
else()
    list(REMOVE_ITEM PSENSOR_SOURCES "${CMAKE_SOURCE_DIR}/src/ui_appindicator.c")
    list(REMOVE_ITEM SERVER_SOURCES "${CMAKE_SOURCE_DIR}/src/ui_appindicator.c")
endif()

if(UNITY_LIBRARIES)
    add_compile_definitions(HAVE_UNITY=1)
else()
    list(REMOVE_ITEM PSENSOR_SOURCES "${CMAKE_SOURCE_DIR}/src/ui_unity.c")
    list(REMOVE_ITEM SERVER_SOURCES "${CMAKE_SOURCE_DIR}/src/ui_unity.c")
endif()

# ATI/ADL optional
find_path(LIBATIADL_INCLUDE adl_sdk.h PATHS /usr/include /usr/local/include)
find_library(LIBATIADL_LIB atiadlxx)
find_library(LIB_RT rt)
if(LIBATIADL_INCLUDE AND LIBATIADL_LIB)
    add_definitions(-DHAVE_LIBATIADL)
    include_directories(${LIBATIADL_INCLUDE})
    set(PSENSOR_EXTRA_LIBS ${LIBATIADL_LIB})
else()
    list(REMOVE_ITEM PSENSOR_SOURCES "${CMAKE_SOURCE_DIR}/src/lib/amd.c")
    list(REMOVE_ITEM SERVER_SOURCES "${CMAKE_SOURCE_DIR}/src/lib/amd.c")
endif()

# Build GUI executable
add_executable(psensor ${PSENSOR_SOURCES})

target_link_libraries(psensor
    ${GTK3_LIBRARIES}
    ${X11_LIBRARIES}
    ${XEXT_LIBRARIES}
    ${JSON_LIBRARIES}
    ${CURL_LIBRARIES}
    ${LIBMICROHTTPD_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    ${UDISKS2_LIBRARIES}
    ${ATASMART_LIBRARIES}
    ${APPINDICATOR3_LIBRARIES}
    ${PSENSOR_EXTRA_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${LIB_RT}
    m
)

# Build server executable
add_executable(psensor-server ${SERVER_SOURCES})

target_link_libraries(psensor-server
    ${GTK3_LIBRARIES}
    ${X11_LIBRARIES}
    ${XEXT_LIBRARIES}
    ${JSON_LIBRARIES}
    ${CURL_LIBRARIES}
    ${LIBMICROHTTPD_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    ${UDISKS2_LIBRARIES}
    ${ATASMART_LIBRARIES}
    ${APPINDICATOR3_LIBRARIES}
    ${GTOP_LIBRARIES}
    ${SENSORS_LIB}
    ${PSENSOR_EXTRA_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    m
)

if(SENSORS_LIB)
    target_link_libraries(psensor ${SENSORS_LIB})
endif()

if(GTOP_FOUND)
    target_link_libraries(psensor ${GTOP_LIBRARIES})
endif()

if(WITH_NVIDIA AND NVIDIA_LIB)
    target_link_libraries(psensor ${NVIDIA_LIB})
endif()
