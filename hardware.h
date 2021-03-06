/*
 * hardware.h
 *
 *  Created on: Nov 17, 2016
 *      Author: is3849
 */

#ifndef HARDWARE_H_
#define HARDWARE_H_

#include <stdio.h>
#include <unistd.h>			/* for sleep() */
#include <stdint.h>			/* for uintptr_t */
#include <hw/inout.h>		/* for in*() and out*() functions */
#include <sys/neutrino.h>	/* for ThreadCtl() */
#include <sys/mman.h>		/* for mmap_device_io() */

#define PORT_LENGTH (1)
#define PA_DATA_ADDRESS (0x288)
#define PB_DATA_ADDRESS (0x289)

// a '1' for the 5th bit signifies input direction, 0 is output
#define PA_DIR_BIT (0x10)//10 0000
#define PB_DIR_BIT (0x2)
#define PB0_BIT (0x1)
#define CTRL_ADDRESS (0x28B)

#define LOW (0x00)
#define HIGH (0xFF)

#define SERVO_PERIOD_NS (20000000)

#define MAX_COUNT (60)

 uintptr_t pa_data_handle;
 uintptr_t pb_data_handle;
 uintptr_t status_handle;
 uintptr_t ctrl_handle;

int config_io(void);
void test_output_pwm(void);
void test_input_pwm(void);


#endif /* HARDWARE_H_ */
