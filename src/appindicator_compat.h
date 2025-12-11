#ifndef APPINDICATOR_COMPAT_H
#define APPINDICATOR_COMPAT_H

#include "config.h"
/* 
 * APPINDICATOR_INCLUDE_NAME will be defined by CMake 
 * as either <libappindicator/app-indicator.h> or 
 * <libayatana-appindicator/app-indicator.h>
 */
#include APPINDICATOR_INCLUDE_NAME

#endif // APPINDICATOR_COMPAT_H
