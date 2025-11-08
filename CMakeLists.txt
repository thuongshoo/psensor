cmake_minimum_required(VERSION 3.10)
project(psensor VERSION 1.2.0 LANGUAGES C)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug/Release/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

# Add compiler flags for Debug build
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

# Build options
option(WITH_GTOP "Build with libgtop support" ON)
option(WITH_NVIDIA "Build with NVIDIA support" ON)
option(WITH_ATI "Build with ATI support" OFF)
option(ENABLE_NLS "Enable Native Language Support" ON)
option(ENABLE_CPPCHECK "Enable cppcheck analysis" ON)

# Find required tools
find_program(HELP2MAN help2man)
find_program(ASCIIDOC asciidoc)
find_program(CPPCHECK cppcheck)
find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)

# Enable large file support
add_compile_definitions(_FILE_OFFSET_BITS=64)

# Set compiler flags similar to autotools
add_compile_options(-Wall)
# Add -rdynamic for GTK signal handlers
add_link_options(-rdynamic)
# GTK/GDK flags to ensure proper API usage
add_definitions(
    -DGTK_DISABLE_SINGLE_INCLUDES
    -DGDK_DISABLE_DEPRECATED
    -DGSEAL_ENABLE
)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Gettext/i18n support
if(ENABLE_NLS)
    find_package(Gettext REQUIRED)
    find_package(Intl REQUIRED)
    
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
        install(FILES ${MO_FILE}
            DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${LANG}/LC_MESSAGES/
            RENAME ${PROJECT_NAME}.mo
        )
    endforeach()
    
    add_custom_target(translations ALL DEPENDS ${MO_FILES})
    add_definitions(-DENABLE_NLS=1)
    add_definitions(-DGETTEXT_PACKAGE="${PROJECT_NAME}")
    add_definitions(-DLOCALEDIR="/usr/local/share/locale")
    add_definitions(-DPACKAGE="${PROJECT_NAME}")
endif()

# Standard install directories (provides CMAKE_INSTALL_DATADIR)
include(GNUInstallDirs)

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
    add_compile_definitions(HAVE_CANBERRA=1)
endif()

if(AT_SPI2_CORE_FOUND)
    add_compile_definitions(HAVE_AT_SPI=1)
endif()

if(DBUS_FOUND)
    add_compile_definitions(HAVE_DBUS=1)
endif()
pkg_check_modules(JSON REQUIRED json-c>=0.11.99)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(LIBMICROHTTPD REQUIRED libmicrohttpd)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
pkg_check_modules(UDISKS2 REQUIRED udisks2)
pkg_check_modules(ATASMART REQUIRED libatasmart)
pkg_check_modules(APPINDICATOR3 REQUIRED appindicator3-0.1)

# Define HAVE_* macros for pkg-config-found libs so headers that expect
# autoconf-style macros will choose the real implementations (not inline stubs)
if(JSON_LIBRARIES)
    add_compile_definitions(HAVE_JSON=1)
endif()
if(CURL_LIBRARIES)
    add_compile_definitions(HAVE_CURL=1)
endif()
if(LIBMICROHTTPD_LIBRARIES)
    add_compile_definitions(HAVE_LIBMICROHTTPD=1)
endif()
if(LIBNOTIFY_LIBRARIES)
    add_compile_definitions(HAVE_LIBNOTIFY=1)
endif()
if(UDISKS2_LIBRARIES)
    add_compile_definitions(HAVE_LIBUDISKS2=1)
endif()

if(ATASMART_LIBRARIES)
    add_compile_definitions(HAVE_ATASMART=1)
endif()

# Remote monitoring support (JSON + CURL)
if(JSON_LIBRARIES AND CURL_LIBRARIES)
    add_compile_definitions(HAVE_REMOTE_SUPPORT=1)
endif()

# Optional dependencies
if(WITH_GTOP)
    pkg_check_modules(GTOP libgtop-2.0)
    if(GTOP_FOUND)
        add_definitions(-DHAVE_GTOP)
    endif()
endif()

# Check for libsensors
find_library(SENSORS_LIB sensors)
if(SENSORS_LIB)
    add_compile_definitions(HAVE_LIBSENSORS=1)
endif()

# NVIDIA support
if(WITH_NVIDIA)
    find_path(NVIDIA_INCLUDE_DIR NVCtrl/NVCtrl.h
        /usr/include
        /usr/local/include
    )
    find_library(NVIDIA_LIB XNVCtrl)
    if(NVIDIA_INCLUDE_DIR AND NVIDIA_LIB)
        add_definitions(-DHAVE_NVIDIA)
    endif()
endif()
if(NVIDIA_LIB)
    list(APPEND PSENSOR_EXTRA_LIBS ${NVIDIA_LIB})
endif()

# Configure header
configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

