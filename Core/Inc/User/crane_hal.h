/*
 * crane_hal.h
 *
 *  Created on: Nov 26, 2025
 *      Author: ryang
 */

#ifndef INC_USER_CRANE_HAL_H_
#define INC_USER_CRANE_HAL_H_

#include <stdint.h>

void Crane_HAL_Init(void);

// vertical motion
void Crane_MoveVerticalUp(void);
void Crane_MoveVerticalDown(void);
void Crane_StopVertical(void);

// platform motion
void Crane_MovePlatformLeft(void);
void Crane_MovePlatformRight(void);
void Crane_StopPlatform(void);

#endif /* INC_USER_CRANE_HAL_H_ */
