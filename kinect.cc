// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OpenNI.h>
#include <NiTE.h>
#include "riftwm.h"
#include "kinect.h"
#include "renderer.h"
#include "linmath.h"

#define SAMPLES 20

typedef struct smooth_t
{
  int index;
  int wrap;
  float pos[SAMPLES][3];
} smooth_t;

typedef struct kinect_t
{
  riftwm_t      *wm;
  renderer_t    *r;
  nite::UserTracker* tracker;
  nite::UserId first_id;
  bool found;
  riftwin_t       *focused;

  float x_shift,y_shift,z_shift;
  smooth_t       s_pos;
} kinect_t;

static void
smooth_add(smooth_t *s, float x, float y, float z)
{
  s->pos[s->index][0] = x;
  s->pos[s->index][1] = y;
  s->pos[s->index][2] = z;

  ++s->index;
  if (s->index >= SAMPLES) {
    s->index = 0;
    s->wrap = 1;
  }
}

static void
smooth_get(smooth_t *s, float *x, float *y, float *z)
{
  float acc[3] = { 0.0f, 0.0f, 0.0f };
  int count = 0;

  if (!s->wrap) {
    for (int i = 0; i < s->index; ++i) {
      acc[0] += s->pos[i][0];
      acc[1] += s->pos[i][1];
      acc[2] += s->pos[i][2];
      ++count;
    }
  } else {
    for (int i = 0; i < SAMPLES; ++i) {
      acc[0] += s->pos[i][0];
      acc[1] += s->pos[i][1];
      acc[2] += s->pos[i][2];
      ++count;
    }
  }

  *z = acc[0] / count;
  *y = acc[1] / count;
  *z = acc[2] / count;
}

kinect_t *
kinect_init(riftwm_t * wm)
{
  kinect_t *k;
  k = new kinect_t;
  k->wm = wm;
  k->r = wm->renderer;

  if (openni::OpenNI::initialize() != openni::STATUS_OK)
  {
    riftwm_error(wm,"Couldnt initialize OpenNI");
  }
  if (nite::NiTE::initialize() != nite::STATUS_OK)
  {
    riftwm_error(wm,"Couldnt initialize NiTE");
  }

  k->tracker = new nite::UserTracker();
  if ( k->tracker->create() != nite::STATUS_OK)
  {
    riftwm_error(wm,"Couldnt initialize UserTracker");
  }

  k->x_shift = 0.0f;
  k->y_shift = 0.0f;
  k->z_shift = 0.0f;
  k->s_pos.wrap = 0;
  k->focused = NULL;
  k->found = false;
  return k;
}

bool signal(nite::Point3f hand, nite::Point3f elbow)
{
  printf("hand %f %f %f\n", hand.x, hand.y, hand.z);
  float ydiff = hand.y - elbow.y;
  float dist = sqrt((hand.x - elbow.x)*(hand.x - elbow.x) +
            (hand.y - elbow.y)*(hand.y - elbow.y) +
            (hand.z - elbow.z)*(hand.z - elbow.z));

  return (ydiff/dist - 1)*(ydiff/dist - 1) < 0.002;
}

// REALLY BAD THING, but i'm too tired
static float ratio = 0.0f;

