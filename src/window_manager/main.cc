#include <GL/glew.h>
#include <GL/glx.h>

// x11 has conflicts with grpc
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#define X11_Success 0

#undef Status
#undef Bool
#undef True
#undef False
#undef None
#undef Always
#undef Success

#include <sys/time.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <unordered_map>
#include <vector>
#include "main.hpp"
#include "src/protobuf/windowmanager.pb.h"

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

typedef GLXContext (*glXCreateContextAttribsARB_t)(Display*,
                                                   GLXFBConfig,
                                                   GLXContext,
                                                   bool,
                                                   const int*);

typedef void (*glXBindTexImageEXT_t)(Display*, GLXDrawable, int, const int*);
typedef void (*glXReleaseTexImageEXT_t)(Display*, GLXDrawable, int);

typedef void (*glXSwapIntervalEXT_t)(Display*, GLXDrawable, int);

#define NAME "noko"

static void gl_compile_shader_and_check_for_errors /* lmao */ (
    GLuint shader,
    const char* source) {
  glShaderSource(shader, 1, &source, 0);
  glCompileShader(shader);

  GLint log_length;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

  char* log_buffer =
      (char*)malloc(log_length);  // 'log_length' includes null character
  glGetShaderInfoLog(shader, log_length, NULL, log_buffer);

  if (log_length) {
    fprintf(stderr, "[SHADER_ERROR] %s\n", log_buffer);
    exit(1);  // no real need to free 'log_buffer' here
  }

  free(log_buffer);
}

GLuint gl_create_shader_program(const char* vertex_source,
                                const char* fragment_source) {
  GLuint program = glCreateProgram();

  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  gl_compile_shader_and_check_for_errors(vertex_shader, vertex_source);
  gl_compile_shader_and_check_for_errors(fragment_shader, fragment_source);

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);

  glLinkProgram(program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

void gl_create_vao_vbo_ibo(GLuint* vao, GLuint* vbo, GLuint* ibo) {
  glGenVertexArrays(1, vao);
  glBindVertexArray(*vao);

  glGenBuffers(1, vbo);
  glBindBuffer(GL_ARRAY_BUFFER, *vbo);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);

  glGenBuffers(1, ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ibo);  // Bind index buffer here
}

void gl_set_vao_vbo_ibo_data(GLuint vao,
                             GLuint vbo,
                             GLsizeiptr vbo_size,
                             const void* vbo_data,
                             GLuint ibo,
                             GLsizeiptr ibo_size,
                             const void* ibo_data) {
  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vbo_size, vbo_data, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibo_size, ibo_data, GL_STATIC_DRAW);
}