# Set the package data directory variable for paths.h.in
set(pkgdatadir "${CMAKE_INSTALL_FULL_DATADIR}/psensor")

# Find psensor data directory at runtime
configure_file(
    ${CMAKE_SOURCE_DIR}/src/paths.h.in
    ${CMAKE_BINARY_DIR}/paths.h
    @ONLY)

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Installation prefix" FORCE)
endif()

# Make sure GNUInstallDirs is included before using its variables
include(GNUInstallDirs)

# Desktop file macro and data dir for source use
add_compile_definitions(PSENSOR_DESKTOP_FILE=\"psensor.desktop\")
add_compile_definitions(DATADIR=\"${CMAKE_INSTALL_FULL_DATADIR}\")
add_compile_definitions(DEFAULT_WWW_DIR=\"${CMAKE_INSTALL_FULL_DATADIR}/psensor/www\")
# Set default PACKAGE_DATA_DIR but allow environment override
if(NOT DEFINED ENV{PSENSOR_DATA_DIR})
    add_compile_definitions(PACKAGE_DATA_DIR=\"${CMAKE_INSTALL_FULL_DATADIR}/psensor\")
else()
    add_compile_definitions(PACKAGE_DATA_DIR=\"$ENV{PSENSOR_DATA_DIR}\")
endif()

message(STATUS "Installation prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Full data directory: ${CMAKE_INSTALL_FULL_DATADIR}")
message(STATUS "Package data directory: ${CMAKE_INSTALL_FULL_DATADIR}/psensor")

if(NOT CANBERRA_GTK3_FOUND)
    message(STATUS "libcanberra-gtk3 not found - sound support will be disabled")
endif()

if(NOT DBUS_FOUND)
    message(STATUS "dbus-1 not found - some features may be limited")
endif()

add_subdirectory(src)

# Installation
include(GNUInstallDirs)

# Install binaries
install(TARGETS psensor psensor-server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install desktop file
install(FILES psensor.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# Install icons for all themes and sizes
install(DIRECTORY icons/hicolor/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor
)
install(DIRECTORY icons/ubuntu-mono-dark/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/ubuntu-mono-dark
)
install(DIRECTORY icons/ubuntu-mono-light/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/ubuntu-mono-light
)

# Install glade UI files
# Glade UI files and main interface
install(FILES src/glade/psensor.glade 
    DESTINATION ${CMAKE_INSTALL_DATADIR}/psensor)
install(DIRECTORY src/glade/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/psensor/glade
    FILES_MATCHING PATTERN "*.glade"
    PATTERN "*.ui"
)

# Install web interface files
install(DIRECTORY www/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/psensor/www
    FILES_MATCHING 
    PATTERN "*.html"
    PATTERN "*.css"
    PATTERN "*.js"
    PATTERN "*.png"
)

# Install GSettings schema
install(FILES src/psensor.gschema.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas
)

# Install man pages
install(FILES src/psensor.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
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

# Install translations
install(DIRECTORY po/
    DESTINATION ${CMAKE_INSTALL_LOCALEDIR}
    FILES_MATCHING PATTERN "*.mo"
)

# Generate man page if help2man is available
if(HELP2MAN)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/psensor.1
        COMMAND ${HELP2MAN} --include=${CMAKE_SOURCE_DIR}/src/description.txt
                           -N --name="Temperature monitoring application"
                           --output=${CMAKE_CURRENT_BINARY_DIR}/psensor.1
                           $<TARGET_FILE:psensor>
        DEPENDS psensor
        COMMENT "Generating man page with help2man"
    )
    add_custom_target(man ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/psensor.1)
endif()

# Generate HTML documentation if asciidoc is available
if(ASCIIDOC)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/README.html
        COMMAND ${ASCIIDOC} --theme style -a themedir=${CMAKE_SOURCE_DIR}/www -n -a toc
                           -o ${CMAKE_CURRENT_BINARY_DIR}/README.html
                           ${CMAKE_SOURCE_DIR}/README
        DEPENDS ${CMAKE_SOURCE_DIR}/README ${CMAKE_SOURCE_DIR}/www/style.css
        COMMENT "Generating README.html with asciidoc"
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/NEWS.html
        COMMAND ${ASCIIDOC} --theme style -a themedir=${CMAKE_SOURCE_DIR}/www
                           -o ${CMAKE_CURRENT_BINARY_DIR}/NEWS.html
                           ${CMAKE_SOURCE_DIR}/NEWS
        DEPENDS ${CMAKE_SOURCE_DIR}/NEWS ${CMAKE_SOURCE_DIR}/www/style.css
        COMMENT "Generating NEWS.html with asciidoc"
    )
    add_custom_target(docs ALL 
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/README.html
                ${CMAKE_CURRENT_BINARY_DIR}/NEWS.html
    )
endif()

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