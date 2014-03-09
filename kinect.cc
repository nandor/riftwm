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

typedef struct kinect_t
{
  riftwm_t      *wm;
  nite::UserTracker* tracker;
  nite::UserId first_id;
  bool found;

  float x_shift,y_shift,z_shift;

} kinect_t;

kinect_t *
kinect_init(riftwm_t * wm)
{
  kinect_t *k;
  k = new kinect_t;
  k->wm = wm;

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

  k->found = false;
  return k;
}

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
          

          // updating the position of the head
          const nite::SkeletonJoint & head = skeleton.getJoint(nite::JOINT_HEAD);
          const nite::Point3f & head_point = head.getPosition();

          k->wm->renderer->pos[0] = -(head_point.x / 1000.0f ) * 10.0f;
          k->wm->renderer->pos[1] = 5.0f -(head_point.y / 1000.0f ) * 5.0f;
          k->wm->renderer->pos[2] = -(head_point.z / 1000.0f ) * 10.0f;
          fprintf(stderr, "%f %f %f\n", k->wm->renderer->pos[0],
                                        k->wm->renderer->pos[1],
                                        k->wm->renderer->pos[2]);
          // updating the posintion of hands

          const nite::SkeletonJoint & left_hand = skeleton.getJoint(nite::JOINT_LEFT_HAND);
          const nite::Point3f & left_hand_point = left_hand.getPosition();

          k->wm->renderer->leftHand[0] = -(left_hand_point.x / 1000.0f ) * 10.0f;
          k->wm->renderer->leftHand[1] = 5.0f -(left_hand_point.y / 1000.0f ) * 5.0f;
          k->wm->renderer->leftHand[2] = -(left_hand_point.z / 1000.0f ) * 10.0f;
          fprintf(stderr, "%f %f %f\n", k->wm->renderer->leftHand[0],
                                        k->wm->renderer->leftHand[1],
                                        k->wm->renderer->leftHand[2]);

          const nite::SkeletonJoint & right_hand = skeleton.getJoint(nite::JOINT_RIGHT_HAND);
          const nite::Point3f & right_hand_point = right_hand.getPosition();

          k->wm->renderer->rightHand[0] = -(right_hand_point.x / 1000.0f ) * 10.0f;
          k->wm->renderer->rightHand[1] = 5.0f -(right_hand_point.y / 1000.0f ) * 5.0f;
          k->wm->renderer->rightHand[2] = -(right_hand_point.z / 1000.0f ) * 10.0f;
          fprintf(stderr, "%f %f %f\n", k->wm->renderer->rightHand[0],
                                        k->wm->renderer->rightHand[1],
                                        k->wm->renderer->rightHand[2]);

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
