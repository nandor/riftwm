// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <GL/glew.h>
#include <IL/il.h>
#include "riftwm.h"
#include "renderer.h"

static void
texture_load(renderer_t *r, GLuint *tex, const char *src)
{
  glGenTextures(1, tex);
  glBindTexture(GL_TEXTURE_2D, *tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  int image;
  ilGenImages(1, &image);
  ilBindImage(image);
  if (!ilLoadImage(src)) {
    riftwm_error(r->wm, "Cannot load texture %s\n", src);
  }
  ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH),
               ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE,
               ilGetData());
  glGenerateMipmap(GL_TEXTURE_2D);
}

static void
fbo_init(renderer_t *r, fbo_t * fbo, int width, int height)
{
  glGenTextures(1, &fbo->color);
  glBindTexture(GL_TEXTURE_2D, fbo->color);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  glGenTextures(1, &fbo->depth);
  glBindTexture(GL_TEXTURE_2D, fbo->depth);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0,
               GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);

  glGenFramebuffers(1, &fbo->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, fbo->color, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         GL_TEXTURE_2D, fbo->depth, 0);

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  switch (status) {
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      riftwm_error(r->wm, "Framebuffer incomplete attachment");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      riftwm_error(r->wm, "Framebuffer missing attachment");
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      riftwm_error(r->wm, "Framebuffer unsupported");
      break;
  }
}

static GLint
shader_compile(renderer_t * r, const char *src, GLenum type)
{
  int status, info_len;
  char *buffer;
  long len;
  FILE *fin;

  if (!(fin = fopen(src, "rb"))) {
    riftwm_error(r->wm, "Cannot open %s\n", src);
  }

  fseek(fin, 0, SEEK_END);
  len = ftell(fin);
  fseek(fin, 0, SEEK_SET);

  buffer = (char*)malloc(len + 1);
  if (fread(buffer, 1, len, fin) != len) {
    riftwm_error(r->wm, "Cannot read shader %s\n", src);
  }
  buffer[len] = '\0';
  const char *ptr = buffer;

  GLint sh = glCreateShader(type);
  glShaderSource(sh, 1, &ptr, NULL);
  glCompileShader(sh);

  glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &info_len);
    // leak here
    char *buffer = (char*)malloc(sizeof(char) * info_len);
    glGetShaderInfoLog(sh, info_len, NULL, buffer);
    riftwm_error(r->wm, "Shader: %s\n", buffer);
  }

  return sh;
}

static void
shader_init(renderer_t *r, shader_t *s, const char *vs, const char *fs)
{
  int status, info_len;
  GLint prog = glCreateProgram();
  glAttachShader(prog, shader_compile(r, vs, GL_VERTEX_SHADER));
  glAttachShader(prog, shader_compile(r, fs, GL_FRAGMENT_SHADER));
  glLinkProgram(prog);

  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &info_len);
    // leak here
    char *buffer = (char*)malloc(sizeof(char) * info_len);
    glGetProgramInfoLog(prog, info_len, NULL, buffer);
    riftwm_error(r->wm, "Program: %s\n", buffer);
  }

  s->prog = prog;
}

renderer_t *
renderer_init(riftwm_t *wm)
{
  renderer_t *r;

  assert((r = (renderer_t*)malloc(sizeof(renderer_t))));
  memset(r, 0, sizeof(renderer_t));
  r->wm = wm;
  r->aspect = (wm->screen_width / 2.0f) / (float)wm->screen_height;
  r->rot_x = 0.0f;
  r->rot_y = 0.0f;

  r->dir[0] =  0.0f;
  r->dir[1] =  0.0f;
  r->dir[2] =  1.0f;
  r->pos[0] =  0.0f;
  r->pos[1] =  0.0f;
  r->pos[2] = -5.0f;

  fbo_init(r, &r->leftFBO, wm->screen_width >> 1, wm->screen_height);
  fbo_init(r, &r->rightFBO, wm->screen_width >> 1, wm->screen_height);
  shader_init(r, &r->warp, "shader/warp.vs.glsl", "shader/warp.fs.glsl");

  r->u_scr_w = glGetUniformLocation(r->warp.prog, "u_scr_w");
  r->u_scr_h = glGetUniformLocation(r->warp.prog, "u_scr_h");

  texture_load(r, &r->floor, "textures/floor.jpg");
  texture_load(r, &r->sky_xn, "textures/sky_xn.png");
  texture_load(r, &r->sky_xp, "textures/sky_xp.png");
  texture_load(r, &r->sky_yn, "textures/sky_yn.png");
  texture_load(r, &r->sky_yp, "textures/sky_yp.png");
  texture_load(r, &r->sky_zn, "textures/sky_zn.png");
  texture_load(r, &r->sky_zp, "textures/sky_zp.png");

  return r;
}

