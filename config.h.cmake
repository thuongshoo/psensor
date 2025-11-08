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

#define PACKAGE "@PROJECT_NAME@"
#define PACKAGE_NAME "@PROJECT_NAME@"
#define PACKAGE_VERSION "@PROJECT_VERSION@"
#define PACKAGE_STRING "@PROJECT_NAME@ @PROJECT_VERSION@"
#define PACKAGE_BUGREPORT "jeanfi@gmail.com"
#define PACKAGE_URL "http://wpitchoune.net/psensor"
#define VERSION "@PROJECT_VERSION@"

#endif /* CONFIG_H */