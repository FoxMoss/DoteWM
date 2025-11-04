#include "src/minimal/client_minimal.h"
#include "src/minimal/scheme_handler.h"
#include "src/minimal/scheme_strings.h"
#include "src/shared/app_factory.h"
#include "src/shared/browser_util.h"
#include "src/shared/resource_util.h"

namespace minimal {

namespace {

std::string GetStartupURL() {
  return "fox://base/index.html";
};

}  // namespace

// Minimal implementation of CefApp for the browser process.
class BrowserApp : public CefApp, public CefBrowserProcessHandler {
 public:
  BrowserApp() {}

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  void OnBeforeCommandLineProcessing(
      const CefString& process_type,
      CefRefPtr<CefCommandLine> command_line) override {
    // Command-line flags can be modified in this callback.
    // |process_type| is empty for the browser process.
    if (process_type.empty()) {
#if defined(OS_MACOSX)
      // Disable the macOS keychain prompt. Cookies will not be encrypted.
      command_line->AppendSwitch("use-mock-keychain");
#endif
    }
  }

  void OnRegisterCustomSchemes(
      CefRawPtr<CefSchemeRegistrar> registrar) override {
    registrar->AddCustomScheme(SCHEME, SCHEME_OPTIONS);
  }

  // CefBrowserProcessHandler methods:
  void OnContextInitialized() override {
    RegisterSchemeHandlerFactory();

    // Create the browser window.
    shared::CreateBrowser(new Client(), GetStartupURL(), CefBrowserSettings());
  }

 private:
  IMPLEMENT_REFCOUNTING(BrowserApp);
  DISALLOW_COPY_AND_ASSIGN(BrowserApp);
};

}  // namespace minimal

namespace shared {

CefRefPtr<CefApp> CreateBrowserProcessApp() {
  return new minimal::BrowserApp();
}

}  // namespace shared