#define glXGetFBConfigAttribChecked(a, b, attr, c)                        \
  if (glXGetFBConfigAttrib((a), (b), (attr), (c))) {                      \
    fprintf(stderr, "WARNING Cannot get FBConfig attribute " #attr "\n"); \
  }

int x11_error_handler(Display* display, XErrorEvent* event) {
  if (!event->resourceid)
    return 0;  // invalid window

  char buffer[1024];
  XGetErrorText(display, event->error_code, buffer, sizeof(buffer));

  printf("XError code = %d, string = %s, resource ID = 0x%lx\n",
         event->error_code, buffer, event->resourceid);

  return 0;
}

void NokoWindowManager::configure_window(Window window,
                                         uint32_t x,
                                         uint32_t y,
                                         uint32_t width,
                                         uint32_t height) {
  XWindowChanges changes;
  changes.x = x;
  changes.y = y;
  changes.width = width;
  changes.height = height;
  XConfigureWindow(display, window, CWX | CWY | CWWidth | CWHeight, &changes);
}

void NokoWindowManager::register_border(Window window,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  if (windows.find(window) == windows.end())
    return;

  windows[window].border = {
      .x = x,
      .y = y,
      .width = width,
      .height = height,
  };
}

void NokoWindowManager::register_base_window(Window base) {
  base_window = base;

  XWMHints hints;
  hints.flags = InputHint;
  hints.input = false;
  XSetWMHints(display, base, &hints);

  XWindowChanges changes;
  changes.x = 0;
  changes.y = 0;
  changes.width = screen_width;
  changes.height = screen_height;
  changes.stack_mode = Below;
  XConfigureWindow(display, base, CWX | CWY | CWWidth | CWHeight | CWStackMode,
                   &changes);
  printf("registering\n");
}

void NokoWindowManager::bind_window_texture(Window window_index) {
  NokoWindow* window = &windows[window_index];

  if (!window->exists)
    return;
  if (!window->visible)
    return;

  // TODO 'XGrabServer'/'XUngrabServer' necessary?
  // it seems to make things 10x faster for whatever reason
  // which is actually good for recording using OBS with XSHM

  XGrabServer(display);
  // glXWaitX(); // same as 'XSync', but a tad more efficient

  // update the window's pixmap

  if (!window->pixmap) {
    XWindowAttributes attribs;
    XGetWindowAttributes(display, window->window, &attribs);

    int format;
    GLXFBConfig config;

    for (int i = 0; i < glx_config_count; i++) {
      config = glx_configs[i];

      int has_alpha;
      glXGetFBConfigAttribChecked(display, config, GLX_BIND_TO_TEXTURE_RGBA_EXT,
                                  &has_alpha);

      XVisualInfo* visual = glXGetVisualFromFBConfig(display, config);
      int visual_depth = visual->depth;
      free(visual);

      if (attribs.depth != visual_depth) {
        continue;
      }

      // found the config we want, break

      format =
          has_alpha ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT;
      break;
    }

    const int pixmap_attributes[] = {
        GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT, GLX_TEXTURE_FORMAT_EXT,
        format, 0  // GLX_TEXTURE_FORMAT_RGB_EXT
    };

    window->x_pixmap = XCompositeNameWindowPixmap(display, window->window);
    window->pixmap =
        glXCreatePixmap(display, config, window->x_pixmap, pixmap_attributes);
  }

  glXBindTexImageEXT(display, window->pixmap, GLX_FRONT_LEFT_EXT, NULL);
}

void NokoWindowManager::run() {
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  while (true) {
    while (process_events())
      ;
    glClearColor(1, 1, 1, 1);
    glClearDepth(1.2);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ipc_step();
    for (auto window : windows) {
      render_window(window.second.window);
    }

    glXSwapBuffers(display, output_window);
  }
}

void NokoWindowManager::render_window(unsigned window_id) {
  NokoWindow* window = &windows[window_id];

  if (!window->exists)
    return;
  if (!window->visible)
    return;

  float gl_x = x_coordinate_to_float(window->x + window->width / 2);
  float gl_y = y_coordinate_to_float(window->y + window->height / 2);
  float gl_width = width_dimension_to_float(window->width);
  float gl_height = height_dimension_to_float(window->height);

  int width_pixels = window->width;
  int height_pixels = window->height;

  if (width_pixels % 2)
    gl_x += 0.5 / screen_width * 2;  // if width odd, add half a pixel to x
  if (height_pixels % 2)
    gl_y +=
        0.5 / screen_height * 2;  // if height odd, subtract half a pixel to y

  // calculate window depth

  float depth = window->depth;
  if (base_window.has_value() && window->window == base_window.value()) {
    depth = 0.9;
  }

  glUseProgram(shader);
  glUniform1i(texture_uniform, 0);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (window->border.has_value() && base_window.has_value() &&
      windows.find(base_window.value()) != windows.end()) {
    glActiveTexture(GL_TEXTURE0);

    bind_window_texture(windows[base_window.value()].window);

    glUniform1f(opacity_uniform, 1);
    glUniform1f(depth_uniform, window->depth + 0.0001);

    glUniform2f(position_uniform, x_coordinate_to_float(screen_width / 2),
                y_coordinate_to_float(screen_height / 2));
    glUniform2f(size_uniform, width_dimension_to_float(screen_width),
                height_dimension_to_float(screen_height));

    uint32_t pixel_border_width =
        window->width - window->border->x + window->border->width;
    uint32_t pixel_border_height =
        window->height - window->border->y + window->border->height;
    float border_x = x_coordinate_to_float(window->border->x + window->x +
                                           pixel_border_width / 2);
    float border_y = y_coordinate_to_float(window->border->y + window->y +
                                           pixel_border_height / 2);
    float border_width = width_dimension_to_float(pixel_border_width);
    float border_height = height_dimension_to_float(pixel_border_height);

    glUniform2f(cropped_position_uniform, border_x, border_y);
    glUniform2f(cropped_size_uniform, border_width, border_height);

    glBindVertexArray(window->vao);
    glDrawElements(GL_TRIANGLES, window->index_count, GL_UNSIGNED_BYTE, NULL);

    unbind_window_texture(windows[base_window.value()].window);
  }

  glActiveTexture(GL_TEXTURE0);
  bind_window_texture(window->window);

  glUniform1f(opacity_uniform, 1);
  glUniform1f(depth_uniform, depth);

  glUniform2f(position_uniform, gl_x, gl_y);
  glUniform2f(size_uniform, gl_width, gl_height);

  glUniform2f(cropped_position_uniform, gl_x, gl_y);
  glUniform2f(cropped_size_uniform, gl_width, gl_height);

  glBindVertexArray(window->vao);
  glDrawElements(GL_TRIANGLES, window->index_count, GL_UNSIGNED_BYTE, NULL);

  unbind_window_texture(window->window);
}

bool NokoWindowManager::process_events() {
  int events_left = XPending(display);

  if (events_left) {
    XEvent event;
    XNextEvent(display, &event);

    if (event.xcookie.type != GenericEvent ||
        event.xcookie.extension != xi_opcode) {
      int type = event.type;

      if (type == CreateNotify) {
        Window x_window = event.xcreatewindow.window;
        if (std::find(blacklisted_windows.begin(), blacklisted_windows.end(),
                      x_window) != blacklisted_windows.end())
          goto done;

        windows[x_window] = {};
        NokoWindow* window = &windows[x_window];

        window->exists = 1;
        window->window = x_window;

        window->opacity = 1.0;
        gl_create_vao_vbo_ibo(&window->vao, &window->vbo, &window->ibo);

        // set up some other stuff for the window
        // this is saying we want focus change and button events from the
        // window

        XSelectInput(display, x_window, FocusChangeMask | PointerMotionMask);
        XGrabButton(display, AnyButton, AnyModifier, x_window, 1,
                    ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                    GrabModeSync, GrabModeSync, 0, 0);

        update_client_list();
        printf("created a window!\n");
        if (base_window.has_value()) {
          XLowerWindow(display, base_window.value());
        }

      } else if (type == ConfigureNotify ||
                 type == MapNotify /* show window */ ||
                 type == UnmapNotify /* hide window */) {
        Window x_window;

        if (type == ConfigureNotify)
          x_window = event.xconfigure.window;
        else if (type == MapNotify)
          x_window = event.xmap.window;
        else if (type == UnmapNotify)
          x_window = event.xunmap.window;

        if (std::find(blacklisted_windows.begin(), blacklisted_windows.end(),
                      x_window) != blacklisted_windows.end())
          goto done;

        if (windows.find(x_window) == windows.end())
          goto done;

        NokoWindow* window = &windows[x_window];

        window->window = x_window;

        // copy everything from the map
        XWindowAttributes attributes;

        XGetWindowAttributes(display, window->window, &attributes);

        window->visible = attributes.map_state == IsViewable;

        window->x = attributes.x;
        window->y = attributes.y;

        window->depth = 0.1;

        window->width = attributes.width;
        window->height = attributes.height;

        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char* prop = nullptr;

        if (XGetWindowProperty(display, window->window, XA_WM_NAME, 0, 1024,
                               false, XA_STRING, &actual_type, &actual_format,
                               &nitems, &bytes_after, &prop) == X11_Success &&
            prop) {
          window->name = std::string((char*)prop);
          printf("set name %s\n", prop);
          XFree(prop);
        }
        // serialize data to client if window not the base window
        if (!base_window.has_value() || base_window.value() != window->window) {
          Packet packet;
          auto segment = packet.add_segments();
          auto reply = segment->mutable_window_map_reply();
          reply->set_window(window->window);
          reply->set_visible(window->visible);
          reply->set_x(window->x);
          reply->set_y(window->y);
          reply->set_width(window->width);
          reply->set_height(window->height);
          if (window->name.has_value()) {
            reply->set_name(window->name.value());
          }

          size_t len = packet.ByteSizeLong();
          char* buf = (char*)malloc(len);
          packet.SerializeToArray(buf, len);

          nn_send(ipc_sock, buf, len, 0);
          free(buf);
        }

        // we're updating the pixel coords
        if (window->x_pixmap) {
          XFreePixmap(display, window->x_pixmap);
          window->x_pixmap = 0;
        }

        if (window->pixmap) {
          glXDestroyPixmap(display, window->pixmap);
          window->pixmap = 0;
        }

        GLfloat vertex_positions[4 * 2] = {
            -1, -1, 1, -1, 1, 1, -1, 1,
        };
        GLubyte indices[6] = {// top tri
                              0, 1, 2,
                              // bottom tri
                              0, 2, 3};

        window->index_count = 6;
        gl_set_vao_vbo_ibo_data(window->vao, window->vbo,
                                sizeof(vertex_positions), vertex_positions,
                                window->ibo, sizeof(indices), indices);

        if (base_window.has_value()) {
          XLowerWindow(display, base_window.value());
        }

      } else if (type == DestroyNotify) {
        Window x_window = event.xdestroywindow.window;
        if (!x_window)
          goto done;

        if (windows[x_window].x_pixmap) {
          XFreePixmap(display, windows[x_window].x_pixmap);
          windows[x_window].x_pixmap = 0;
        }

        if (windows[x_window].pixmap) {
          glXDestroyPixmap(display, windows[x_window].pixmap);
          windows[x_window].pixmap = 0;
        }

        if (base_window.has_value() && x_window != base_window.value() &&
            x_window != 0) {
          Packet packet;
          auto segment = packet.add_segments();
          auto reply = segment->mutable_window_close_reply();
          reply->set_window(x_window);

          size_t len = packet.ByteSizeLong();
          char* buf = (char*)malloc(len);
          packet.SerializeToArray(buf, len);

          nn_send(ipc_sock, buf, len, 0);
        }

        if (windows.find(x_window) == windows.end())
          goto done;

        windows.erase(x_window);

        update_client_list();
      } else if (type == ButtonPress || type == ButtonRelease) {
        Window x_window = event.xbutton.window;

        printf("%i %i %i\n", event.xbutton.window, event.xbutton.button,
               event.xbutton.state);

        float depth = 2.0;
        bool is_border = false;
        for (auto bordered_window : windows) {
          if (!bordered_window.second.border.has_value())
            continue;

          int border_x =
              bordered_window.second.x + bordered_window.second.border->x;
          int border_y =
              bordered_window.second.y + bordered_window.second.border->y;
          int border_x2 = bordered_window.second.x +
                          bordered_window.second.width +
                          bordered_window.second.border->width;
          int border_y2 = bordered_window.second.y +
                          bordered_window.second.height +
                          bordered_window.second.border->height;

          if (event.xbutton.x_root > border_x &&
              event.xbutton.y_root > border_y &&
              event.xbutton.x_root < border_x2 &&
              event.xbutton.y_root < border_y2 &&
              bordered_window.second.depth < depth) {
            is_border = true;
            depth = bordered_window.second.depth;
          }

          if (event.xbutton.x_root > bordered_window.second.x &&
              event.xbutton.y_root > bordered_window.second.y &&
              event.xbutton.x_root <
                  bordered_window.second.x + bordered_window.second.width &&
              event.xbutton.y_root <
                  bordered_window.second.y + bordered_window.second.height &&
              bordered_window.second.depth <= depth) {
            is_border = false;
            depth = bordered_window.second.depth;
          }
        }

        if (is_border && base_window.has_value() &&
            event.xbutton.window != base_window.value()) {
          XAllowEvents(display, SyncPointer, CurrentTime);

          printf("sending border\n");
          XEvent forward_event;
          forward_event.type = type;
          forward_event.xbutton.type = type;
          forward_event.xbutton.window = base_window.value();
          forward_event.xbutton.display = display;
          forward_event.xbutton.send_event = true;
          forward_event.xbutton.root = root_window;
          forward_event.xbutton.subwindow = base_window.value();
          forward_event.xbutton.x_root = event.xbutton.x_root;
          forward_event.xbutton.y_root = event.xbutton.y_root;
          forward_event.xbutton.x = event.xbutton.x_root;
          forward_event.xbutton.y = event.xbutton.y_root;
          forward_event.xbutton.state = event.xbutton.state;
          forward_event.xbutton.button = event.xbutton.button;
          forward_event.xbutton.time = CurrentTime;
          XSendEvent(display, base_window.value(), true,
                     type == ButtonPress ? ButtonPressMask : ButtonReleaseMask,
                     &forward_event);
        } else {
          XAllowEvents(display, ReplayPointer, CurrentTime);
        }

        if (type == ButtonPress && !is_border) {
          focus_window(x_window);
        }

      } else if (type == MotionNotify) {
        if (base_window.has_value() &&
            event.xmotion.window == base_window.value()) {
          goto done;
        }
      } else if (type == KeyPress || type == KeyRelease) {
        // TODO
      }
    } else {
      // xi events only
      Window root_return, child_return;
      int root_x_return, root_y_return;
      int win_x_return, win_y_return;
      unsigned int mask_return;
      int retval = XQueryPointer(display, root_window, &root_return,
                                 &child_return, &root_x_return, &root_y_return,
                                 &win_x_return, &win_y_return, &mask_return);

      if (!retval) {
        goto done;
      }

      // got value, forward to base window

      if (base_window.has_value()) {
        XEvent forward_event;
        forward_event.type = MotionNotify;
        forward_event.xmotion.type = MotionNotify;
        forward_event.xmotion.window = base_window.value();
        forward_event.xmotion.display = display;
        forward_event.xmotion.send_event = true;
        forward_event.xmotion.root = root_window;
        forward_event.xmotion.subwindow = base_window.value();
        forward_event.xmotion.x_root = root_x_return;
        forward_event.xmotion.y_root = root_y_return;
        forward_event.xmotion.x = root_x_return;
        forward_event.xmotion.y = root_y_return;
        forward_event.xmotion.time = CurrentTime;
        XSendEvent(display, base_window.value(), true, PointerMotionMask,
                   &forward_event);
      }
    }
    XFreeEventData(display, &event.xcookie);
  }
done:
  return events_left;
}

void NokoWindowManager::update_client_list() {
  Window* client_list = (Window*)malloc(windows.size() * sizeof(Window));

  size_t i = 0;
  for (auto window : windows) {
    client_list[i] = window.second.window;
    i++;
  }

  XChangeProperty(display, root_window, client_list_atom, XA_WINDOW, 32,
                  PropModeReplace, (unsigned char*)client_list, windows.size());
  free(client_list);
}

void NokoWindowManager::focus_window(Window window) {
  if (base_window.has_value() && window == base_window.value())
    return;

  XSetInputFocus(display, window, RevertToParent, CurrentTime);
  XMapRaised(display, window);

  printf("sending focus!\n");

  Packet packet;
  auto segment = packet.add_segments();
  auto reply = segment->mutable_window_focus_reply();
  reply->set_window(window);

  size_t len = packet.ByteSizeLong();
  char* buf = (char*)malloc(len);
  packet.SerializeToArray(buf, len);

  nn_send(ipc_sock, buf, len, 0);
  free(buf);

  focused_window = window;
}

std::optional<NokoWindowManager*> NokoWindowManager::create() {
  NokoWindowManager* ret = new NokoWindowManager;
  ret->display = XOpenDisplay(NULL);
  if (ret->display == NULL)
    return {};

  XSynchronize(ret->display, 1);

  ret->screen = DefaultScreen(ret->display);
  ret->root_window = DefaultRootWindow(ret->display);

  XWindowAttributes root_attributes;
  XGetWindowAttributes(ret->display, ret->root_window, &root_attributes);

  ret->screen_width = root_attributes.width;
  ret->screen_height = root_attributes.height;

  XSelectInput(ret->display, ret->root_window,
               SubstructureNotifyMask | PointerMotionMask | ButtonMotionMask |
                   ButtonPressMask | ButtonReleaseMask);

  ret->client_list_atom = XInternAtom(ret->display, "_NET_CLIENT_LIST", 0);

  Atom supported_list_atom = XInternAtom(ret->display, "_NET_SUPPORTED", 0);
  Atom supported_atoms[] = {supported_list_atom, ret->client_list_atom};

  XChangeProperty(ret->display, ret->root_window, supported_list_atom, XA_ATOM,
                  32, PropModeReplace, (const unsigned char*)supported_atoms,
                  sizeof(supported_atoms) / sizeof(*supported_atoms));

  // this bit is alegedy some gnome jank
  Atom supporting_wm_check_atom =
      XInternAtom(ret->display, "_NET_SUPPORTING_WM_CHECK", 0);
  Window support_window =
      XCreateSimpleWindow(ret->display, ret->root_window, 0, 0, 1, 1, 0, 0, 0);

  Window support_window_list[1] = {support_window};

  XChangeProperty(ret->display, ret->root_window, supporting_wm_check_atom,
                  XA_WINDOW, 32, PropModeReplace,
                  (const unsigned char*)support_window_list, 1);
  XChangeProperty(ret->display, support_window, supporting_wm_check_atom,
                  XA_WINDOW, 32, PropModeReplace,
                  (const unsigned char*)support_window_list, 1);

  Atom name_atom = XInternAtom(ret->display, "_NET_WM_NAME", 0);
  XChangeProperty(ret->display, support_window, name_atom, XA_STRING, 8,
                  PropModeReplace, (const unsigned char*)NAME, sizeof(NAME));
  // sick

  XSetErrorHandler(x11_error_handler);

  ret->blacklisted_windows.push_back(support_window);

  Window screen_owner =
      XCreateSimpleWindow(ret->display, ret->root_window, 0, 0, 1, 1, 0, 0, 0);
  Xutf8SetWMProperties(ret->display, screen_owner, "xcompmgr", "xcompmgr", NULL,
                       0, NULL, NULL, NULL);

  char name[] = "_NET_WM_CM_S##";
  snprintf(name, sizeof(name), "_NET_WM_CM_S%d", ret->screen);

  Atom atom = XInternAtom(ret->display, name, 0);
  XSetSelectionOwner(ret->display, atom, screen_owner, 0);

  XCompositeRedirectSubwindows(ret->display, ret->root_window,
                               CompositeRedirectManual);

  ret->overlay_window =
      XCompositeGetOverlayWindow(ret->display, ret->root_window);

  // passthrough
  XserverRegion region = XFixesCreateRegion(ret->display, NULL, 0);
  XFixesSetWindowShapeRegion(ret->display, ret->overlay_window, ShapeInput, 0,
                             0, region);
  XFixesDestroyRegion(ret->display, region);

  // create the output window
  // this window is where the actual drawing is going to happen

  /* const */ int default_visual_attributes[] = {GLX_RGBA,
                                                 GLX_DOUBLEBUFFER,
                                                 GLX_SAMPLE_BUFFERS,
                                                 1,
                                                 GLX_SAMPLES,
                                                 4,
                                                 GLX_RED_SIZE,
                                                 8,
                                                 GLX_GREEN_SIZE,
                                                 8,
                                                 GLX_BLUE_SIZE,
                                                 8,
                                                 GLX_ALPHA_SIZE,
                                                 8,
                                                 GLX_DEPTH_SIZE,
                                                 16,
                                                 0};

  XVisualInfo* default_visual =
      glXChooseVisual(ret->display, ret->screen, default_visual_attributes);
  if (!default_visual)
    return {};

  XSetWindowAttributes output_window_attributes = {
      .border_pixel = 0,
      .colormap = XCreateColormap(ret->display, ret->root_window,
                                  default_visual->visual, AllocNone),
  };

  ret->output_window = XCreateWindow(
      ret->display, ret->root_window, 0, 0, (unsigned int)ret->screen_width,
      (unsigned int)ret->screen_height, 0, default_visual->depth, InputOutput,
      default_visual->visual, (unsigned long)(CWBorderPixel | CWColormap),
      &output_window_attributes);

  XReparentWindow(ret->display, ret->output_window, ret->overlay_window, 0, 0);
  XMapRaised(ret->display, ret->output_window);

  const int config_attributes[] = {
      GLX_BIND_TO_TEXTURE_RGBA_EXT, 1, GLX_BIND_TO_TEXTURE_TARGETS_EXT,
      GLX_TEXTURE_2D_BIT_EXT, GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DRAWABLE_TYPE,
      GLX_PIXMAP_BIT, GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR, GLX_X_RENDERABLE, 1,
      GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT, (GLint)GLX_DONT_CARE, GLX_BUFFER_SIZE,
      32,
      //		GLX_SAMPLE_BUFFERS, 1,
      //		GLX_SAMPLES, 4,
      GLX_DOUBLEBUFFER, 1, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
      GLX_ALPHA_SIZE, 8, GLX_STENCIL_SIZE, 0, GLX_DEPTH_SIZE, 16, 0};

  ret->glx_configs = glXChooseFBConfig(
      ret->display, ret->screen, config_attributes, &ret->glx_config_count);
  if (!ret->glx_configs)
    return {};

  // create our OpenGL context
  // we must load the 'glXCreateContextAttribsARB' function ourselves

  const int gl_version_attributes[] = {// we want OpenGL 3.3
                                       GLX_CONTEXT_MAJOR_VERSION_ARB,
                                       3,
                                       GLX_CONTEXT_MINOR_VERSION_ARB,
                                       3,
                                       GLX_CONTEXT_FLAGS_ARB,
                                       GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                                       0};

  glXCreateContextAttribsARB_t glXCreateContextAttribsARB =
      (glXCreateContextAttribsARB_t)glXGetProcAddressARB(
          (const GLubyte*)"glXCreateContextAttribsARB");
  ret->glx_context = glXCreateContextAttribsARB(
      ret->display, ret->glx_configs[0], NULL, 1, gl_version_attributes);
  if (!ret->glx_context)
    return {};

  // load the other two functions we need but don't have

  ret->glXBindTexImageEXT = (glXBindTexImageEXT_t)glXGetProcAddress(
      (const GLubyte*)"glXBindTexImageEXT");
  ret->glXReleaseTexImageEXT = (glXReleaseTexImageEXT_t)glXGetProcAddress(
      (const GLubyte*)"glXReleaseTexImageEXT");

  // finally, make the context we just made the OpenGL context of this thread
  glXMakeCurrent(ret->display, ret->output_window, ret->glx_context);

  // initialize GLEW
  // this will be needed for most modern OpenGL calls

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK)
    return {};

  const char* vertex_shader_source =
      "#version 330\n"
      "layout(location = 0) in vec2 vertex_position;"
      "out vec2 local_position;"

      "uniform float depth;"
      "uniform vec2 position;"
      "uniform vec2 size;"
      "uniform vec2 cropped_position;"
      "uniform vec2 cropped_size;"

      "void main(void) {"
      "   local_position = vertex_position;"
      "   gl_Position = vec4(vertex_position * (cropped_size/2) + "
      "cropped_position, depth, "
      "1.0);"
      "}";

  const char* fragment_shader_source =
      "#version 330\n"
      "in vec2 local_position;"
      "out vec4 fragment_colour;"

      "uniform float opacity;"
      "uniform sampler2D texture_sampler;"
      "uniform vec2 position;"
      "uniform vec2 size;"
      "uniform vec2 cropped_position;"
      "uniform vec2 cropped_size;"

      "void main(void) {"
      "   vec2 uncroped_position = (local_position / "
      "((size/2)/(cropped_size/2)) - position + cropped_position);"
      "   vec4 colour = texture(texture_sampler, uncroped_position  * "
      "vec2(0.5, -0.5) + vec2(0.5));"
      "   float alpha = opacity * colour.a;"
      // "   if(local_position.x < cropped_position.x || local_position.y < "
      // "      cropped_position.y || local_position.x > cropped_position.x + "
      // "      cropped_size.x || local_position.y > cropped_position.y + "
      // "      cropped_size.y) {"
      // "      alpha = 0;"
      // "   }"
      "   fragment_colour = vec4(colour.rgb, alpha);"
      "}";

  ret->shader =
      gl_create_shader_program(vertex_shader_source, fragment_shader_source);
  ret->texture_uniform = glGetUniformLocation(ret->shader, "texture_sampler");

  ret->opacity_uniform = glGetUniformLocation(ret->shader, "opacity");
  ret->depth_uniform = glGetUniformLocation(ret->shader, "depth");

  ret->position_uniform = glGetUniformLocation(ret->shader, "position");
  ret->size_uniform = glGetUniformLocation(ret->shader, "size");

  ret->cropped_position_uniform =
      glGetUniformLocation(ret->shader, "cropped_position");
  ret->cropped_size_uniform = glGetUniformLocation(ret->shader, "cropped_size");

  // blacklist the overlay and output windows for events
  ret->blacklisted_windows.push_back(ret->overlay_window);
  ret->blacklisted_windows.push_back(ret->output_window);

  // https://stackoverflow.com/questions/62448181/how-do-i-monitor-mouse-movement-events-in-all-windows-not-just-one-on-x11#62469861
  int event, error;
  if (!XQueryExtension(ret->display, "XInputExtension", &ret->xi_opcode, &event,
                       &error)) {
    return {};
  }

  int major = 2;
  int minor = 0;
  int retval = XIQueryVersion(ret->display, &major, &minor);
  if (retval != X11_Success) {
    return {};
  }

  unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8] = {0};
  // select direct motion events
  XISetMask(mask_bytes, XI_RawMotion);

  XIEventMask evmasks[1];
  evmasks[0].deviceid = XIAllMasterDevices;
  evmasks[0].mask_len = sizeof(mask_bytes);
  evmasks[0].mask = mask_bytes;
  XISelectEvents(ret->display, ret->root_window, evmasks, 1);

  return ret;
}
void NokoWindowManager::unbind_window_texture(Window window_index) {
  NokoWindow* window = &windows[window_index];

  glXReleaseTexImageEXT(display, window->pixmap, GLX_FRONT_LEFT_EXT);
  XUngrabServer(display);
}

