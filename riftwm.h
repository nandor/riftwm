// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#ifndef __RIFTWM_RIFTWM_H__
#define __RIFTWM_RIFTWM_H__

#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

typedef struct renderer_t renderer_t;
typedef struct kinect_t   kinect_t;
typedef struct oculus_t   oculus_t;

typedef struct riftwm_t
{
  Display          *dpy;
  int               screen;
  Window            root;
  GLXContext        context;
  int               screen_width;
  int               screen_height;

  jmp_buf           err_jmp;
  char             *err_msg;

  volatile int      running;

  renderer_t       *renderer;
  kinect_t         *kinect;
  oculus_t         *oculus;
} riftwm_t;

void riftwm_init(riftwm_t *);
void riftwm_run(riftwm_t *);
void riftwm_destroy(riftwm_t *);
void riftwm_error(riftwm_t *, const char *, ...);

#endif
