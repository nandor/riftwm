// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <X11/extensions/composite.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include "riftwm.h"

// -----------------------------------------------------------------------------
// Internal stuff
// -----------------------------------------------------------------------------
static void
create_texture(riftwm_t *wm, riftwin_t *win)
{
  // Retrieve the pixmap from the XComposite
  win->pixmap = XCompositeNameWindowPixmap(wm->dpy, win->window);

  // Create a backing OpenGL pixmap
  const int ATTR[] =
  {
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    None
  };

  if (!(win->glx_pixmap = glXCreatePixmap(wm->dpy, wm->fb_config[1].config,
                                          win->pixmap, ATTR)))
  {
    riftwm_error(wm, "Cannot create GLX pixmap");
  }

  // Create the texture
  glGenTextures(1, &win->texture);
  glBindTexture(GL_TEXTURE_2D, win->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glXBindTexImageEXT(wm->dpy, win->glx_pixmap, GLX_FRONT_LEFT_EXT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void
add_window(riftwm_t *wm, Window window)
{
  riftwin_t *win;
  XWindowAttributes attr;

  // Allocate storage for the structure
  win = (riftwin_t*)malloc(sizeof(riftwin_t));
  memset(win, 0, sizeof(riftwin_t));
  win->window = window;
  win->next = wm->windows;
  wm->windows = win;
  wm->window_count ++;

  // Retrieve properties
  if (!XGetWindowAttributes(wm->dpy, window, &attr)) {
    riftwm_error(wm, "Cannot retrieve window attributes");
  }

  // Check the state of the window
  win->width = attr.width;
  win->height = attr.height;
  switch (attr.map_state) {
    case IsUnmapped:
      break;
    case IsUnviewable:
      break;
    case IsViewable:
      create_texture(wm, win);
      break;
  }
}

static void
free_window(riftwm_t *wm, riftwin_t *win)
{
  if (win->texture) {
    glDeleteTextures(1, &win->texture);
    win->texture = 0;
  }

  if (win->glx_pixmap) {
    glXDestroyPixmap(wm->dpy, win->glx_pixmap);
    win->glx_pixmap = 0;
  }

  free(win);
}

static riftwin_t *
find_window(riftwm_t *wm, Window window)
{
  riftwin_t *win = wm->windows;
  while (win) {
    if (win->window == window) {
      return win;
    }
    win = win->next;
  }

  return NULL;
}

static void
destroy_window(riftwm_t *wm, Window window)
{
  riftwin_t *win = wm->windows, *last = NULL, *tmp;

  while (win) {
    if (win->window == window) {
      tmp = win;
      win = win->next;
      if (last) {
        last->next = win;
      } else {
        wm->windows = win;
      }
      free_window(wm, tmp);
    } else {
      last = win;
      win = win->next;
    }
  }
}

static void
render_window(riftwm_t *wm, riftwin_t *win)
{
  glBindTexture(GL_TEXTURE_2D, win->texture);

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.8f,  0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.8f,  0.8f, 0.0f);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, 0);
}

static void
render_windows(riftwm_t *wm)
{
  glClearColor(0.0f, 1.0f, 0.0f, 0.5f);
  glViewport(0, 0, wm->screen_width, wm->screen_height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);

  riftwin_t *win = wm->windows;
  while (win) {
    render_window(wm, win);
    win = win->next;
  }

  glDisable(GL_TEXTURE_2D);
}

static void
scan_windows(riftwm_t *wm)
{
  Window root, parent, *children;
  unsigned count, i;

  XGrabServer(wm->dpy);

  if (XQueryTree(wm->dpy, wm->root, &root, &parent, &children, &count)) {
    for (i = 0; i < count; ++i) {
      add_window(wm, children[i]);
    }

    if (children) {
      XFree(children);
    }
  }

  XUngrabServer(wm->dpy);
}

// -----------------------------------------------------------------------------
// X Event handlers
// -----------------------------------------------------------------------------
static void
evt_create_notify(riftwm_t *wm, XEvent *evt)
{
  add_window(wm, evt->xcreatewindow.window);
}

static void
evt_destroy_notify(riftwm_t *wm, XEvent *evt)
{
  destroy_window(wm, evt->xdestroywindow.window);
}

static void
evt_configure_request(riftwm_t *wm, XEvent *evt)
{
  riftwin_t *win;
  XConfigureRequestEvent *cfg = &evt->xconfigurerequest;

  if (!(win = find_window(wm, cfg->window))) {
    riftwm_error(wm, "Cannot find findow");
  }


  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = wm->dpy;
  ce.event = win->window;
  ce.window = win->window;
  ce.x = 0;
  ce.y = 0;
  ce.width = 500;
  ce.height = 500;
  ce.border_width = 0;
  ce.above = None;
  ce.override_redirect = False;

  XSendEvent(wm->dpy, win->window, False, StructureNotifyMask, (XEvent*)&ce);
}

static void
evt_map_request(riftwm_t *wm, XEvent *evt)
{
  riftwin_t *win;
  XMapRequestEvent *map = &evt->xmaprequest;

  if (!(win = find_window(wm, map->window))) {
    riftwm_error(wm, "Cannot map window");
  }

  XMoveResizeWindow(wm->dpy, win->window, 0, 0, 500, 500);
  XMapWindow(wm->dpy, win->window);
  create_texture(wm, win);
}

struct
{
  const char *name;
  void (*func) (riftwm_t *wm, XEvent *evt);
}
handlers[LASTEvent] =
{
  [KeyPress]         = { "KeyPress",          NULL },
  [KeyRelease]       = { "KeyRelease",        NULL },
  [ButtonPress]      = { "ButtonPress",       NULL },
  [ButtonRelease]    = { "ButtonRelease",     NULL },
  [MotionNotify]     = { "MotionNotify",      NULL },
  [EnterNotify]      = { "EnterNotify",       NULL },
  [LeaveNotify]      = { "LeaveNotify",       NULL },
  [FocusIn]          = { "FocusIn",           NULL },
  [FocusOut]         = { "FocusOut",          NULL },
  [KeymapNotify]     = { "KeymapNotify",      NULL },
  [Expose]           = { "Expose",            NULL },
  [GraphicsExpose]   = { "GraphicsExpose",    NULL },
  [NoExpose]         = { "NoExpose",          NULL },
  [VisibilityNotify] = { "VisibilityNotify",  NULL },
  [CreateNotify]     = { "CreateNotify",      evt_create_notify     },
  [DestroyNotify]    = { "DestroyNotify",     evt_destroy_notify    },
  [UnmapNotify]      = { "UnmapNotify",       NULL },
  [MapNotify]        = { "MapNotify",         NULL },
  [MapRequest]       = { "MapRequest",        evt_map_request       },
  [ReparentNotify]   = { "ReparentNotify",    NULL },
  [ConfigureNotify]  = { "ConfigureNotify",   NULL },
  [ConfigureRequest] = { "ConfigureRequest",  evt_configure_request },
  [GravityNotify]    = { "GravityNotify",     NULL },
  [ResizeRequest]    = { "ResizeRequest",     NULL },
  [CirculateNotify]  = { "CirculateNotify",   NULL },
  [CirculateRequest] = { "CirculateRequest",  NULL },
  [PropertyNotify]   = { "PropertyNotify",    NULL },
  [SelectionClear]   = { "SelectionClear",    NULL },
  [SelectionRequest] = { "SelectionRequest",  NULL },
  [SelectionNotify]  = { "SelectionNotify",   NULL },
  [ColormapNotify]   = { "ColormapNotify",    NULL },
  [ClientMessage]    = { "ClientMessage",     NULL },
  [MappingNotify]    = { "MappingNotify",     NULL },
  [GenericEvent]     = { "GenericEvent",      NULL },
};

// -----------------------------------------------------------------------------
// Exported API
// -----------------------------------------------------------------------------
void
riftwm_init(riftwm_t *wm)
{
  int event_base, error_base;
  int major, minor, i;

  // Init Xlib
  if (!(wm->dpy = XOpenDisplay(NULL))) {
    riftwm_error(wm, "Cannot open display");
  }

  if (!(wm->screen = XScreenCount(wm->dpy))) {
    riftwm_error(wm, "Cannot get screen count");
  }

  // Check if  the composite extension is available
  if (XCompositeQueryExtension(wm->dpy, &event_base, &error_base)) {
    XCompositeQueryVersion(wm->dpy, &major, &minor);
    if (major < 0 || (major == 0 && minor < 2)) {
      riftwm_error(wm, "XComposite %d.%d < 0.4", major, minor);
    }
  } else {
    riftwm_error(wm, "XComposite unavailable");
  }

  // Check if the damage query extension is available
  if (XDamageQueryExtension(wm->dpy, &event_base, &error_base)) {
    XDamageQueryVersion(wm->dpy, &major, &minor);
    if (major < 1 || (major == 0 && minor < 1)) {
      riftwm_error(wm, "XDamage %d.%d < 1.1", major, minor);
    }
  } else {
    riftwm_error(wm, "XDamage unavailable");
  }

  // Select the oculus screen
  while (--wm->screen >= 0) {
    wm->screen_width = XDisplayWidth(wm->dpy, wm->screen);
    wm->screen_height = XDisplayHeight(wm->dpy, wm->screen);

    if (wm->screen_width == 1280 && wm->screen_height == 800)
    {
      break;
    }
    break;
  }

  if (wm->screen < 0) {
    riftwm_error(wm, "Cannot find oculus screen");
  }

  // Cet the windows on which we'll render
  wm->root = RootWindow(wm->dpy, wm->screen);
  wm->overlay = XCompositeGetOverlayWindow(wm->dpy, wm->root);

  // Retrieve all fb configs
  const int FBATTR[] =
  {
    GLX_X_RENDERABLE, True,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    None
  };

  int count;
  GLXFBConfig *fbs;
  if (!(fbs = glXChooseFBConfig(wm->dpy, wm->screen, FBATTR, &wm->fb_count)) ||
      wm->fb_count < 0)
  {
    if (fbs) {
      XFree(fbs);
    }
    riftwm_error(wm, "Cannot retrieve framebuffer configs");
  }

  wm->fb_config = (riftfb_t*)malloc(sizeof(riftfb_t) * wm->fb_count);
  for (i = 0; i < wm->fb_count; ++i) {
    wm->fb_config[i].config = fbs[i];
    if (glXGetFBConfigAttrib(wm->dpy, fbs[i], GLX_DEPTH_SIZE,
                             &wm->fb_config[i].depth) != Success)
    {
      riftwm_error(wm, "Cannot retrieve fb attributes");
    }
  }

  XFree(fbs);

  // Create an OpenGL Context
  XVisualInfo *vi;
  GLint VIATTR[] =
  {
      GLX_RGBA,
      GLX_DEPTH_SIZE, 24,
      GLX_DOUBLEBUFFER,
      None
  };

  if (!(vi = glXChooseVisual(wm->dpy, 0, VIATTR))) {
    XFree(vi);
    riftwm_error(wm, "Cannot choose visual");
  }

  if (!(wm->context = glXCreateContext(wm->dpy, vi, NULL, GL_TRUE))) {
    XFree(vi);
    riftwm_error(wm, "Cannot create OpenGL context");
  }

  XFree(vi);
  if (!glXMakeCurrent(wm->dpy, wm->overlay, wm->context)) {
    riftwm_error(wm, "Cannot bind OpenGL context");
  }

  // Init GLEW
  if (glewInit() != GLEW_OK) {
    riftwm_error(wm, "Cannot initialise GLEW");
  }

  // Enable compositing
  XCompositeRedirectSubwindows(wm->dpy, wm->root, CompositeRedirectManual);

  // Capture events
  XSelectInput(wm->dpy, wm->root, SubstructureRedirectMask |
                                  SubstructureNotifyMask |
                                  KeyPressMask |
                                  KeyReleaseMask);
}

void
riftwm_run(riftwm_t *wm)
{

  XGrabPointer(wm->dpy, wm->root, True, PointerMotionMask,
               GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
  XGrabKeyboard(wm->dpy, wm->root, True, GrabModeAsync,
                GrabModeAsync, CurrentTime);

  wm->running = 1;
  while (wm->running) {
    // Process events
    while (XPending(wm->dpy) > 0) {
      XEvent evt;
      XNextEvent(wm->dpy, &evt);
      if (evt.type < LASTEvent) {
        fprintf(stderr, "%s %d\n", handlers[evt.type].name,
                                   evt.xany.window);
        if (handlers[evt.type].func) {
          handlers[evt.type].func(wm, &evt);
        }
      }
    }

    // Draw stuff
    render_windows(wm);
    glXSwapBuffers(wm->dpy, wm->overlay);
  }
}

void
riftwm_destroy(riftwm_t *wm)
{
  riftwin_t *win, *tmp;
  win = wm->windows;
  while (win) {
    tmp = win;
    win = win->next;
    free_window(wm, win);
  }

  if (wm->context) {
    glXMakeCurrent(wm->dpy, 0, NULL);
    glXDestroyContext(wm->dpy, wm->context);
    wm->context = NULL;
  }

  if (wm->overlay) {
    XCompositeReleaseOverlayWindow(wm->dpy, wm->overlay);
    wm->overlay = 0;
  }

  if (wm->dpy) {
    XCloseDisplay(wm->dpy);
    wm->dpy = NULL;
  }

  if (wm->err_msg) {
    free(wm->err_msg);
    wm->err_msg = NULL;
  }

  if (wm->fb_config) {
    free(wm->fb_config);
    wm->fb_config = NULL;
  }
}

void
riftwm_error(riftwm_t *wm, const char *fmt, ...)
{
  int size = 100, n;
  char * tmp;
  va_list ap;

  assert(wm->err_msg = (char*)malloc(size));

  while (1) {
    va_start(ap, fmt);
    n = vsnprintf(wm->err_msg, size, fmt, ap);
    va_end(ap);

    if (-1 < n && n < size) {
      break;
    }

    size = n > -1 ? (n + 1) : (size << 1);
    if (!(tmp = (char*)realloc(wm->err_msg, size))) {
      free(wm->err_msg);
      break;
    }

    wm->err_msg = tmp;
  }

  longjmp(wm->err_jmp, 1);
}

int
main()
{
  riftwm_t wm;

  memset(&wm, 0, sizeof(wm));
  if (setjmp(wm.err_jmp)) {
    fprintf(stderr, "Error: %s\n", wm.err_msg);
    riftwm_destroy(&wm);
    return EXIT_FAILURE;
  }

  riftwm_init(&wm);
  scan_windows(&wm);
  riftwm_run(&wm);
  riftwm_destroy(&wm);

  return EXIT_SUCCESS;
}
