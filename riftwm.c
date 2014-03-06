// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <X11/extensions/Xcomposite.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include "riftwm.h"

// -----------------------------------------------------------------------------
// Unique instance referenced by signal handlers
// -----------------------------------------------------------------------------
static riftwm_t wm;

// -----------------------------------------------------------------------------
// Internal stuff
// -----------------------------------------------------------------------------
static void
create_texture(riftwm_t *wm, riftwin_t *win)
{
  XWindowAttributes attr;

  // Retrieve properties
  if (!XGetWindowAttributes(wm->dpy, win->window, &attr)) {
    riftwm_error(wm, "Cannot retrieve window attributes");
  }

  win->width = attr.width;
  win->height = attr.height;
  if (attr.map_state != IsViewable) {
    return;
  }

  // Free old resources
  if (win->glx_pixmap) {
    glBindTexture(GL_TEXTURE_2D, win->texture);
    wm->glXReleaseTexImageEXT(wm->dpy, win->glx_pixmap, GLX_FRONT_LEFT_EXT);
    glBindTexture(GL_TEXTURE_2D, 0);

    glXDestroyPixmap(wm->dpy, win->glx_pixmap);
    win->glx_pixmap = 0;
  }

  if (win->pixmap) {
    XFreePixmap(wm->dpy, win->pixmap);
    win->pixmap = 0;
  }

  if (!win->texture) {
    glGenTextures(1, &win->texture);
  }

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
  glBindTexture(GL_TEXTURE_2D, win->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  wm->glXBindTexImageEXT(wm->dpy, win->glx_pixmap, GLX_FRONT_LEFT_EXT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  win->dirty = 0;
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

static riftwin_t *
add_window(riftwm_t *wm, Window window)
{
  riftwin_t *win;

  // Check whether the window is already managed by us
  if (!(win = find_window(wm, window)))
  {
    // Allocate storage for the structure
    win = (riftwin_t*)malloc(sizeof(riftwin_t));
    memset(win, 0, sizeof(riftwin_t));
    win->window = window;
    win->next = wm->windows;
    win->dirty = 1;
    win->mapped = 0;
    wm->windows = win;
    wm->window_count++;
  }

  return win;
}

static void
free_window(riftwm_t *wm, riftwin_t *win)
{
  if (win->glx_pixmap) {
    glBindTexture(GL_TEXTURE_2D, win->texture);
    wm->glXReleaseTexImageEXT(wm->dpy, win->glx_pixmap, GLX_FRONT_LEFT_EXT);
    glBindTexture(GL_TEXTURE_2D, 0);
    glXDestroyPixmap(wm->dpy, win->glx_pixmap);
    win->glx_pixmap = 0;
  }

  if (win->pixmap) {
    XFreePixmap(wm->dpy, win->pixmap);
    win->pixmap = 0;
  }

  if (win->texture) {
    glDeleteTextures(1, &win->texture);
    win->texture = 0;
  }

  free(win);
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

      wm->window_count--;
      free_window(wm, tmp);
    } else {
      last = win;
      win = win->next;
    }
  }
}

static void
update_window(riftwm_t *wm, riftwin_t *win)
{
}

static void
update_windows(riftwm_t *wm)
{
  riftwin_t *win = wm->windows;
  while (win) {
    update_window(wm, win);
    win = win->next;
  }
}

static void
render_window(riftwm_t *wm, riftwin_t *win)
{
  if (win->dirty) {
    create_texture(wm, win);
  }

  glBindTexture(GL_TEXTURE_2D, win->texture);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.8f,  0.8f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.8f,  0.8f, 0.0f);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void
render_windows(riftwm_t *wm)
{
  glClearColor(1.0f, 1.0f, 0.0f, 0.5f);
  glViewport(0, 0, wm->screen_width, wm->screen_height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);

  riftwin_t *win = wm->windows;
  while (win) {
    if (win->mapped) {
      render_window(wm, win);
    }
    win = win->next;
  }

  glDisable(GL_TEXTURE_2D);
}

static void
scan_windows(riftwm_t *wm)
{
  Window root, parent, *children;
  XWindowAttributes attr;
  unsigned count, i;
  riftwin_t * win;

  XGrabServer(wm->dpy);
  if (XQueryTree(wm->dpy, wm->root, &root, &parent, &children, &count)) {
    for (i = 0; i < count; ++i) {
      win = add_window(wm, children[i]);
      if (!XGetWindowAttributes(wm->dpy, win->window, &attr)) {
        riftwm_error(wm, "Cannot retrieve window attributes");
      }

      win->mapped = attr.map_state == IsViewable;
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
evt_key_press(riftwm_t *wm, XEvent *evt)
{
  KeySym keysym;

  keysym = XLookupKeysym(&evt->xkey, 0);
  switch (keysym) {
    case XK_Escape:
      wm->running = 0;
      break;
    case XK_F1:
      riftwm_restart(wm);
      break;
    case XK_F2:
      system("subl &");
      break;
  }
}

static void
evt_create_notify(riftwm_t *wm, XEvent *evt)
{
  riftwin_t *win;
  win = add_window(wm, evt->xcreatewindow.window);
  win->dirty = 1;
}

static void
evt_configure_request(riftwm_t *wm, XEvent *evt)
{
  XConfigureRequestEvent *cfg = &evt->xconfigurerequest;
  riftwin_t *win;

  win = add_window(wm, cfg->window);
  win->dirty = 1;

  // Send a message to the window giving it a default size
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
  win = add_window(wm, evt->xmaprequest.window);
  win->dirty = 1;

  XMoveResizeWindow(wm->dpy, win->window, 0, 0, 500, 500);
  XMapWindow(wm->dpy, win->window);
  XFlush(wm->dpy);
}

static void
evt_map_notify(riftwm_t *wm, XEvent *evt)
{
  riftwin_t *win;
  win = add_window(wm, evt->xmap.window);
  win->mapped = 1;
  win->dirty = 1;
}

static void
evt_destroy_notify(riftwm_t *wm, XEvent *evt)
{
  destroy_window(wm, evt->xdestroywindow.window);
}

static void
evt_unmap_notify(riftwm_t *wm, XEvent *evt)
{
  destroy_window(wm, evt->xunmap.window);
}

static void
evt_configure_notify(riftwm_t *wm, XEvent *evt)
{
  riftwin_t *win;
  win = add_window(wm, evt->xconfigure.window);
  win->dirty = 1;
}

struct
{
  const char *name;
  void (*func) (riftwm_t *wm, XEvent *evt);
}
handlers[LASTEvent] =
{
  [KeyPress]         = { "KeyPress",          evt_key_press         },
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
  [UnmapNotify]      = { "UnmapNotify",       evt_unmap_notify      },
  [MapNotify]        = { "MapNotify",         evt_map_notify        },
  [MapRequest]       = { "MapRequest",        evt_map_request       },
  [ReparentNotify]   = { "ReparentNotify",    NULL },
  [ConfigureNotify]  = { "ConfigureNotify",   evt_configure_notify  },
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

  // Link GLX routines
  if (!(wm->glXBindTexImageEXT = (glXBindTexImageEXTProc)
          glXGetProcAddress("glXBindTexImageEXT")) ||
      !(wm->glXReleaseTexImageEXT = (glXReleaseTexImageEXTProc)
          glXGetProcAddress("glXReleaseTexImageEXT")))
  {
    riftwm_error(wm, "Cannot bind GLX_EXT_texture_from_pixmap");
  }

  // Init GLEW
  if (glewInit() != GLEW_OK) {
    riftwm_error(wm, "Cannot initialise GLEW");
  }

  XCompositeRedirectSubwindows(wm->dpy, wm->root, CompositeRedirectAutomatic);

  // Capture events
  XSelectInput(wm->dpy, wm->root, SubstructureRedirectMask |
                                  SubstructureNotifyMask |
                                  KeyPressMask |
                                  KeyReleaseMask);

  XSync(wm->dpy, False);
}

void
riftwm_restart(riftwm_t * wm)
{
  struct stat st;
  int length, read;
  char *path;

  if (lstat("/proc/self/exe", &st) == -1) {
    riftwm_error(wm, "Cannot stat /proc/self/exe");
  }

  length = read = 0;
  while (read >= length) {
    length += 128;
    path = (char*)malloc(sizeof(char) * length);
    if ((read = readlink("/proc/self/exe", path, length)) < 0) {
      riftwm_error(wm, "Cannot readlink /proc/self/exe");
    }

    if (read >= length) {
      free(path);
    }
  }

  path[read] = '\0';

  fprintf(stderr, "Restarting ...\n");
  if (execv(path, NULL) < 0) {
    riftwm_error(wm, "Restart failed");
  }
}

void
riftwm_run(riftwm_t *wm)
{
  XEvent evt;

  XGrabPointer(wm->dpy, wm->root, True, PointerMotionMask,
               GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
  XGrabKeyboard(wm->dpy, wm->root, True, GrabModeAsync,
                GrabModeAsync, CurrentTime);

  wm->running = 1;
  while (wm->running) {
    // Process events
    while (XPending(wm->dpy) > 0) {
      XNextEvent(wm->dpy, &evt);
      if (evt.type < LASTEvent) {
        if (wm->verbose) {
          fprintf(stderr, "Event %s\n", handlers[evt.type].name);
        }
        if (handlers[evt.type].func) {
          handlers[evt.type].func(wm, &evt);
        }
      }
    }

    // Update window attributes
    update_windows(wm);
    XFlush(wm->dpy);

    // Display the windows
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
    free_window(wm, tmp);
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

  if (wm->err_msg) {
    free(wm->err_msg);
    wm->err_msg = NULL;
  }

  if (wm->fb_config) {
    free(wm->fb_config);
    wm->fb_config = NULL;
  }

  XSync(wm->dpy, True);

  if (wm->dpy) {
    XCloseDisplay(wm->dpy);
    wm->dpy = NULL;
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

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------
static void
usage()
{
  puts("RiftWM v0.0.1");
  puts("Usage:");
  puts("\triftwm [options]");
  puts("Options:");
  puts("\t--quiet: Don't print stuff\n");
}

int
main(int argc, char **argv)
{
  int c, opt_idx;
  static struct option options[] =
  {
    { "help",    no_argument, NULL,        0   },
    { "quiet",   no_argument, &wm.verbose, 0   },
    { NULL,      0,           NULL,        0   }
  };

  // Parse command line arguments
  memset(&wm, 0, sizeof(wm));
  wm.verbose = 1;
  while ((c = getopt_long(argc, argv, "rh", options, &opt_idx)) != -1) {
    switch (c) {
      // A flag was set
      case 0:
        if (options[opt_idx].flag) {
          break;
        }
        break;
      // Help was requested or wrong commands given
      case 'h':
        usage();
        return EXIT_SUCCESS;
      case '?':
      default:
        usage();
        return EXIT_FAILURE;
    }
  }

  // Do something with unmatched arguments
  while (optind < argc) {
    ++optind;
  }

  // Register error handler
  if (setjmp(wm.err_jmp)) {
    fprintf(stderr, "Error: %s\n", wm.err_msg);
    riftwm_destroy(&wm);
    return EXIT_FAILURE;
  }

  // Run the wm
  riftwm_init(&wm);
  scan_windows(&wm);
  riftwm_run(&wm);
  riftwm_destroy(&wm);
  return EXIT_SUCCESS;
}
