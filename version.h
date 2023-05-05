/* version.h
 * Copyright (C) 2023 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef VERSION_H
#define VERSION_H

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#define VER_MAJOR 1
#define VER_MINOR 0
#define VER_REVIS 0
#define VER_BUILD 0

#define VER_FILE_VERSION \
    VER_MAJOR, VER_MINOR, VER_REVIS, VER_BUILD

#define VER_FILE_VERSION_STR \
        STRINGIFY(VER_MAJOR) \
    "." STRINGIFY(VER_MINOR) \
    "." STRINGIFY(VER_REVIS) \
    "." STRINGIFY(VER_BUILD)

#define VER_PRODUCT_VERSION      VER_FILE_VERSION
#define VER_PRODUCT_VERSION_STR  VER_FILE_VERSION_STR

#endif // VERSION_H
