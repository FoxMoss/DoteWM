# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# See the included README.md file for usage instructions.

# For VS2022 and Xcode 12+ support.
cmake_minimum_required(VERSION 3.21)

# Only generate Debug and Release configuration types.
set(CMAKE_CONFIGURATION_TYPES Debug Release)

# Project name.
project(cef)

# Use folders in the resulting project files.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


# Find GTK before including CEF
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Add GTK include directories globally
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

#
# CEF configuration.
#

# Specify the CEF distribution version.
set(CEF_VERSION "139.0.28+g55ab8a8+chromium-139.0.7258.139")
set(CEF_CHANNEL "")

# Determine the platform.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
  if("${PROJECT_ARCH}" STREQUAL "arm64")
    set(CEF_PLATFORM "macosarm64")
  elseif("${PROJECT_ARCH}" STREQUAL "x86_64")
    set(CEF_PLATFORM "macosx64")
  elseif("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "arm64")
    set(PROJECT_ARCH "arm64")
    set(CEF_PLATFORM "macosarm64")
  else()
    set(PROJECT_ARCH "x86_64")
    set(CEF_PLATFORM "macosx64")
  endif()
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm")
    set(CEF_PLATFORM "linuxarm")
  elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64")
    set(CEF_PLATFORM "linuxarm64")
  elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(CEF_PLATFORM "linux64")
  else()
    message(FATAL_ERROR "Linux x86 32-bit builds are discontinued.")
  endif()
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  if("${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "ARM64")
    set(CEF_PLATFORM "windowsarm64")
  elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(CEF_PLATFORM "windows64")
  else()
    set(CEF_PLATFORM "windows32")
  endif()
endif()

# Add this project's cmake/ directory to the module path.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Download and extract the CEF binary distribution (executes DownloadCEF.cmake).
include(DownloadCEF)
DownloadCEF("${CEF_PLATFORM}" "${CEF_CHANNEL}" "${CEF_VERSION}" "${CMAKE_SOURCE_DIR}/third_party/cef")

# Add the CEF binary distribution's cmake/ directory to the module path.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

# Load the CEF configuration (executes FindCEF.cmake).
find_package(CEF REQUIRED)

#
# Target configuration.
#

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".so")

# Include the libcef_dll_wrapper target (executes libcef_dll/CMakeLists.txt).
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Include CEF's test application targets (executes <target>/CMakeLists.txt).
add_subdirectory(${CEF_ROOT}/tests/cefclient)
add_subdirectory(${CEF_ROOT}/tests/cefsimple)
add_subdirectory(${CEF_ROOT}/tests/gtest)
add_subdirectory(${CEF_ROOT}/tests/ceftests)

# Allow includes relative to the current source directory.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(src)


# TODO: Include other application targets here.

# Display configuration settings.
PRINT_CEF_CONFIG()


set(CPACK_PACKAGE_NAME "dote-wm")
set(CPACK_PACKAGE_AUTHOR "FoxMoss")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A window manager framework for web technology")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "dote-wm")

set(CPACK_GENERATOR "TGZ;DEB;RPM")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "foxmoss@mediaology.com")

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")

set(CPACK_STRIP_FILES TRUE)

set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Applications")
set(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION "Main executables")
set(CPACK_COMPONENT_APPLICATIONS_REQUIRED TRUE)

set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime Libraries")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Required runtime libraries")
set(CPACK_COMPONENT_RUNTIME_REQUIRED TRUE)

set(CPACK_COMPONENT_RESOURCES_DISPLAY_NAME "Resources")
set(CPACK_COMPONENT_RESOURCES_DESCRIPTION "Application resources and data files")
set(CPACK_COMPONENT_RESOURCES_REQUIRED TRUE)

include(CPack)
