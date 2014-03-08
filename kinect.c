// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "riftwm.h"
#include "kinect.h"

kinect_t *
kinect_init(riftwm_t *wm)
{
  kinect_t *k;
  assert((k = (kinect_t*)malloc(sizeof(kinect_t))));
  k->wm = wm;
  return k;
}

void
kinect_update(kinect_t *k)
{

}

void
kinect_destroy(kinect_t *l)
{

}
