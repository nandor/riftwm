// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#ifndef __RIFTWM_RIFTWM_H__
#define __RIFTWM_RIFTWM_H__

#include <setjmp.h>
#include <X11/Xlib.h>

// -----------------------------------------------------------------------------
typedef void (*glXBindTexImageEXTProc) (Display*, GLXDrawable, int, const int*);
typedef void (*glXReleaseTexImageEXTProc) (Display*, GLXDrawable, int);
typedef struct renderer_t renderer_t;
typedef struct kinect_t   kinect_t;
typedef struct oculus_t   oculus_t;

typedef struct riftwin_t
{
  Window            window;
  GLuint            texture;
  Pixmap            pixmap;
  GLXPixmap         glx_pixmap;
  int               glx_bound;
  int               width;
  int               height;

  struct riftwin_t *next;
} riftwin_t;

typedef struct riftfb_t
{
  GLXFBConfig       config;
  int               depth;
} riftfb_t;

typedef struct riftwm_t
{
  Display                   *dpy;
  int                        screen;
  Window                     root;
  Window                     overlay;
  GLXContext                 context;
  riftfb_t                  *fb_config;
  int                        fb_count;
  glXBindTexImageEXTProc     glXBindTexImageEXT;
  glXReleaseTexImageEXTProc  glXReleaseTexImageEXT;

  int                        screen_width;
  int                        screen_height;

  jmp_buf                    err_jmp;
  char                      *err_msg;

  volatile int               running;
  riftwin_t                 *windows;
  int                        window_count;

  renderer_t                *renderer;
  kinect_t                  *kinect;
  oculus_t                  *oculus;
} riftwm_t;

// -----------------------------------------------------------------------------
void riftwm_init(riftwm_t *);
void riftwm_run(riftwm_t *);
void riftwm_scan(riftwm_t *);
void riftwm_destroy(riftwm_t *);
void riftwm_error(riftwm_t *, const char *, ...);

#endif
