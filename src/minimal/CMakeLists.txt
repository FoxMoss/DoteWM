# Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

#
# Source files.
#

# Main executable sources.
set(EXAMPLE_SRCS
  app_browser_minimal.cc
  client_minimal.cc
  main_minimal.cc
  scheme_handler.cc
  )
set(EXAMPLE_SRCS_WINDOWS
  resource.h
  )
APPEND_PLATFORM_SOURCES(EXAMPLE_SRCS)
  
if(OS_LINUX OR OS_WINDOWS)
  # On Windows and Linux the same executable is used for all processes.
  set(EXAMPLE_SRCS
    ${EXAMPLE_SRCS}
    app_renderer_minimal.cc
    )
elseif(OS_MAC)
  # On macOS a separate helper executable is used for subprocesses.
  set(EXAMPLE_HELPER_SRCS
    app_renderer_minimal.cc
    process_helper_mac_minimal.cc
    )
endif()

# Main executable resources.
set(EXAMPLE_RESOURCES_SRCS
  )
set(EXAMPLE_RESOURCES_SRCS_WINDOWS
  # Resources that embed the *.html and *.png files in the executable.
  win/resource.rc
  )
APPEND_PLATFORM_SOURCES(EXAMPLE_RESOURCES_SRCS)

if(OS_MACOSX OR OS_WINDOWS)
  # On macOS and Windows include the shared resources.
  set(EXAMPLE_RESOURCES_SRCS
    ${EXAMPLE_RESOURCES_SRCS}
    ${SHARED_RESOURCES_SRCS}
    )
endif()


#
# Shared configuration.
#

# Target executable names.
set(MINIMAL_TARGET "minimal")
if(OS_MAC)
  set(EXAMPLE_HELPER_TARGET "minimal_Helper")
  set(EXAMPLE_HELPER_OUTPUT_NAME "minimal Helper")
endif()


#
# Linux configuration.
#

if(OS_LINUX)
  # Executable target.
  add_executable(${MINIMAL_TARGET} ${EXAMPLE_SRCS})
  SET_EXAMPLE_EXECUTABLE_TARGET_PROPERTIES(${MINIMAL_TARGET})
  add_dependencies(${MINIMAL_TARGET} shared)
  target_link_libraries(${MINIMAL_TARGET} shared)
  target_compile_options(${MINIMAL_TARGET} PRIVATE -std=c++20 -g -fexceptions)
endif()


#
# Mac OS X configuration.
#