void
kinect_update(kinect_t *k)
{
  nite::UserTrackerFrameRef frame;
  k->tracker->readFrame(&frame);
  const nite::Array<nite::UserData>& users = frame.getUsers();
  if (!users.isEmpty())
  {

    for (int i = 0; i < users.getSize(); ++i)
    {
      const nite::Skeleton & skeleton = users[i].getSkeleton();

      if (skeleton.getState() == nite::SKELETON_TRACKED)
      {
        if (!k->found) {
          k->found = true;
          k->first_id = users[i].getId();
        }

        if (users[i].getId() == k->first_id)
        {
          const nite::SkeletonJoint & head = skeleton.getJoint(nite::JOINT_HEAD);
          const nite::SkeletonJoint & left_hand = skeleton.getJoint(nite::JOINT_LEFT_HAND);
          const nite::SkeletonJoint & left_elbow = skeleton.getJoint(nite::JOINT_LEFT_ELBOW);
          const nite::SkeletonJoint & right_hand = skeleton.getJoint(nite::JOINT_RIGHT_HAND);
          const nite::Point3f & head_point = head.getPosition();
          const nite::Point3f & left_hand_point = left_hand.getPosition();
          const nite::Point3f & right_hand_point = right_hand.getPosition();
          const nite::Point3f & left_elbow_point = left_elbow.getPosition();
          float hx, hy, hz;

          if (!k->wm->renderer->has_origin) {
            k->r->has_origin = 1;
            k->x_shift = head_point.x;
            k->y_shift = head_point.y;
            k->z_shift = head_point.z;
          }


          // Update the head position
          hx = head_point.x, hy = head_point.y, hz = head_point.z;
          smooth_add(&k->s_pos, head_point.x, head_point.y, head_point.z);
          smooth_get(&k->s_pos, &hx, &hy, &hz);
          k->r->pos[0] = (k->x_shift - hx) / 200.0f - k->r->origin[0];
          k->r->pos[1] = (k->y_shift - hy) / 200.0f - k->r->origin[1];
          k->r->pos[2] = (k->z_shift - hz) / 200.0f - k->r->origin[2];

          // Left hand
          hx = left_hand_point.x, hy = left_hand_point.y, hz = left_hand_point.z;
          k->r->leftHand[0] = (k->x_shift - hx) / 200.0f - k->r->origin[0];
          k->r->leftHand[1] = (k->y_shift - hy) / 200.0f - k->r->origin[1];
          k->r->leftHand[2] = (k->z_shift - hz) / 200.0f - k->r->origin[2];

          // Right hand
          hx = right_hand_point.x, hy = right_hand_point.y, hz = right_hand_point.z;
          k->r->rightHand[0] = (k->x_shift - hx) / 200.0f - k->r->origin[0];
          k->r->rightHand[1] = (k->y_shift - hy) / 200.0f - k->r->origin[1];
          k->r->rightHand[2] = (k->z_shift - hz) / 200.0f - k->r->origin[2];

          fprintf(stderr, "%f %f %f\n", k->r->pos[0], k->r->pos[1], k->r->pos[2]);
          vec3 to_hand;
          vec3_sub(to_hand, k->r->rightHand, k->r->pos);

          riftwin_t *win = k->wm->windows;
          riftwin_t *max = NULL;
          riftwin_t *foc = NULL;
          float max_cos = -2.0f;

          int handsUp = signal(left_hand_point, left_elbow_point);
          if (k->focused) {
            k->focused->moving = 0;
            if (handsUp) {
              k->focused->pos[0] = -k->r->rightHand[0];
              k->focused->pos[1] = -k->r->rightHand[1];
              k->focused->pos[2] = -k->r->rightHand[2];
              k->focused->moving = 1;
            }
          }

          while (win)
          {
            if (win->mapped)
            {
              if(win->focused) {
                foc = win;
              }

              float temp[3];
              temp[0] = -win->pos[0];
              temp[1] = -win->pos[1];
              temp[2] = -win->pos[2];

              vec3 to_win, to_hand;
              vec3_sub(to_hand, k->r->rightHand, k->r->pos);
              vec3_sub(to_win, temp, k->r->pos);
              if (win != k->focused) {
                ratio = vec3_len(to_win) / vec3_len(to_hand);
              }
              vec3_norm(to_win, to_win);
              vec3_norm(to_hand, to_hand);

              float cos_th = vec3_mul_inner(to_win, to_hand);

              if (cos_th > 0.96 && cos_th > max_cos)
              {
                max_cos = cos_th;
                max = win;
              }
            }
            win = win->next;
          }

          if (max != NULL)
          {
            if (foc != max)
            {
              printf("focused window\n");
              max->focused = 1;
              k->focused = max;
              //focus_window(k->wm, max);
            }
          } else if (handsUp == 0) {
            // No window is focused, unfocus old window
            if (foc) foc->focused = 0;
            k->focused = NULL;
          }

        }
      }

      if (skeleton.getState() == nite::SKELETON_NONE) {
        k->tracker->startSkeletonTracking(users[i].getId());
        fprintf(stderr, "User found! : none\n");
      }
    }
  }
}

void
kinect_destroy(kinect_t *k)
{
  delete k->tracker;
  delete k;
}
