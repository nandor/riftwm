// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#include "riftwm.h"
#include "kinect.h"


kinect_t *
kinect_init(riftwm_t * riftwm)
{
	kinect_t *kinect = (kinect*) malloc(sizeof (kinect_t)); 
	kinect->tracker  = (NiteUserTrackerHandle*) malloc(sizeof(NiteUserTrackerHandle));
	trackerHandle = kinect->tracker;

	if ( niteInitializeUserTracker(trackerHandle) != NITE_STATUS_OK) {
		riftwm_error(riftwm,"Couldnt initialize tracker");
	}
	return kinect
}

void
kinect_destroy(kinect_t *)
{

}
