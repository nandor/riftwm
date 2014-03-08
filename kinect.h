// This file is part of the RiftWM project
// Licensing information can be found in the LICENSE file
// (C) 2013 The RiftWM Project. All rights reserved.
#ifndef __KINECT_H__
#define __KINECT_H__

#include "NiteCAPI.h"
#include "NiteCEnums.h"
#include "NiteCTypes.h"

typedef struct kinect_t kinect_t;

#ifdef __cplusplus
extern "C"
{
#endif 
	kinect_t *kinect_init(riftwm_t *);
	void kinect_update(kinect_t *);
	void kinect_destroy(kinect_t *);
#ifdef __cplusplus
}
#endif 

#endif /*__KINECT_H__*/