float NokoWindowManager::height_dimension_to_float(int pixels) {
  return (float)pixels / screen_height * 2;
}

float NokoWindowManager::width_dimension_to_float(int pixels) {
  return (float)pixels / screen_width * 2;
}

float NokoWindowManager::x_coordinate_to_float(int pixels) {
  return width_dimension_to_float(pixels) - 1;
}

float NokoWindowManager::y_coordinate_to_float(int pixels) {
  return -height_dimension_to_float(pixels) + 1;
}

int NokoWindowManager::float_to_width_dimension(float x) {
  return (int)round(x / 2 * screen_width);
}

int NokoWindowManager::float_to_height_dimension(float x) {
  return (int)round(x / 2 * screen_height);
}

int NokoWindowManager::float_to_x_coordinate(float x) {
  return float_to_width_dimension(x + 1);
}

int NokoWindowManager::float_to_y_coordinate(float x) {
  return float_to_height_dimension(-x + 1);
}

int main(int argc, char* argv[]) {
  auto wm = NokoWindowManager::create();
  if (!wm.has_value()) {
    printf("wm initialize fail\n");
  }

  int pid = fork();
  if (pid == 0) {
    char* args[] = {
        (char*)"/home/foxmoss/Projects/cef-project/build/Debug/minimal", NULL};
    execv("/home/foxmoss/Projects/cef-project/build/Debug/minimal", args);
    exit(1);
  }

  wm.value()->run();

  return 0;
}
