// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "linmath.h"

typedef struct shader_t
{
  GLuint prog;
} shader_t;

typedef struct fbo_t
{
  GLuint fbo;
  GLuint depth;
  GLuint color;
} fbo_t;

typedef struct renderer_t
{
  float             rot_x;
  float             rot_y;
  vec3              dir;
  vec3              pos;


  fbo_t             leftFBO;
  fbo_t             rightFBO;
  shader_t          warp;

  GLint             u_scr_w;
  GLint             u_scr_h;

  float             aspect;
  riftwm_t         *wm;
} renderer_t;

renderer_t *renderer_init(riftwm_t *wm);
void renderer_frame(renderer_t *);
void renderer_destroy(renderer_t *);

#endif /*__RENDERER_H__*/
