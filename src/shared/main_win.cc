// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "src/shared/main.h"

#include <windows.h>

#include "src/shared/app_factory.h"
#include "src/shared/client_manager.h"
#include "src/shared/main_util.h"

namespace shared {

// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance, void* sandbox_info) {
  // Provide CEF with command-line arguments.
  CefMainArgs main_args(hInstance);

  // Create a temporary CommandLine object.
  CefRefPtr<CefCommandLine> command_line = CreateCommandLine(main_args);

  // Create a CefApp of the correct process type.
  CefRefPtr<CefApp> app;
  switch (GetProcessType(command_line)) {
    case PROCESS_TYPE_BROWSER:
      app = CreateBrowserProcessApp();
      break;
    case PROCESS_TYPE_RENDERER:
      app = CreateRendererProcessApp();
      break;
    case PROCESS_TYPE_OTHER:
      app = CreateOtherProcessApp();
      break;
  }

  // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
  // that share the same executable. This function checks the command-line and,
  // if this is a sub-process, executes the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, app, sandbox_info);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }

  // Create the singleton manager instance.
  ClientManager manager;

  // Specify CEF global settings here.
  CefSettings settings;

  if (!sandbox_info) {
    settings.no_sandbox = true;
  }

  // Initialize the CEF browser process. The first browser instance will be
  // created in CefBrowserProcessHandler::OnContextInitialized() after CEF has
  // been initialized. May return false if initialization fails or if early exit
  // is desired (for example, due to process singleton relaunch behavior).
  if (!CefInitialize(main_args, settings, app, sandbox_info)) {
    return 1;
  }

  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();

  return 0;
}

}  // namespace shared
