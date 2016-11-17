#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <hw/inout.h>
#include <time.h>
#include <unistd.h>			/* for Sleep() */
#include <math.h>
#include <sys/neutrino.h>	/* for ThreadCtl() */
#include <sys/mman.h>		/* for mmap_device_io() */
#include <stdint.h>
#include "hardware.c"

typedef struct{
	struct timespec curr_duty_cycle;
	uintptr_t data_handle;
} Servo;

// function prototypes
void* generateOutput(void* args);
Servo makeServo(uintptr_t data_handle);

void* generateOutput(void* args) {
	Servo *servo = (Servo *)arg;
	struct time_spec low_time_interval;

	low_time_interval.tv_sec = 0;
	low_time_interval.tv_nsec = SERVO_PERIOD_NS - servo->curr_duty_cycle;

	while(1) {
		out8(servo->data_handle, HIGH);
		nanosleep(&servo->curr_duty_cycle, NULL);
		out8(servo->data_handle, LOW);
		nanosleep(&low_time_interval, NULL);
	}

}

int main(int argc, char *argv[]) {
	printf("Welcome to the QNX Momentics IDE\n");
	Servo servo1, servo2;
	pthread_t servo1_thr, servo2_thr;

	servo1 = makeServo(pa_data_handle);
	servo2 = makeServo(pb_data_handle);
	pthread_create(servo1_thr, NULL, &generateOutput, (void *)&servo1);

	return EXIT_SUCCESS;
}

Servo makeServo(uintptr_t data_handle)
{
	struct Servo servo;
	servo.data_handle = data_handle;
	return servo;
}
