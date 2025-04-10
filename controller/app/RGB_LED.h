#ifndef RGB_PWM_H
#define RGB_PWM_H

#include "msp430fr2355.h"
#include <msp430.h>
#include <stdint.h>

#define PWM_MAX 255

// LED pin definitions (new mapping)
// Red: P2.0, Green: P2.2, Blue: P4.0
#define RED_PIN   BIT0  // Port2.0
#define GREEN_PIN BIT2  // Port2.2
#define BLUE_PIN  BIT0  // Port4.0

// Function prototypes for RGB LED control via PWM.
void initPWM(void);
void setColorLocked(void);
void setColorUnlocking(void);
void setColorUnlocked(void);
void setCustomColor(uint8_t red, uint8_t green, uint8_t blue);

#endif // RGB_PWM_H
