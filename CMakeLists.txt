cmake_minimum_required(VERSION 3.10)
project(psensorfork VERSION 1.4.0 LANGUAGES C)

set(PACKAGE_BUGREPORT "yuyoonshoo@gmail.com")
set(PACKAGE_ORIGINAL_NAME "psensorfork")
set(PACKAGE_URL "https://github.com/thuongshoo/psensor")
set(PACKAGE_APP_ID "com.github.thuongshoo.${PACKAGE_ORIGINAL_NAME}")
# if(FLATPAK_BUILD)
    set(PACKAGE_NAME ${PACKAGE_APP_ID})
#     message(STATUS "PACKAGE_NAME: ${PACKAGE_NAME}")
# else()
#     set(PACKAGE_NAME ${PACKAGE_ORIGINAL_NAME})
#     message(STATUS "PACKAGE_NAME: ${PACKAGE_NAME}")
# endif()

# enable compile commands database
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 
set(DESIRED_C_STANDARDS 23 17 11)

#
set(HIGHEST_SUPPORTED_STANDARD 11)
foreach(STANDARD IN LISTS DESIRED_C_STANDARDS)
    # 
    list(FIND CMAKE_C_COMPILE_FEATURES "c_std_${STANDARD}" FEATURE_INDEX)
    if(NOT FEATURE_INDEX EQUAL -1)
        set(HIGHEST_SUPPORTED_STANDARD ${STANDARD})
        break()
    endif()
endforeach()

# 
if(DEFINED HIGHEST_SUPPORTED_STANDARD)
    message(STATUS "Using the highest supported C standard: C${HIGHEST_SUPPORTED_STANDARD}")
    set(CMAKE_C_STANDARD ${HIGHEST_SUPPORTED_STANDARD})
else()
    # Fallback: 
    message(STATUS "No desired C standard found. Falling back to C11.")
    set(CMAKE_C_STANDARD ${HIGHEST_SUPPORTED_STANDARD})
endif()


# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug/Release/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

# Add compiler flags for Debug build
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# Build options
option(WITH_GTOP "Build with libgtop support" ON)
option(WITH_NVIDIA "Build with NVIDIA support" ON)
option(WITH_ATI "Build with ATI support" OFF)
option(WITH_UDISKS2 "Build with udisks2 support" ON)
option(ENABLE_NLS "Enable Native Language Support" ON)
option(ENABLE_CPPCHECK "Enable cppcheck analysis" ON)
option(ENABLE_COLOR_USE_DOUBLE "Use double precision for color" ON)
if(ENABLE_COLOR_USE_DOUBLE)
    set(COLOR_USE_DOUBLE 1)
endif()
option(USE_SYSTEM_LIBSENSORS "Use system libsensors instead of building from source" OFF)
# Find required tools
find_program(HELP2MAN help2man)
find_program(ASCIIDOC asciidoc)
find_program(CPPCHECK cppcheck)
find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)

# Standard install directories (provides CMAKE_INSTALL_DATADIR and
# CMAKE_INSTALL_FULL_LOCALEDIR). Ensure GNUInstallDirs is available before
# any install() calls or before we evaluate install destinations used for
# translations so CMAKE_INSTALL_FULL_LOCALEDIR is stable during configure.
include(GNUInstallDirs)

# Enable large file support
add_compile_definitions(_FILE_OFFSET_BITS=64)

