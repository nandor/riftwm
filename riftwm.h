// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#ifndef __RIFTWM_RIFTWM_H__
#define __RIFTWM_RIFTWM_H__

#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/extensions/composite.h>
#include <openhmd/openhmd.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include "linmath.h"

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C"
{
#endif
typedef void (*glXBindTexImageEXTProc) (Display*, GLXDrawable, int, const int*);
typedef void (*glXReleaseTexImageEXTProc) (Display*, GLXDrawable, int);
typedef struct renderer_t renderer_t;
typedef struct kinect_t   kinect_t;

typedef struct riftwin_t
{
  Window            window;
  GLuint            texture;
  Pixmap            pixmap;
  GLXPixmap         glx_pixmap;
  int               glx_bound;
  int               width;
  int               height;
  int               dirty;
  int               mapped;
  int               focused;
  int               moving;
  char             *name;
  vec3              pos;
  vec3              rot;
  float             r_width;
  float             r_height;

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

  int                        vcursor_x;
  int                        vcursor_y;
  int                        vcursor_show;

  int                        key_up;
  int                        key_down;
  int                        key_left;
  int                        key_right;

  jmp_buf                    err_jmp;
  char                      *err_msg;

  int                        verbose;
  volatile int               running;
  riftwin_t                 *windows;
  int                        window_count;

  int                        has_rift;
  ohmd_context              *rift_ctx;
  ohmd_device               *rift_dev;
  renderer_t                *renderer;
  kinect_t                  *kinect;
  float                      head_pos[3];
} riftwm_t;

// -----------------------------------------------------------------------------
void riftwm_init(riftwm_t *);
void riftwm_run(riftwm_t *);
void riftwm_scan(riftwm_t *);
void riftwm_destroy(riftwm_t *);
void riftwm_restart(riftwm_t *);
void riftwm_error(riftwm_t *, const char *, ...);
void riftwin_update(riftwm_t *, riftwin_t *);

void focus_window(riftwm_t *wm, riftwin_t *win);


#ifdef __cplusplus
}
#endif

#endif
