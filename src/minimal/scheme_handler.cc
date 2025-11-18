#include "src/minimal/scheme_handler.h"
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include <sys/utsname.h>

#include "include/cef_browser.h"
#include "include/cef_callback.h"
#include "include/cef_frame.h"
#include "include/cef_request.h"
#include "include/cef_resource_handler.h"
#include "include/cef_response.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_helpers.h"

#include "nn.h"
#include "src/minimal/mime.h"
#include "src/minimal/scheme_strings.h"
#include "src/protobuf/windowmanager.pb.h"
#include "src/shared/client_util.h"
#include "src/shared/resource_util.h"

namespace minimal {

namespace {

// Implementation of the scheme handler for client:// requests.
class ClientSchemeHandler : public CefResourceHandler {
  int sock;

 public:
  ClientSchemeHandler(int sock) : sock(sock), offset_(0) {}

  bool NotFound(std::string url, CefRefPtr<CefCallback> callback) {
    data_ = url + " not found.";

    mime_type_ = "text/html";

    callback->Continue();
    return true;
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    CEF_REQUIRE_IO_THREAD();

    std::string url = request->GetURL();

    std::string path_start = SCHEME "://" DOMAIN "/";
    size_t start_pos = url.rfind(path_start, 0);
    if (start_pos == std::string::npos || start_pos != 0)
      return NotFound(url, callback);

    std::string base_path(url.begin() + path_start.size(), url.end());

    std::string home_dir = getenv("HOME");
    if (home_dir == "") {
      home_dir = ".";
    }

    std::filesystem::path target_file =
        home_dir + "/.config/" SCHEME "/" + base_path;

    if (!std::filesystem::exists(target_file))
      return NotFound(target_file.string(), callback);

    std::ifstream file_stream(target_file.string());
    std::ostringstream data_stream;
    data_stream << file_stream.rdbuf();
    data_ = data_stream.str();

    auto type = mime_types.find(target_file.extension().string());
    if (type == mime_types.end())
      mime_type_ = "text/html";
    else {
      mime_type_ = type->second;
    }

    Packet packet;
    auto segment = packet.add_segments();
    segment->mutable_file_register_request()->set_file_path(
        target_file.string());

    size_t len = packet.ByteSizeLong();
    char* buf = (char*)malloc(len);
    packet.SerializeToArray(buf, len);

    nn_send(sock, buf, len, 0);
    free(buf);

    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    CEF_REQUIRE_IO_THREAD();

    DCHECK(!data_.empty());

    response->SetMimeType(mime_type_);
    response->SetStatus(200);

    // Set the resulting response length.
    response_length = data_.length();
  }

  void Cancel() override { CEF_REQUIRE_IO_THREAD(); }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    CEF_REQUIRE_IO_THREAD();

    bool has_data = false;
    bytes_read = 0;

    if (offset_ < data_.length()) {
      // Copy the next block of data into the buffer.
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(data_.length() - offset_));
      memcpy(data_out, data_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

 private:
  std::string data_;
  std::string mime_type_;
  size_t offset_;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(ClientSchemeHandler);
};

// Implementation of the factory for creating scheme handlers.
class ClientSchemeHandlerFactory : public CefSchemeHandlerFactory {
  int sock;

 public:
  ClientSchemeHandlerFactory(int sock) : sock(sock) {}

  // Return a new scheme handler instance to handle the request.
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    CEF_REQUIRE_IO_THREAD();
    return new ClientSchemeHandler(sock);
  }

 private:
  IMPLEMENT_REFCOUNTING(ClientSchemeHandlerFactory);
  DISALLOW_COPY_AND_ASSIGN(ClientSchemeHandlerFactory);
};

}  // namespace

void RegisterSchemeHandlerFactory(int sock) {
  CefRegisterSchemeHandlerFactory(SCHEME, DOMAIN,
                                  new ClientSchemeHandlerFactory(sock));
}

}  // namespace minimal
