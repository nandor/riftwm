// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#ifndef __KINECT_H__
#define __KINECT_H__

typedef struct kinect_t
{
  riftwm_t      *wm;
} kinect_t;

kinect_t *kinect_init(riftwm_t *);
void kinect_destroy(kinect_t *);

#endif /*__KINECT_H__*/