#include "system.h"

#ifdef USE_MOTION_SENSOR

#if VITA

#include <psp2/motion.h>

//-lSceMotion_stub is required.

static SceMotionSensorState state;
static bool sensor_state = false;

void systemUpdateMotionSensor (void) {
	sceMotionGetSensorState(&state, 1);
}

int systemGetSensorX (void) {
	return state.accelerometer.x * -0x30000000;
}

int systemGetSensorY (void) {
	return state.accelerometer.y * 0x30000000;
}

void systemSetSensorState(bool val) {
	if(val == sensor_state) return;
	
	if(val) {
		sceMotionStartSampling();
	} else {
		sceMotionStopSampling();
	}
	sensor_state = val;
}
#endif

#endif