static void
render_scene(renderer_t *r)
{
  float depth = 0.0f, mat[16], d;

  riftwin_t *win = r->wm->windows;
  while (win) {
    riftwin_update(r->wm, win);
    if (win->mapped) {
      float height = 2.0f;
      float width = win->height * height / win->width;

      glPushMatrix();
      glRotatef((M_PI - r->rot_y)  * 180.0f / M_PI, 0.0f, 1.0f, 0.0f);

      glBindTexture(GL_TEXTURE_2D, win->texture);
      glGenerateMipmap(GL_TEXTURE_2D);
      glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-width,  height, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f( width,  height, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f( width, -height, 0.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-width, -height, 0.0f);
      glEnd();
      glPopMatrix();
    }

    win = win->next;
    depth += 3.0f;
  }

  GLUquadricObj *q = gluNewQuadric();

  glColor3f(1.0f, 0.0f, 0.0f);

  glPushMatrix();
  glTranslatef(-r->leftHand[0], -r->leftHand[1], -r->leftHand[2]);
  glDisable(GL_TEXTURE_2D);
  gluSphere(q, 0.5f, 16, 16);
  glEnable(GL_TEXTURE_2D);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-r->rightHand[0], -r->rightHand[1], -r->rightHand[2]);
  glDisable(GL_TEXTURE_2D);
  gluSphere(q, 0.5f, 16, 16);
  glEnable(GL_TEXTURE_2D);
  glPopMatrix();

  glColor3f(1.0f, 1.0f, 1.0f);

  // Render floor
  glBindTexture(GL_TEXTURE_2D, r->floor);
  glBegin(GL_QUADS);
    glTexCoord2f( 0.0f,  0.0f); glVertex3f(-50.0f, -5.0f,  50.0f);
    glTexCoord2f(10.0f,  0.0f); glVertex3f( 50.0f, -5.0f,  50.0f);
    glTexCoord2f(10.0f, 10.0f); glVertex3f( 50.0f, -5.0f, -50.0f);
    glTexCoord2f( 0.0f, 10.0f); glVertex3f(-50.0f, -5.0f, -50.0f);
  glEnd();

  // Skybox
  glBindTexture(GL_TEXTURE_2D, r->sky_yp);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-50.0f, 50.0f,  50.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 50.0f, 50.0f,  50.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 50.0f, 50.0f, -50.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-50.0f, 50.0f, -50.0f);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, r->sky_xp);
  glBegin(GL_QUADS);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(50.0f, -50.0f,  50.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(50.0f,  50.0f,  50.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(50.0f,  50.0f, -50.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(50.0f, -50.0f, -50.0f);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, r->sky_xn);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-50.0f, -50.0f,  50.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-50.0f,  50.0f,  50.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-50.0f,  50.0f, -50.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-50.0f, -50.0f, -50.0f);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, r->sky_zn);
  glBegin(GL_QUADS);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-50.0f,  50.0f, 50.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 50.0f,  50.0f, 50.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( 50.0f, -50.0f, 50.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-50.0f, -50.0f, 50.0f);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, r->sky_zp);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-50.0f,  50.0f, -50.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 50.0f,  50.0f, -50.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 50.0f, -50.0f, -50.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-50.0f, -50.0f, -50.0f);
  glEnd();
}

void
renderer_frame(renderer_t *r)
{
  float mat[16];

  // Left eye
  glBindFramebuffer(GL_FRAMEBUFFER, r->leftFBO.fbo);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0, 0, r->wm->screen_width >> 1, r->wm->screen_height);

  glMatrixMode(GL_PROJECTION);
  ohmd_device_getf(r->wm->rift_dev, OHMD_LEFT_EYE_GL_PROJECTION_MATRIX, mat);
  glLoadMatrixf(mat);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  ohmd_device_getf(r->wm->rift_dev, OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX, mat);
  glLoadMatrixf(mat);
  glTranslatef(r->pos[0], r->pos[1], r->pos[2]);
  render_scene(r);

  // Right eye
  glBindFramebuffer(GL_FRAMEBUFFER, r->rightFBO.fbo);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0, 0, r->wm->screen_width >> 1, r->wm->screen_height);

  glMatrixMode(GL_PROJECTION);
  ohmd_device_getf(r->wm->rift_dev, OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX, mat);
  glLoadMatrixf(mat);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  ohmd_device_getf(r->wm->rift_dev, OHMD_RIGHT_EYE_GL_MODELVIEW_MATRIX, mat);
  glLoadMatrixf(mat);
  glTranslatef(r->pos[0], r->pos[1], r->pos[2]);
  render_scene(r);

  // Warp
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, r->wm->screen_width, r->wm->screen_height);
  glUseProgram(r->warp.prog);

  glUniform1i(r->u_scr_w, r->wm->screen_width >> 1);
  glUniform1i(r->u_scr_h, r->wm->screen_height);

  glBindTexture(GL_TEXTURE_2D, r->leftFBO.color);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 0.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 0.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, r->rightFBO.color);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f( 0.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f( 0.0f,  1.0f);
  glEnd();

  glUseProgram(0);
}

void
renderer_destroy(renderer_t *r)
{
  if (r) {
    free(r);
  }
}
