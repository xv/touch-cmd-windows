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
#define VER_MINOR 1
#define VER_PATCH 0

#define VERSION_STR \
        STRINGIFY(VER_MAJOR) \
    "." STRINGIFY(VER_MINOR) \
    "." STRINGIFY(VER_PATCH)

#endif // VERSION_H
