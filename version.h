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

#define VER_FILEVERSION \
    VER_MAJOR, VER_MINOR, VER_REVIS, VER_BUILD

#define VER_DISPVERSION_STR \
        STRINGIFY(VER_MAJOR) \
    "." STRINGIFY(VER_MINOR) \
    "." STRINGIFY(VER_REVIS)

#define VER_FILEVERSION_STR \
        VER_DISPVERSION_STR \
    "." STRINGIFY(VER_BUILD)

#define VER_PRODUCTVERSION      VER_FILEVERSION
#define VER_PRODUCTVERSION_STR  VER_DISPVERSION_STR

#endif // VERSION_H
