// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "src/minimal/client_minimal.h"
#include <X11/X.h>
#include <absl/strings/str_format.h>
#include <cstdio>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <thread>

#undef Success

#include "nn.h"
#include "pair.h"
#include "src/protobuf/windowmanager.pb.h"
#include "src/shared/client_util.h"

#include <format>
#include <nlohmann/json.hpp>
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"
#include "src/shared/client_util.h"
#include "src/shared/resource_util.h"

namespace minimal {

class MessageHandler : public CefMessageRouterBrowserSide::Handler {
 public:
  explicit MessageHandler(int sock) : ipc_sock(sock) {}

  int ipc_sock;

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    try {
      nlohmann::json from_browser = nlohmann::json::parse(request.ToString());

      Packet packet;
      for (auto segment_json : from_browser) {
        if (segment_json["t"] == "window_map") {  // for some reason it crashes
                                                  // when we use ["type"] lol
          auto segment = packet.add_segments();
          auto window_map = segment->mutable_window_map_request();
          window_map->set_window(
              std::stoll(segment_json["window"].get<std::string>()));
          window_map->set_x(segment_json["x"]);
          window_map->set_y(segment_json["y"]);
          window_map->set_width(segment_json["width"]);
          window_map->set_height(segment_json["height"]);

        } else if (segment_json["t"] == "window_reorder") {
          auto segment = packet.add_segments();
          auto window_reorder = segment->mutable_window_reorder_request();

          for (auto window : segment_json["windows"]) {
            window_reorder->add_windows(std::stoll(window.get<std::string>()));
          }

        } else if (segment_json["t"] == "window_focus") {
          auto segment = packet.add_segments();
          auto window_focus = segment->mutable_window_focus_request();

          window_focus->set_window(
              std::stoll(segment_json["window"].get<std::string>()));

        } else if (segment_json["t"] == "window_register_border") {
          auto segment = packet.add_segments();
          auto window_register_border =
              segment->mutable_window_register_border_request();

          window_register_border->set_window(
              std::stoll(segment_json["window"].get<std::string>()));
          window_register_border->set_x(segment_json["x"]);
          window_register_border->set_y(segment_json["y"]);
          window_register_border->set_width(segment_json["width"]);
          window_register_border->set_height(segment_json["height"]);
        } else if (segment_json["t"] == "render_request") {
          auto segment = packet.add_segments();
          segment->mutable_render_request();
        } else if (segment_json["t"] == "run_program") {
          auto segment = packet.add_segments();
          auto reply = segment->mutable_run_program_request();
          for (auto command_chunk : segment_json["command"]) {
            reply->add_command(command_chunk.get<std::string>());
          }
        } else if (segment_json["t"] == "window_close") {
          auto segment = packet.add_segments();
          auto reply = segment->mutable_window_close_request();
          reply->set_window(
              std::stoll(segment_json["window"].get<std::string>()));
        } else if (segment_json["t"] == "browser_start") {
          auto segment = packet.add_segments();
          segment->mutable_browser_start_request();
        }
      }
      if (packet.segments_size() != 0) {
        size_t len = packet.ByteSizeLong();
        char* buf = (char*)malloc(len);
        packet.SerializeToArray(buf, len);

        nn_send(ipc_sock, buf, len, 0);
        free(buf);
      }

      char* buf;
      int result = nn_recv(ipc_sock, &buf, NN_MSG, 0);

      nlohmann::json to_browser = nlohmann::json::array();

      if (result > 0) {
        Packet packet;
        packet.ParseFromArray(buf, result);

        for (auto segment : packet.segments()) {
          switch (segment.data_case()) {
            case DataSegment::kWindowFocusReply: {
              nlohmann::json obj = {
                  {"t", "window_focus"},
                  {"window",
                   std::to_string(segment.window_focus_reply().window())}};

              to_browser.push_back(obj);

            } break;
            case DataSegment::kWindowMapReply: {
              std::string str_window_type;

              switch (segment.window_map_reply().type()) {
                case WINDOW_TYPE_DESKTOP:
                  str_window_type = "WINDOW_TYPE_DESKTOP";
                  break;
                case WINDOW_TYPE_DOCK:
                  str_window_type = "WINDOW_TYPE_DOCK";
                  break;
                case WINDOW_TYPE_TOOLBAR:
                  str_window_type = "WINDOW_TYPE_TOOLBAR";
                  break;
                case WINDOW_TYPE_MENU:
                  str_window_type = "WINDOW_TYPE_MENU";
                  break;
                case WINDOW_TYPE_UTILITY:
                  str_window_type = "WINDOW_TYPE_UTILITY";
                  break;
                case WINDOW_TYPE_SPLASH:
                  str_window_type = "WINDOW_TYPE_SPLASH";
                  break;
                case WINDOW_TYPE_DIALOG:
                  str_window_type = "WINDOW_TYPE_DIALOG";
                  break;
                case WINDOW_TYPE_DROPDOWN_MENU:
                  str_window_type = "WINDOW_TYPE_DROPDOWN_MENU";
                  break;
                case WINDOW_TYPE_POPUP_MENU:
                  str_window_type = "WINDOW_TYPE_POPUP_MENU";
                  break;
                case WINDOW_TYPE_TOOLTIP:
                  str_window_type = "WINDOW_TYPE_TOOLTIP";
                  break;
                case WINDOW_TYPE_NOTIFICATION:
                  str_window_type = "WINDOW_TYPE_NOTIFICATION";
                  break;
                case WINDOW_TYPE_COMBO:
                  str_window_type = "WINDOW_TYPE_COMBO";
                  break;
                case WINDOW_TYPE_DND:
                  str_window_type = "WINDOW_TYPE_DND";
                  break;
                case WINDOW_TYPE_NORMAL:
                  str_window_type = "WINDOW_TYPE_NORMAL";
                  break;
                default:
                  str_window_type = "WINDOW_TYPE_NORMAL";
                  break;
              }

              nlohmann::json obj = {
                  {"t", "window_map"},
                  {"name", segment.mutable_window_map_reply()->name()},
                  {"has_border",
                   segment.mutable_window_map_reply()->has_border()},
                  {"window",
                   std::to_string(segment.window_map_reply().window())},
                  {"visible", segment.window_map_reply().visible()},
                  {"x", segment.window_map_reply().x()},
                  {"y", segment.window_map_reply().y()},
                  {"width", segment.window_map_reply().width()},
                  {"height", segment.window_map_reply().height()},
                  {"win_t", str_window_type},
              };

              to_browser.push_back(obj);

            } break;
            case DataSegment::kMouseMoveReply: {
              nlohmann::json obj = {{"t", "mouse_move"},
                                    {"x", segment.mouse_move_reply().x()},
                                    {"y", segment.mouse_move_reply().y()}};

              to_browser.push_back(obj);
            } break;
            case DataSegment::kMousePressReply: {
              nlohmann::json obj = {
                  {"t", "mouse_press"},
                  {"state", segment.mouse_press_reply().state()},
                  {"x", segment.mouse_press_reply().x()},
                  {"y", segment.mouse_press_reply().y()}};

              to_browser.push_back(obj);
            } break;
            case DataSegment::kRenderReply: {
              nlohmann::json obj = {
                  {"t", "render_reply"},
                  {"last_frame_observered",
                   segment.render_reply().last_frame_observered()}};
              to_browser.push_back(obj);
            } break;
            case DataSegment::kWindowCloseReply: {
              nlohmann::json obj = {
                  {"t", "window_close"},
                  {"window",
                   std::to_string(segment.window_close_reply().window())},
              };
              to_browser.push_back(obj);
            } break;
            case DataSegment::kReloadReply: {
              nlohmann::json obj = {
                  {"t", "reload"},
              };
              to_browser.push_back(obj);
            } break;

            case DataSegment::kLogMessageReply: {
              nlohmann::json obj = {
                  {"t", "log"},
                  {"message", segment.log_message_reply().message()}};
              to_browser.push_back(obj);
            } break;

            case DataSegment::kWindowIconReply: {
              nlohmann::json obj = {
                  {"t", "window_icon"},
                  {"window",
                   std::to_string(segment.window_icon_reply().window())},
                  {"image", segment.window_icon_reply().image()}};
              to_browser.push_back(obj);
            } break;
            default:
              break;
          }
        }

        nn_freemsg(buf);
      }
      const std::string& url = frame->GetURL();

      callback->Success(to_browser.dump());
    } catch (const std::exception& e) {
      callback->Failure(-1, std::string(e.what()));
    }
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

Client::Client(int* sock) : sock(sock) {}

void Client::OnTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title) {
  // Call the default shared implementation.
  shared::OnTitleChange(browser, "fox-desktop");
}

bool Client::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefProcessId source_process,
                                      CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();