# Set compiler flags similar to autotools
add_compile_options(-Wall)
# Add -rdynamic for GTK signal handlers
add_link_options(-rdynamic)
# GTK/GDK flags to ensure proper API usage
add_compile_definitions(
    GTK_DISABLE_SINGLE_INCLUDES
    GDK_DISABLE_DEPRECATED
    GSEAL_ENABLE
)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Gettext/i18n support
if(ENABLE_NLS)
    find_package(Gettext REQUIRED)
    find_package(Intl REQUIRED)
    # Ensure a valid CMAKE_INSTALL_FULL_LOCALEDIR; some CMake invocations
    # (or cached configs) may leave CMAKE_INSTALL_LOCALEDIR empty which would
    # define LOCALEDIR as an empty string in the binary. Provide a fallback
    # to <prefix>/share/locale so bindtextdomain() can find installed catalogs.
    if(NOT DEFINED CMAKE_INSTALL_FULL_LOCALEDIR OR CMAKE_INSTALL_FULL_LOCALEDIR STREQUAL "")
        set(CMAKE_INSTALL_FULL_LOCALEDIR "${CMAKE_INSTALL_PREFIX}/share/locale")
    endif()

    # Find all PO files
    file(GLOB PO_FILES ${CMAKE_SOURCE_DIR}/po/*.po)
    
    # Generate MO files for each PO file
    foreach(PO_FILE ${PO_FILES})
        get_filename_component(LANG ${PO_FILE} NAME_WE)
        set(MO_FILE ${CMAKE_CURRENT_BINARY_DIR}/po/${LANG}.mo)
        add_custom_command(
            OUTPUT ${MO_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/po
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${MO_FILE} ${PO_FILE}
            DEPENDS ${PO_FILE}
            COMMENT "Generating MO file for ${LANG}"
        )
        list(APPEND MO_FILES ${MO_FILE})
        # Install MO files into the full install locale directory. Use
        # CMAKE_INSTALL_FULL_LOCALEDIR which is validated/fallbacked above
        # to avoid generating installs like "/zh_CN/..." when the
        # short variable is empty during packaging.
        install(FILES ${MO_FILE}
            DESTINATION share/locale/${LANG}/LC_MESSAGES/
            RENAME ${PROJECT_NAME}.mo
        )
    endforeach()
    
    add_custom_target(translations ALL DEPENDS ${MO_FILES})
    set(ENABLE_NLS 1)
    add_compile_definitions(GETTEXT_PACKAGE="${PROJECT_NAME}")
    # Compile LOCALEDIR into the binary so bindtextdomain(PACKAGE, LOCALEDIR)
    # will resolve to an absolute directory under the install prefix. This
    # relies on CMAKE_INSTALL_FULL_LOCALEDIR being defined (above via
    # GNUInstallDirs or our fallback).
    #add_compile_definitions(LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
    set (LOCALEDIR "${CMAKE_INSTALL_FULL_LOCALEDIR}")
endif()

# Required dependencies
pkg_check_modules(GTK3 REQUIRED gtk+-3.0>=3.4)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(XEXT REQUIRED xext)

# Optional dependencies
pkg_check_modules(CANBERRA_GTK3 libcanberra-gtk3)
pkg_check_modules(AT_SPI2_CORE at-spi2-core)
pkg_check_modules(DBUS dbus-1)

# Add compilation flags for optional features
if(CANBERRA_GTK3_FOUND)
    set(HAVE_CANBERRA 1)
endif()

if(AT_SPI2_CORE_FOUND)
    set(HAVE_AT_SPI 1)
endif()

if(DBUS_FOUND)
    set(HAVE_DBUS 1)
endif()
pkg_check_modules(JSON REQUIRED json-c>=0.11.99)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(LIBMICROHTTPD REQUIRED libmicrohttpd)
pkg_check_modules(LIBNOTIFY libnotify)
if(WITH_UDISKS2)
    pkg_check_modules(UDISKS2  udisks2)
endif()
pkg_check_modules(ATASMART libatasmart)

set(APPINDICATOR_INCLUDE_STRING "")
# 1. Attempt to find the Ubuntu/upstream AppIndicator
pkg_check_modules(APPINDICATOR_UPSTREAM appindicator3-0.1)
if(APPINDICATOR_UPSTREAM_FOUND)
    message(STATUS "Found AppIndicator (Ubuntu/Upstream). Setting include path for <libappindicator/app-indicator.h>")
    set(APPINDICATOR_PC_NAME appindicator3-0.1)
    # Define the exact string to use in the #include directive
    set(APPINDICATOR_INCLUDE_STRING "\"libappindicator/app-indicator.h\"")
    set(HAVE_APPINDICATOR 1)
else()
    # 2. Attempt to find the Debian/Ayatana variant
    pkg_check_modules(APPINDICATOR_AYATANA ayatana-appindicator3-0.1)
    if(APPINDICATOR_AYATANA_FOUND)
        message(STATUS "Found Ayatana AppIndicator (Debian). Setting include path for <libayatana-appindicator/app-indicator.h>")
        set(APPINDICATOR_PC_NAME ayatana-appindicator3-0.1)
        # Define the exact string to use in the #include directive
        set(APPINDICATOR_INCLUDE_STRING "\"libayatana-appindicator/app-indicator.h\"")
        set(HAVE_APPINDICATOR 1)
    else()
        # 3. If neither is found, error out.
        message(FATAL_ERROR "Could not find either 'appindicator3-0.1' or 'ayatana-appindicator3-0.1' development packages.")
    endif()
endif()

# Use pkg-config one last time to populate all linker variables correctly
pkg_check_modules(APPINDICATOR REQUIRED ${APPINDICATOR_PC_NAME})

# Define HAVE_* macros for pkg-config-found libs so headers that expect
# autoconf-style macros will choose the real implementations (not inline stubs)
if(JSON_LIBRARIES)
    set(HAVE_JSON 1)
endif()
if(CURL_LIBRARIES)
    set(HAVE_CURL 1)
endif()
if(LIBMICROHTTPD_LIBRARIES)
    set(HAVE_LIBMICROHTTPD 1)
endif()
if(LIBNOTIFY_LIBRARIES)
    set(HAVE_LIBNOTIFY 1)
endif()
if(UDISKS2_LIBRARIES)
    set(HAVE_LIBUDISKS2 1)
endif()

if(ATASMART_LIBRARIES)
    set(HAVE_ATASMART 1)
endif()

check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("string.h" HAVE_STRING_H)

if(HAVE_STDLIB_H AND HAVE_STRING_H)
    set(STDC_HEADERS 1)
endif()

# Kiểm tra stdbool.h (cho HAVE_STDBOOL_H)
check_include_file("stdbool.h" HAVE_STDBOOL_H)

# Remote monitoring support (JSON + CURL)
if(JSON_LIBRARIES AND CURL_LIBRARIES)
    set(HAVE_REMOTE_SUPPORT 1)
endif()

# Optional dependencies
if(WITH_GTOP)
    pkg_check_modules(GTOP libgtop-2.0)
    if(GTOP_FOUND)
        set(HAVE_GTOP 1)
    endif()
endif()

# Check for libsensors or build statically from source if not found or requested
set(HAVE_LIBSENSORS 0)
if(USE_SYSTEM_LIBSENSORS)
    find_library(SENSORS_LIB NAMES sensors)
    find_path(SENSORS_INCLUDE_DIR NAMES sensors/sensors.h)
    if(SENSORS_LIB AND SENSORS_INCLUDE_DIR)
        message(STATUS "Found system libsensors: ${SENSORS_LIB}")
        set(HAVE_LIBSENSORS 1)
    else()
        message(STATUS "System libsensors not found, will build lm-sensors from source statically.")
    endif()
endif()

if(NOT HAVE_LIBSENSORS)
    set(LMSENSORS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/lm-sensors")
    set(LMSENSORS_STAGING "${CMAKE_BINARY_DIR}/lm-sensors-staging")

    add_custom_command(
        OUTPUT
            ${LMSENSORS_STAGING}/lib/libsensors.a
            ${LMSENSORS_STAGING}/include/sensors/sensors.h
        COMMENT "Building lm-sensors static library"
        COMMAND make -d BUILD_STATIC_LIB=1 BUILD_SHARED_LIB=0 PREFIX=${LMSENSORS_STAGING} BUILD_DIR=flatpakbuilđir user
        COMMENT "Installing lm-sensors static library"
        COMMAND make -d BUILD_STATIC_LIB=1 BUILD_SHARED_LIB=0 PREFIX=${LMSENSORS_STAGING} BUILD_DIR=flatpakbuilđir user_install
        WORKING_DIRECTORY "${LMSENSORS_SOURCE_DIR}"
        COMMENT "Done: Building and installing lm-sensors static library"
        VERBATIM
    )
    add_custom_target(lm-sensors-static ALL
        DEPENDS
            ${LMSENSORS_STAGING}/lib/libsensors.a
            ${LMSENSORS_STAGING}/include/sensors/sensors.h
    )

    set(SENSORS_INCLUDE_DIR "${LMSENSORS_STAGING}/include")
    set(SENSORS_LIB "${LMSENSORS_STAGING}/lib/libsensors.a")
    set(HAVE_LIBSENSORS 1)
endif()

# NVIDIA support
if(WITH_NVIDIA)
    find_path(NVIDIA_INCLUDE_DIR NVCtrl/NVCtrl.h
        /usr/include
        /usr/local/include
    )
    find_library(NVIDIA_LIB XNVCtrl)
    if(NVIDIA_INCLUDE_DIR AND NVIDIA_LIB)
        set(HAVE_NVIDIA 1)
    endif()
endif()
if(NVIDIA_LIB)
    list(APPEND PSENSOR_EXTRA_LIBS ${NVIDIA_LIB})
endif()



# Set the package data directory variable for paths.h.in
set(pkgdatadir "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")



# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Installation prefix" FORCE)
endif()

# Make sure GNUInstallDirs is included before using its variables
include(GNUInstallDirs)

# Desktop file macro and data dir for source use
set(DESKTOP_FILE "\"${PROJECT_NAME}.desktop\"")
set(PSENSOR_DESKTOP_FILE "${PROJECT_NAME}.desktop")
message(STATUS "DESKTOP_FILE: ${DESKTOP_FILE}")
message(STATUS "PSENSOR_DESKTOP_FILE: ${PSENSOR_DESKTOP_FILE}")

set(DATADIR_VAL "\"${CMAKE_INSTALL_FULL_DATADIR}\"")
set(DATADIR ${CMAKE_INSTALL_FULL_DATADIR})

set(WWW_DIR_VAL "\"${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/www\"")
set(DEFAULT_WWW_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/www")

# Set default PACKAGE_DATA_DIR but allow environment override
if(NOT DEFINED ENV{PSENSOR_DATA_DIR})
    set(PACKAGE_DATA_DIR1 "\\"" )
    set(PACKAGE_DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}"")
else()
    set(PACKAGE_DATA_DIR2 "\"$ENV{PSENSOR_DATA_DIR}\"")
    set(PACKAGE_DATA_DIR "$ENV{PSENSOR_DATA_DIR}")
endif()

message(STATUS "Installation prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Full data directory: ${CMAKE_INSTALL_FULL_DATADIR}")
message(STATUS "Package data directory: ${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")

if(NOT CANBERRA_GTK3_FOUND)
    message(STATUS "libcanberra-gtk3 not found - sound support will be disabled")
endif()

if(NOT DBUS_FOUND)
    message(STATUS "dbus-1 not found - some features may be limited")
endif()

# Xác định tên đích tùy môi trường
if(FLATPAK_BUILD)
    set(ICON_INSTALL_NAME "${PACKAGE_APP_ID}")
    set(DESKTOP_INSTALL_NAME "${PACKAGE_APP_ID}.desktop")
else()
    set(ICON_INSTALL_NAME "${PACKAGE_NAME}")
    set(DESKTOP_INSTALL_NAME "${PACKAGE_NAME}.desktop")
endif()

# Configure header
configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)
# Find psensorfork data directory at runtime
configure_file(
    ${CMAKE_SOURCE_DIR}/src/paths.h.in
    ${CMAKE_BINARY_DIR}/paths.h
    @ONLY)
# Installation
include(GNUInstallDirs)
add_subdirectory(src)



# Install binaries
install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}-server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Cài desktop file với tên phù hợp
install(FILES ${PROJECT_NAME}.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    RENAME "${DESKTOP_INSTALL_NAME}"
)

# Chỉ sửa đổi file .desktop sau khi cài đặt khi build cho Flatpak
if(FLATPAK_BUILD)
    install(CODE "
        file(READ \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications/${DESKTOP_INSTALL_NAME}\" DESKTOP_CONTENT)
        string(REGEX REPLACE \"Icon=psensorfork\" \"Icon=${PACKAGE_APP_ID}\" DESKTOP_CONTENT_MODIFIED \"\${DESKTOP_CONTENT}\")
        file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications/${DESKTOP_INSTALL_NAME}\" \"\${DESKTOP_CONTENT_MODIFIED}\")
    " COMPONENT Applications)
endif()

# # Install icons for all themes and sizes
# install(DIRECTORY icons/hicolor/
#     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor
#     PATTERN "Makefile*" EXCLUDE    
# )
# install(DIRECTORY icons/ubuntu-mono-dark/
#     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/ubuntu-mono-dark
#     PATTERN "Makefile*" EXCLUDE    
# )
# install(DIRECTORY icons/ubuntu-mono-light/
#     DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/ubuntu-mono-light
#     PATTERN "Makefile*" EXCLUDE
# )

# Cài icon chính cho tất cả các size (cả hai tên)
set(HICOLOR_SIZES 14x14 16x16 22x22 24x24 32x32 48x48 64x64)
foreach(size IN LISTS HICOLOR_SIZES)
    if(EXISTS ${CMAKE_SOURCE_DIR}/icons/hicolor/${size}/psensorfork.png)
        if(FLATPAK_BUILD)
            # Tên có tiền tố Application ID (cho Flatpak export)
        install(FILES ${CMAKE_SOURCE_DIR}/icons/hicolor/${size}/psensorfork.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/apps
            RENAME "${PACKAGE_APP_ID}.png")
        else()
        # Tên gốc cho .deb
        install(FILES ${CMAKE_SOURCE_DIR}/icons/hicolor/${size}/psensorfork.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/apps
            RENAME "psensorfork.png")
        endif()
    endif()
endforeach()
# Cài icon scalable (chính, hot, normal) — cả hai tên
foreach(icon_suffix "" "_normal" "_hot")
    if(EXISTS ${CMAKE_SOURCE_DIR}/icons/hicolor/scalable/psensorfork${icon_suffix}.svg)
        if(FLATPAK_BUILD)
            # Tên có tiền tố Application ID
            install(FILES ${CMAKE_SOURCE_DIR}/icons/hicolor/scalable/psensorfork${icon_suffix}.svg
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
                RENAME "${PACKAGE_APP_ID}${icon_suffix}.svg")
        else()
        # Tên gốc
        install(FILES ${CMAKE_SOURCE_DIR}/icons/hicolor/scalable/psensorfork${icon_suffix}.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME "psensorfork${icon_suffix}.svg")
        endif()    
    endif()
endforeach()
# Cài icon cho ubuntu-mono-dark và ubuntu-mono-light — cả hai tên
foreach(theme "ubuntu-mono-dark" "ubuntu-mono-light")
    foreach(icon_suffix "" "_normal" "_hot")
        if(EXISTS ${CMAKE_SOURCE_DIR}/icons/${theme}/status/22/psensorfork${icon_suffix}.svg)
            if(FLATPAK_BUILD)
                # Tên có tiền tố Application ID
                install(FILES ${CMAKE_SOURCE_DIR}/icons/${theme}/status/22/psensorfork${icon_suffix}.svg
                    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/${theme}/status/22
                    RENAME "${PACKAGE_APP_ID}${icon_suffix}.svg")
            else()
                # Tên gốc
                install(FILES ${CMAKE_SOURCE_DIR}/icons/${theme}/status/22/psensorfork${icon_suffix}.svg
                    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/${theme}/status/22
                    RENAME "psensorfork${icon_suffix}.svg")
            endif()    
        endif()
    endforeach()
endforeach()

# Install glade UI files
# Main interface first
install(FILES src/glade/${PROJECT_NAME}.glade
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})

# Then install all glade files to the glade subdirectory
install(FILES
    src/glade/${PROJECT_NAME}-appindicator.glade
    src/glade/${PROJECT_NAME}-pref.glade
    # src/glade/${PROJECT_NAME}.glade
    src/glade/${PROJECT_NAME}-edit.glade
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})

# Install web interface files
install(DIRECTORY www/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/www
    FILES_MATCHING 
    PATTERN "*.html"
    PATTERN "*.css"
    PATTERN "*.js"
    PATTERN "*.png"
    PATTERN "Makefile*" EXCLUDE
)

# Install GSettings schema
install(FILES src/${PROJECT_NAME}.gschema.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas
)

# Install documentation
install(FILES
    AUTHORS
    ChangeLog
    COPYING
    NEWS
    NEWS.html
    README
    README.html
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

message(STATUS "CMAKE_INSTALL_FULL_LOCALEDIR = ${CMAKE_INSTALL_FULL_LOCALEDIR}")
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")

# Generate man page if help2man is available
if(HELP2MAN)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/src/${PROJECT_NAME}.1
        COMMAND ${HELP2MAN} --include=${CMAKE_SOURCE_DIR}/src/description.txt
                           -N --name="Temperature monitoring application"
                           --output=${CMAKE_CURRENT_BINARY_DIR}/src/${PROJECT_NAME}.1
                           $<TARGET_FILE:${PROJECT_NAME}>
        DEPENDS ${PROJECT_NAME}
        COMMENT "Generating man page with help2man"
    )
    add_custom_target(man ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/src/${PROJECT_NAME}.1)
    
    # CHUYỂN install() VÀO ĐÂY - CHỈ KHI help2man TỒN TẠI
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/src/${PROJECT_NAME}.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
else()
    message(STATUS "help2man not found, man page will not be installed")
endif()


# # Generate HTML documentation if asciidoc is available
# if(ASCIIDOC)
#     add_custom_command(
#         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/README.html
#         COMMAND ${ASCIIDOC} --theme style -a themedir=${CMAKE_SOURCE_DIR}/www -n -a toc
#                            -o ${CMAKE_CURRENT_BINARY_DIR}/README.html
#                            ${CMAKE_SOURCE_DIR}/README
#         DEPENDS ${CMAKE_SOURCE_DIR}/README ${CMAKE_SOURCE_DIR}/www/style.css
#         COMMENT "Generating README.html with asciidoc"
#     )
#     add_custom_command(
#         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/NEWS.html
#         COMMAND ${ASCIIDOC} --theme style -a themedir=${CMAKE_SOURCE_DIR}/www
#                            -o ${CMAKE_CURRENT_BINARY_DIR}/NEWS.html
#                            ${CMAKE_SOURCE_DIR}/NEWS
#         DEPENDS ${CMAKE_SOURCE_DIR}/NEWS ${CMAKE_SOURCE_DIR}/www/style.css
#         COMMENT "Generating NEWS.html with asciidoc"
#     )
#     add_custom_target(docs ALL 
#         DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/README.html
#                 ${CMAKE_CURRENT_BINARY_DIR}/NEWS.html
#     )
# endif()

# Run cppcheck if available
if(CPPCHECK AND ENABLE_CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK} --enable=all --inline-suppr
                           -I ${CMAKE_SOURCE_DIR}/src
                           -I ${CMAKE_SOURCE_DIR}/src/lib
                           ${CMAKE_SOURCE_DIR}/src
        COMMENT "Running cppcheck analysis"
    )
endif()


# Post-install commands
install(CODE "
    # Compile GSettings schema
    execute_process(
        COMMAND ${GLIB_COMPILE_SCHEMAS} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas
    )
")