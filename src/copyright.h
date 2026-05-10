#ifndef PSENSOR_COPYRIGHT_H
#define PSENSOR_COPYRIGHT_H

/* Author information */
#define PSENSOR_ORIGINAL_AUTHOR "Jean-François Wauthy"
#define PSENSOR_ORIGINAL_EMAIL "jeanfi@gmail.com"
#define PSENSOR_ORIGINAL_YEARS "2010-2016"

#define PSENSOR_CURRENT_MAINTAINER "thuongshoo"
#define PSENSOR_CURRENT_EMAIL "yuyoonshoo@gmail.com"
#define PSENSOR_CURRENT_YEARS "2024-2026"

/* 
 * Copyright string for CLI - giữ nguyên %s để truyền năm vào
 * KHÔNG nối chuỗi trong macro, để gettext() có thể dịch toàn bộ chuỗi
 */
#define PSENSOR_COPYRIGHT_CLI \
    "Copyright (C) %s %s\n" \
    "Copyright (C) %s %s\n" \
    "License GPLv2: GNU GPL version 2 or later " \
    "<http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>\n" \
    "This is free software: you are free to change and redistribute it.\n" \
    "There is NO WARRANTY, to the extent permitted by law.\n"

/* Authors array for GTK about dialog */
#define PSENSOR_AUTHORS { \
    PSENSOR_CURRENT_MAINTAINER " <" PSENSOR_CURRENT_EMAIL "> (Current Maintainer)", \
    PSENSOR_ORIGINAL_AUTHOR " <" PSENSOR_ORIGINAL_EMAIL "> (Original Author)", \
    NULL \
}

/*
 * About dialog copyright text - string hoàn chỉnh để gettext dịch được
 * KHÔNG dùng %s vì GTK about dialog không xử lý format
 */
#define PSENSOR_COPYRIGHT_GTK \
    "Copyright (C) 2010-2016 Jean-François Wauthy\n" \
    "Copyright (C) 2024-2026 Lê Hoài Thanh"

#endif /* PSENSOR_COPYRIGHT_H */