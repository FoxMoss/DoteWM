#include <GL/glew.h>
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <sys/time.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <unordered_map>
#include <vector>

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

typedef GLXContext (*glXCreateContextAttribsARB_t)(Display*,
                                                   GLXFBConfig,
                                                   GLXContext,
                                                   Bool,
                                                   const int*);

typedef void (*glXBindTexImageEXT_t)(Display*, GLXDrawable, int, const int*);
typedef void (*glXReleaseTexImageEXT_t)(Display*, GLXDrawable, int);

typedef void (*glXSwapIntervalEXT_t)(Display*, GLXDrawable, int);

#define NAME "noko"

void gl_create_vao_vbo_ibo(GLuint* vao, GLuint* vbo, GLuint* ibo) {
  glGenVertexArrays(1, vao);
  glBindVertexArray(*vao);

  glGenBuffers(1, vbo);
  glBindBuffer(GL_ARRAY_BUFFER, *vbo);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);

  glGenBuffers(1, ibo);
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

int x11_error_handler(Display* display, XErrorEvent* event) {
  if (!event->resourceid)
    return 0;  // invalid window

  char buffer[1024];
  XGetErrorText(display, event->error_code, buffer, sizeof(buffer));

  printf("XError code = %d, string = %s, resource ID = 0x%lx\n",
         event->error_code, buffer, event->resourceid);

  return 0;
}

struct NokoWindow {
  int exists;
  Window window;

  int visible;

  float opacity;
  int x, y;
  int width, height;

  Pixmap x_pixmap;
  GLXPixmap pixmap;

  GLuint vao, vbo, ibo;
};

class NokoWindowManager {
 public:
  static std::optional<NokoWindowManager*> create() {
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

    XChangeProperty(ret->display, ret->root_window, supported_list_atom,
                    XA_ATOM, 32, PropModeReplace,
                    (const unsigned char*)supported_atoms,
                    sizeof(supported_atoms) / sizeof(*supported_atoms));

    // this bit is alegedy some gnome jank
    Atom supporting_wm_check_atom =
        XInternAtom(ret->display, "_NET_SUPPORTING_WM_CHECK", 0);
    Window support_window = XCreateSimpleWindow(ret->display, ret->root_window,
                                                0, 0, 1, 1, 0, 0, 0);

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

    Window screen_owner = XCreateSimpleWindow(ret->display, ret->root_window, 0,
                                              0, 1, 1, 0, 0, 0);
    Xutf8SetWMProperties(ret->display, screen_owner, "xcompmgr", "xcompmgr",
                         NULL, 0, NULL, NULL, NULL);

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

    XReparentWindow(ret->display, ret->output_window, ret->overlay_window, 0,
                    0);
    XMapRaised(ret->display, ret->output_window);

    const int config_attributes[] = {
        GLX_BIND_TO_TEXTURE_RGBA_EXT, 1, GLX_BIND_TO_TEXTURE_TARGETS_EXT,
        GLX_TEXTURE_2D_BIT_EXT, GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT, GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_X_RENDERABLE, 1, GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT,
        (GLint)GLX_DONT_CARE, GLX_BUFFER_SIZE, 32,
        //		GLX_SAMPLE_BUFFERS, 1,
        //		GLX_SAMPLES, 4,
        GLX_DOUBLEBUFFER, 1, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE,
        8, GLX_ALPHA_SIZE, 8, GLX_STENCIL_SIZE, 0, GLX_DEPTH_SIZE, 16, 0};

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

    // blacklist the overlay and output windows for events
    ret->blacklisted_windows.push_back(ret->overlay_window);
    ret->blacklisted_windows.push_back(ret->output_window);

    gettimeofday(&ret->previous_time, 0);

    return ret;
  }

  void run() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    while (true) {
      // glClearColor(0.4, 0.2, 0.4, 1.0);
      // gruvbox background colour (#292828)
      glClearColor(1, 1, 1, 1.);

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glXSwapBuffers(display, output_window);

      // render our windows

      // for (int i = 0; i < window_count; i++) {
      //   render_window(wm, i, average_delta);
      // }
      //
      // float delta = (float)cwm_swap(&wm->cwm) / 1000000;
      //
      // average_delta += delta;
      // average_delta /= 2;
    }
  }

  NokoWindowManager() {}
  ~NokoWindowManager() {}

 private:
  Display* display;
  int screen;
  Window root_window;
  Window overlay_window;
  Window output_window;
  Atom client_list_atom;

  size_t global_window_ticker = 0;

  std::vector<Window> blacklisted_windows;
  std::unordered_map<Window, NokoWindow> windows;

  GLXFBConfig* glx_configs;
  int glx_config_count;
  GLXContext glx_context;

  glXBindTexImageEXT_t glXBindTexImageEXT;
  glXReleaseTexImageEXT_t glXReleaseTexImageEXT;

  uint32_t screen_width;
  uint32_t screen_height;

  struct timeval previous_time;

  void process_events() {
    int events_left = XPending(display);

    if (events_left) {
      XEvent event;
      XNextEvent(display, &event);

      int type = event.type;

      if (type == CreateNotify) {
        Window x_window = event.xcreatewindow.window;
        if (std::find(blacklisted_windows.begin(), blacklisted_windows.end(),
                      x_window) == blacklisted_windows.end())
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

        XSelectInput(display, x_window, FocusChangeMask);
        XGrabButton(display, AnyButton, AnyModifier, x_window, 1,
                    ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                    GrabModeSync, GrabModeSync, 0, 0);
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
                      x_window) == blacklisted_windows.end())
          goto done;

        if (windows.find(x_window) == windows.end())
          goto done;

        NokoWindow* window = &windows[x_window];

        int was_visible = window->visible;

        // copy everything from the map
        XWindowAttributes attributes;

        XGetWindowAttributes(display, window->window, &attributes);

        window->visible = attributes.map_state == IsViewable;

        window->x = attributes.x;
        window->y = attributes.y;

        window->width = attributes.width;
        window->height = attributes.height;

        // if window wasn't visible before but is now, center it to the cursor
        // position

        if (window->visible && !was_visible && !window->x && !window->y) {
          __attribute__((unused)) Window rw, cw;  // root_return, child_return
          __attribute__((unused)) int wx, wy;     // win_x_return, win_y_return
          __attribute__((unused)) unsigned int mask;  // mask_return

          int x, y;
          XQueryPointer(display, window->window, &rw, &cw, &x, &y, &wx, &wy,
                        &mask);

          window->x = x - window->width / 2;
          window->y = y - window->height / 2;

          XMoveWindow(display, window->window, window->x, window->y);
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

        GLfloat vertex_positions[4];
        GLubyte indices[3];

        if (wm->modify_event_callback) {
          wm->modify_event_callback(
              thing, window_index, window->visible,
              wm_x_coordinate_to_float(wm, window->x + window->width / 2),
              wm_y_coordinate_to_float(wm, window->y + window->height / 2),
              wm_width_dimension_to_float(wm, window->width),
              wm_height_dimension_to_float(wm, window->height));
        }
      }
    }
  done:
    return;
  }
};

int main(int argc, char* argv[]) {
  auto wm = NokoWindowManager::create();
  if (!wm.has_value()) {
    printf("wm initialize fail\n");
  }

  wm.value()->run();

  return 0;
}
