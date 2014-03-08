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
            const nite::SkeletonJoint & head = skeleton.getJoint(nite::JOINT_HEAD);
            const nite::Point3f & head_point = head.getPosition();

            //k->wm->head_pos[0] = head_point.x/1000.0f - k->x_shift;
            //k->wm->head_pos[1] = head_point.y/1000.0f - k->y_shift;
            //k->wm->head_pos[2] = head_point.z/1000.0f - k->z_shift;
            printf("%5.2f, %5.2f, %5.2f \n", head_point.x/1000.0f - k->x_shift,
                                              head_point.y/1000.0f - k->y_shift,
                                              head_point.z/1000.0f - k->z_shift);
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
