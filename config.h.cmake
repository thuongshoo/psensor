#ifndef CONFIG_H
#define CONFIG_H

#cmakedefine HAVE_APPINDICATOR
#cmakedefine HAVE_ATASMART
#cmakedefine HAVE_CURL
#cmakedefine HAVE_GTOP
#cmakedefine HAVE_JSON
#cmakedefine HAVE_LIBATIADL
#cmakedefine HAVE_LIBMICROHTTPD
#cmakedefine HAVE_LIBNOTIFY
#cmakedefine HAVE_LIBSENSORS
#cmakedefine HAVE_LIBUDISKS2
#cmakedefine HAVE_NVIDIA
#cmakedefine HAVE_REMOTE_SUPPORT
#cmakedefine HAVE_UNITY
#cmakedefine STDC_HEADERS
#cmakedefine HAVE_STDBOOL_H

#define PACKAGE "@PROJECT_NAME@"
#define PACKAGE_NAME "@PROJECT_NAME@"
#define PACKAGE_CONFIGURATION_FILENAME "@PROJECT_NAME@.cfg"
#define PACKAGE_NAME_WITHOUT_QUOTE @PROJECT_NAME@
#define PACKAGE_GSETTING "com.github.thuongshoo.@PROJECT_NAME@"
#define PSENSOR_FORK_SERVER_NAME "@PROJECT_NAME@-server"
#define PACKAGE_VERSION "@PROJECT_VERSION@"
#define PACKAGE_STRING "@PROJECT_NAME@ @PROJECT_VERSION@"
#define PACKAGE_USER_FOLDER ".@PROJECT_NAME@"
#define PACKAGE_BUGREPORT "@PACKAGE_BUGREPORT@"

#define PACKAGE_URL "@PACKAGE_URL@"
#define VERSION "@PROJECT_VERSION@"

#ifndef __clang_analyzer__
  #define ANALYZER_RETURNS_MALLOC
  #define ANALYZER_RETURNS_MALLOC_SIZE(idx)
  #define ANALYZER_TAKES_MALLOC(idx)
  #define ANALYZER_HOLDS_MALLOC(idx)
#else
  #define ANALYZER_RETURNS_MALLOC __attribute__((ownership_returns(malloc)))
  #define ANALYZER_RETURNS_MALLOC_SIZE(idx) __attribute__((ownership_returns(malloc, idx)))
  #define ANALYZER_TAKES_MALLOC(idx) __attribute__((ownership_takes(malloc, idx)))
  #define ANALYZER_HOLDS_MALLOC(idx) __attribute__((ownership_holds(malloc, idx)))
#endif


#define ENABLE_DEBUG_PRINT 0

#if ENABLE_DEBUG_PRINT
  #define DEBUG_PRINT(fmt, ...) g_print(fmt, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(fmt, ...) ((void)0)
#endif

#ifndef nullptr
#define nullptr NULL
#endif

#include <stddef.h>

#endif /* CONFIG_H */