if(OS_MAC)
  # Create source groups for Xcode.
  SET_EXAMPLE_SOURCE_GROUPS("${EXAMPLE_SRCS}")
  SET_EXAMPLE_SOURCE_GROUPS("${EXAMPLE_HELPER_SRCS}")

  # Output path for the main app bundle.
  set(EXAMPLE_APP "${MINIMAL_TARGET_OUT_DIR}/${MINIMAL_TARGET}.app")

  # Variables referenced from the main Info.plist file.
  set(EXECUTABLE_NAME "${MINIMAL_TARGET}")
  set(PRODUCT_NAME "${MINIMAL_TARGET}")

  # Main app bundle target.
  add_executable(${MINIMAL_TARGET} MACOSX_BUNDLE ${EXAMPLE_SRCS} ${EXAMPLE_RESOURCES_SRCS})
  SET_EXAMPLE_EXECUTABLE_TARGET_PROPERTIES(${MINIMAL_TARGET})
  add_dependencies(${MINIMAL_TARGET} shared)
  target_link_libraries(${MINIMAL_TARGET} shared)
  set_target_properties(${MINIMAL_TARGET} PROPERTIES
    RESOURCE "${EXAMPLE_RESOURCES_SRCS}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/${SHARED_INFO_PLIST}"
    )

  # Copy the CEF framework into the Frameworks directory and create the necessary symlinks.
  COPY_MAC_FRAMEWORK("${MINIMAL_TARGET}" "${CEF_BINARY_DIR}" "${EXAMPLE_APP}")

  # Create the multiple Helper app bundle targets.
  foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
    # Convert to a list and extract the suffix values.
    string(REPLACE ":" ";" _suffix_list ${_suffix_list})
    list(GET _suffix_list 0 _name_suffix)
    list(GET _suffix_list 1 _target_suffix)
    list(GET _suffix_list 2 _plist_suffix)

    # Define Helper target and output names.
    set(_helper_target "${EXAMPLE_HELPER_TARGET}${_target_suffix}")
    set(_helper_output_name "${EXAMPLE_HELPER_OUTPUT_NAME}${_name_suffix}")

    # Create Helper-specific variants of the helper-Info.plist file. Do this
    # manually because the configure_file command (which is executed as part of
    # MACOSX_BUNDLE_INFO_PLIST) uses global env variables and would insert the
    # wrong values with multiple targets.
    set(_helper_info_plist "${CMAKE_CURRENT_BINARY_DIR}/helper-Info${_target_suffix}.plist.in")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${SHARED_HELPER_INFO_PLIST}" _plist_contents)
    string(REPLACE "\${EXECUTABLE_NAME}" "${_helper_output_name}" _plist_contents ${_plist_contents})
    string(REPLACE "\${PRODUCT_NAME}" "${_helper_output_name}" _plist_contents ${_plist_contents})
    string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_plist_suffix}" _plist_contents ${_plist_contents})
    file(WRITE ${_helper_info_plist} ${_plist_contents})

    # Create Helper executable target.
    add_executable(${_helper_target} MACOSX_BUNDLE ${EXAMPLE_HELPER_SRCS})
    SET_EXAMPLE_EXECUTABLE_TARGET_PROPERTIES(${_helper_target})
    add_dependencies(${_helper_target} shared_helper)
    target_link_libraries(${_helper_target} shared_helper)
    set_target_properties(${_helper_target} PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${_helper_info_plist}
      OUTPUT_NAME ${_helper_output_name}
      )

    # Add the Helper as a dependency of the main executable target.
    add_dependencies(${MINIMAL_TARGET} "${_helper_target}")

    # Copy the Helper app bundle into the Frameworks directory.
    add_custom_command(
      TARGET ${MINIMAL_TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${MINIMAL_TARGET_OUT_DIR}/${_helper_output_name}.app"
              "${EXAMPLE_APP}/Contents/Frameworks/${_helper_output_name}.app"
      VERBATIM
      )
  endforeach()

  # Manually process and copy over resource files.
  # The Xcode generator can support this via the set_target_properties RESOURCE
  # directive but that doesn't properly handle nested resource directories.
  # Remove these prefixes from input file paths.
  set(PREFIXES
    "../shared/mac/"
    )
  COPY_MAC_RESOURCES("${EXAMPLE_RESOURCES_SRCS}" "${PREFIXES}" "${MINIMAL_TARGET}" "${CMAKE_CURRENT_SOURCE_DIR}" "${EXAMPLE_APP}")
endif()


#
# Windows configuration.
#

if(OS_WINDOWS)
    # Add resources to the sources variable for convenience.
  set(EXAMPLE_SRCS
    ${EXAMPLE_SRCS}
    ${EXAMPLE_RESOURCES_SRCS}
    )

  # Create source groups for Visual Studio.
  SET_EXAMPLE_SOURCE_GROUPS("${EXAMPLE_SRCS}")

  if(USE_SANDBOX)
    # Shared library (DLL) target.
    add_library(${MINIMAL_TARGET} SHARED ${EXAMPLE_SRCS})
    SET_EXAMPLE_LIBRARY_TARGET_PROPERTIES(${MINIMAL_TARGET})

    # Copy and rename the bootstrap binary.
    COPY_SINGLE_FILE(${MINIMAL_TARGET} ${CEF_BINARY_DIR}/bootstrap.exe ${MINIMAL_TARGET_OUT_DIR}/${MINIMAL_TARGET}.exe)
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
      # Make the bootstrap binary the default command for the Visual Studio debugger.
      set_target_properties(${MINIMAL_TARGET} PROPERTIES
        VS_DEBUGGER_COMMAND "$<TARGET_FILE_DIR:${MINIMAL_TARGET}>/${MINIMAL_TARGET}.exe"
      )
    endif()
  else()
    # Executable target (sandbox disabled).
    add_executable(${MINIMAL_TARGET} WIN32 ${EXAMPLE_SRCS})
    SET_EXAMPLE_EXECUTABLE_TARGET_PROPERTIES(${MINIMAL_TARGET})
  endif()

  add_dependencies(${MINIMAL_TARGET} shared)
  target_link_libraries(${MINIMAL_TARGET} shared)
endif()