  return message_router_->OnProcessMessageReceived(browser, frame,
                                                   source_process, message);
}

void Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

#if defined(OS_LINUX)
  ::Window window = browser->GetHost()->GetWindowHandle();

  if ((*sock = nn_socket(AF_SP, NN_PAIR)) < 0) {
    printf("nn_socket\n");
  }
  if (nn_connect(*sock, "ipc:///tmp/noko.ipc") < 0) {
    printf("nn_connect\n");
  }

  // non-blocking
  int to = 0;
  if (nn_setsockopt(*sock, NN_SOL_SOCKET, NN_RCVTIMEO, &to, sizeof(to)) < 0) {
    printf("ipc failed\n");
  }

  Packet packet;
  auto segment = packet.add_segments();
  segment->mutable_window_request()->set_window(window);

  printf("initializing base window!\n");

  size_t len = packet.ByteSizeLong();
  char* buf = (char*)malloc(len);
  packet.SerializeToArray(buf, len);

  nn_send(*sock, buf, len, 0);

  free(buf);
#endif

  if (!message_router_) {
    // Create the browser-side router for query handling.
    CefMessageRouterConfig config;
    message_router_ = CefMessageRouterBrowserSide::Create(config);

    // Register handlers with the router.
    message_handler_.reset(new MessageHandler(*sock));
    message_router_->AddHandler(message_handler_.get(), false);
  }

  // Call the default shared implementation.
  shared::OnAfterCreated(browser);
}

bool Client::DoClose(CefRefPtr<CefBrowser> browser) {
  // Call the default shared implementation.
  return shared::DoClose(browser);
}

void Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  // Call the default shared implementation.
  return shared::OnBeforeClose(browser);
}

}  // namespace minimal
