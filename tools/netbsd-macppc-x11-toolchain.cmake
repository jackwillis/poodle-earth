# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Dynamic NetBSD 11.1/macppc cross build of the X11 app on a Linux host. The compiler comes from
# tools/netbsd-macppc-tools.sh, the sysroot from tools/netbsd-macppc-sysroot.sh. Optimisation flags are
# not set here: CMake appends -O3 to any *_RELEASE_INIT, so pass -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
# (tools/netbsd-macppc-x11-build.sh does).
foreach(variable GLOBE_NETBSD_TOOLS GLOBE_NETBSD_SYSROOT)
  if("$ENV{${variable}}" STREQUAL "")
    message(FATAL_ERROR "Set ${variable} (tools: the tooldir/bin directory; sysroot: the assembled sysroot)")
  endif()
endforeach()
set(CMAKE_SYSTEM_NAME NetBSD)
set(CMAKE_SYSTEM_PROCESSOR powerpc)
set(CMAKE_CXX_COMPILER "$ENV{GLOBE_NETBSD_TOOLS}/powerpc--netbsd-g++")
set(CMAKE_SYSROOT "$ENV{GLOBE_NETBSD_SYSROOT}")
# Target headers and libraries, X11 included, come only from the sysroot; build-time programs
# (Python for the embedded data) come from the host.
set(CMAKE_FIND_ROOT_PATH "$ENV{GLOBE_NETBSD_SYSROOT}")
set(CMAKE_PREFIX_PATH "$ENV{GLOBE_NETBSD_SYSROOT}/usr/X11R7")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# X11 lives outside the default loader path on NetBSD; the run-time path is the target's, never the runner's.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-rpath,/usr/X11R7/lib -Wl,-zrelro")
set(CMAKE_SKIP_BUILD_RPATH TRUE)
