// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <GL/glxew.h>
#include <GL/glew.h>
#include "riftwm.h"

void
riftwm_init(riftwm_t *wm)
{
  // Init Xlib
  if (!(wm->dpy = XOpenDisplay(NULL))) {
    riftwm_error(wm, "Cannot open display");
  }

  if (!(wm->screen = XScreenCount(wm->dpy))) {
    riftwm_error(wm, "Cannot get screen count");
  }

  // Select the oculus screen
  while (--wm->screen >= 0) {
    if ((wm->screen_width = XDisplayWidth(wm->dpy, wm->screen)) == 1280 &&
        (wm->screen_height = XDisplayHeight(wm->dpy, wm->screen)) == 800)
    {
      break;
    }

    // TODO: remove this
    break;
  }

  if (wm->screen < 0) {
    riftwm_error(wm, "Cannot find oculus screen");
  }


  // Check if we can do compositing
  int event_base, error_base;
  if (XCompositeQueryVersion(wm->dpy, &event_base, &error_base)) {
    int major = 0, minor = 2;
    XCompositeQueryVersion(wm->dpy, &major, &minor);
    if (major < 0 || (major == 0 && minor < 0.2)) {
      riftwm_error(wm, "Compositor version %d.%d < 0.2", major, minor);
    }
  }

  wm->root = RootWindow(wm->dpy, wm->screen);

  // Create the OpenGL context
  XVisualInfo *vi;
  GLint ATTR[] =
  {
      GLX_RGBA,
      GLX_DEPTH_SIZE, 24,
      GLX_DOUBLEBUFFER,
      None
  };

  if (!(vi = glXChooseVisual(wm->dpy, 0, ATTR))) {
    XFree(vi);
    riftwm_error(wm, "Cannot choose visual");
  }

  if (!(wm->context = glXCreateContext(wm->dpy, vi, NULL, GL_TRUE))) {
    XFree(vi);
    riftwm_error(wm, "Cannot create OpenGL context");
  }

  XFree(vi);
  if (!glXMakeCurrent(wm->dpy, wm->root, wm->context)) {
    riftwm_error(wm, "Cannot bind OpenGL context");
  }

  XCompositeRedirectSubwindows(wm->dpy, wm->root, CompositeRedirectAutomatic);
  XSelectInput(wm->dpy, wm->root, SubstructureNotifyMask);
}

void
riftwm_run(riftwm_t *wm)
{
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);

  wm->running = 1;
  while (wm->running) {
    while (XPending(wm->dpy)) {
      XEvent evt;

      XNextEvent(wm->dpy, &evt);
      switch (evt.type) {

      }

      fprintf(stderr, "%d\n", evt.type);
    }

    glViewport(0, 0, wm->screen_width, wm->screen_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glXSwapBuffers(wm->dpy, wm->root);
  }
}

void
riftwm_destroy(riftwm_t *wm)
{
  if (wm->context) {
    glXMakeCurrent(wm->dpy, 0, NULL);
    glXDestroyContext(wm->dpy, wm->context);
    wm->context = NULL;
  }

  if (wm->dpy) {
    XCloseDisplay(wm->dpy);
    wm->dpy = NULL;
  }

  if (wm->err_msg) {
    free(wm->err_msg);
    wm->err_msg = NULL;
  }
}

void
riftwm_error(riftwm_t *wm, const char *fmt, ...) {
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
  riftwm_run(&wm);
  riftwm_destroy(&wm);

  return EXIT_SUCCESS;
}
