// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include "riftwm.h"

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
  XCompositeRedirectSubwindows(wm->dpy, wm->root, CompositeRedirectAutomatic);
  XSelectInput(wm->dpy, wm->root, SubstructureRedirectMask);
}

void
riftwm_add_window(riftwm_t *wm, Window window)
{
  riftwin_t *wnd;

  wnd = (riftwin_t*)malloc(sizeof(riftwin_t));
  wnd->window = window;
  wnd->next = wm->windows;
  wm->windows = wnd;

  // Retrieve properties
  wnd->width = 500;
  wnd->height = 500;

  // Retrieve the pixmap from the XComposite
  wnd->pixmap = XCompositeNameWindowPixmap(wm->dpy, wnd->window);

  // Create a backing OpenGL pixmap
  const int ATTR[] =
  {
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    None
  };

  if (!(wnd->glx_pixmap = glXCreatePixmap(wm->dpy, wm->fb_config[1].config,
                                          wnd->pixmap, ATTR)))
  {
    riftwm_error(wm, "Cannot create GLX pixmap");
  }

  // Create the texture
  glGenTextures(1, &wnd->texture);
  glBindTexture(GL_TEXTURE_2D, wnd->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glXBindTexImageEXT(wm->dpy, wnd->glx_pixmap, GLX_FRONT_LEFT_EXT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
  //glXReleaseTexImageEXT(wm->dpy, wnd->glx_pixmap, GLX_FRONT_LEFT_EXT);
}

void
riftwm_delete_window(riftwm_t *wm, riftwin_t *win)
{

}

void
riftwm_scan(riftwm_t *wm)
{
  Window root, parent, *children;
  unsigned count, i;

  if (XQueryTree(wm->dpy, wm->root, &root, &parent, &children, &count)) {
    for (i = 0; i < count; ++i) {
      riftwm_add_window(wm, children[i]);
    }

    if (children) {
      XFree(children);
    }
  }
}

void
riftwm_run(riftwm_t *wm)
{
  XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XStringToKeysym("F1")), Mod1Mask,
           wm->root, True, GrabModeAsync, GrabModeAsync);

  wm->running = 1;
  XSync(wm->dpy, False);
  XFlush(wm->dpy);
  while (wm->running) {
    while (XPending(wm->dpy) > 0) {
      XEvent evt;

      XNextEvent(wm->dpy, &evt);
      switch (evt.type) {
        case KeyPress:
          system("export DISPLAY=:1.0 && glxgears &");
          break;
        case CreateNotify:
          riftwm_add_window(wm, evt.xcreatewindow.window);
          break;
        case ConfigureNotify:
          fprintf(stderr, "ConfigureNotify\n");
          break;
        case DestroyNotify:
          fprintf(stderr, "DestroyNotify\n");
          break;
        case MapNotify:
          fprintf(stderr, "MapNotify\n");
          break;
        case UnmapNotify:
          fprintf(stderr, "UnmapNotify\n");
          break;
        case ReparentNotify:
          fprintf(stderr, "ReparentNotify\n");
          break;
        case CirculateNotify:
          fprintf(stderr, "CirculateNotify\n");
          break;
      }

      fprintf(stderr, "Event: %d", evt.type);
    }

    glClearColor(0.0f, 1.0f, 0.0f, 0.5f);
    glViewport(0, 0, wm->screen_width, wm->screen_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, wm->windows->texture);
    glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
      glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
      glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.8f,  0.8f, 0.0f);
      glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.8f,  0.8f, 0.0f);
    glEnd();

    glXSwapBuffers(wm->dpy, wm->overlay);
  }
}

void
riftwm_destroy(riftwm_t *wm)
{
  riftwin_t *win, *tmp;
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

  win = wm->windows;
  while (win) {
    tmp = win;
    win = win->next;
    free(tmp);
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
  riftwm_scan(&wm);
  riftwm_run(&wm);
  riftwm_destroy(&wm);

  return EXIT_SUCCESS;
}
