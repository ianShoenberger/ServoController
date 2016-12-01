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
#include <string.h>
#include <ctype.h>
#include "hardware.h"

// OpCode Constants
#define MOV (32)
#define WAIT (64)
#define LOOP_START (128)
#define END_LOOP (160)
#define RECIPE_END (0)

typedef struct{
	int position;
	int id;
	int recipe_cursor;
	int loop_ctr;
	int state;
	int paused;
	struct timespec duty_cycle;
	struct timespec low_interval;
	uintptr_t data_handle;
	pthread_mutex_t wave_mutex;
	pthread_mutex_t cursor_mutex;
} Servo;

// function prototypes
void* generate_output(void* args);
void* read_recipe(void* args);
Servo make_servo(uintptr_t data_handle, int id);
void set_pwm(Servo *servo, int pos);

void* generate_output(void* args) {
	Servo *servo = (Servo *)args;

	while(1) {
		pthread_mutex_lock(&servo->wave_mutex);
		out8(servo->data_handle, HIGH);
		//printf("HIGH\n");
		nanosleep(&servo->duty_cycle, NULL);
		out8(servo->data_handle, LOW);
		//printf("LOW\n");
		//printf("servo pos: %d\n", servo->position);
		nanosleep(&servo->low_interval, NULL);
		pthread_mutex_unlock(&servo->wave_mutex);
	}

	return NULL;
}

void* read_recipe(void* args) {
	char recipe[] = {MOV+0,MOV+5,MOV+0,MOV+3,LOOP_START+0,MOV+1,MOV+4,END_LOOP,MOV+0,MOV+2,WAIT+0,MOV+3,WAIT+0,MOV+2,MOV+3,WAIT+31,WAIT+31,WAIT+31,MOV+4};
	Servo *servo = (Servo *)args;
	int opcode = 0;
	int opcode_argument = 0;
	int op_mask = 0xE0;
	int op_arg_mask = 0x1F;
	int tmp_cursor = 0;

	struct timespec servo_wait_period;
	servo_wait_period.tv_sec = 0;
	servo_wait_period.tv_nsec = 0;

	while(1)
	{
		// better to lock the cursor so that we dont end up with a command with the wrong paired value!
		pthread_mutex_lock(&servo->cursor_mutex);
		tmp_cursor = servo->recipe_cursor;
		pthread_mutex_unlock(&servo->cursor_mutex);

		// mask the element to get the opcode - the top 3 bits
		opcode = recipe[tmp_cursor] & op_mask;
		opcode_argument = recipe[tmp_cursor] & op_arg_mask;

		switch(opcode)
		{
		case MOV:
			printf("Mov: %d\n", opcode_argument);
			set_pwm(servo, opcode_argument);
			// add wait time here
			break;
		case WAIT:
			printf("Wait: %d\n", opcode_argument);
			// increase by 1 - requirements say the value will always be 1/10 more than what is specified
			opcode_argument++;
			servo_wait_period.tv_sec = opcode_argument / 10;
			servo_wait_period.tv_nsec = (opcode_argument % 10) * 100000000;
			nanosleep(&servo_wait_period, NULL);
			break;
		case LOOP_START:
			printf("Loop_Start: %d\n", opcode_argument);
			// set loop counter only if this is first time through
			if(servo->loop_ctr == 0)
			{
				servo->loop_ctr = opcode_argument;
			}
			break;
		case END_LOOP:
			printf("End_Loop\n");
			if(servo->loop_ctr > 0)
			{
				servo->loop_ctr--;
				// rewind loop till we get to first LOOP_START
				while(opcode != LOOP_START)
				{
					tmp_cursor--;
					opcode = recipe[tmp_cursor];
				}
				pthread_mutex_lock(&servo->cursor_mutex);
				servo->recipe_cursor = tmp_cursor;
				pthread_mutex_unlock(&servo->cursor_mutex);

			}
			break;
		case RECIPE_END:
			break;
		}

		if(opcode != RECIPE_END)
		{
			pthread_mutex_lock(&servo->cursor_mutex);
			servo->recipe_cursor += 1;
			pthread_mutex_unlock(&servo->cursor_mutex);
		}
	}

	return NULL;
}

int main(int argc, char *argv[]) {
	struct _clockperiod new_resolution;
	static Servo servo1, servo2;
	Servo *servoArray[2];
	new_resolution.fract = 0;
	new_resolution.nsec = 10000;
	//int rc = 0;
	char buff[10];
	char temp_override_cmd = '\0';
	size_t input_len;
	int cmd_iterator = 0;
	int new_servo_pos = 0;
	printf("Welcome to the QNX Momentics IDE\n");

	// manipulate the resolution of the clock
	ClockPeriod_r(CLOCK_REALTIME, &new_resolution, NULL, 0);
	config_io();

	servo1 = make_servo(pa_data_handle, 0);
	servo2 = make_servo(pb_data_handle, 1);
	//test_output_pwm();

	pthread_create(NULL, NULL, generate_output, (void *)&servo1);
	pthread_create(NULL, NULL, generate_output, (void *)&servo2);
	pthread_create(NULL, NULL, read_recipe, (void *)&servo1);
	pthread_create(NULL, NULL, read_recipe, (void *)&servo2);


	servoArray[0] = &servo1;
	servoArray[1] = &servo2;

	while(1)
	{
		memset(buff, 0, sizeof(buff));
		fgets(buff, sizeof(buff), stdin);
		input_len = strlen(buff);
		// we check for length greater 3 because <CR> counts as a char!
		if(input_len >= 3)
		{
			for(cmd_iterator = 0; cmd_iterator < 2; cmd_iterator++) {
				temp_override_cmd = buff[cmd_iterator];
				temp_override_cmd = tolower(temp_override_cmd);

				switch(temp_override_cmd)
				{
				case 'p':
					printf("user paused recipe\n");
					// paused <=1;

					break;
				case 'c':
					printf("user continued recipe\n");
					// paused <= 0;
					break;
				case 'r':
					if(servoArray[cmd_iterator]->position > 0)
					{
						new_servo_pos = servoArray[cmd_iterator]->position - 1;
						set_pwm(servoArray[cmd_iterator], new_servo_pos);
					}
					break;
				case 'l':
					if(servoArray[cmd_iterator]->position < 5)
					{
						new_servo_pos = servoArray[cmd_iterator]->position + 1;
						set_pwm(servoArray[cmd_iterator], new_servo_pos);
					}
					break;
				case 'n':
					printf("user chose no-op\n");
					//do nothing
					break;
				case 'b':
					printf("user restarted recipe\n");
					// recipe pointer goes back to 0
					break;
				}
			}
		}
	}

	printf("bye\n");
	return EXIT_SUCCESS;
}

Servo make_servo(uintptr_t data_handle, int id)
{
	Servo servo;
	struct timespec added_wait_time;
	added_wait_time.tv_sec = 1;
	added_wait_time.tv_nsec = 0;

	servo.id = id;
	servo.position = 0;
	servo.recipe_cursor = 0;
	servo.loop_ctr = 0;
	servo.data_handle = data_handle;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
	servo.wave_mutex = mutex;
	servo.cursor_mutex = mutex2;
	set_pwm(&servo, 0);
	servo.paused = 0;
	nanosleep(&added_wait_time, NULL);
	return servo;
}

void set_pwm(Servo *servo, int pos) {
	int offset = 388000;
	int coeff = 400000;
	int period = 20000000;
	struct timespec added_wait_time;
	added_wait_time.tv_sec = 0;
	added_wait_time.tv_nsec = 0;
	int num_positions_moved = 0;
	int tmp = 0;
	int wait_time = 0;


	num_positions_moved = abs(servo->position - pos);
	servo->position = pos;
	pthread_mutex_lock(&servo->wave_mutex);
	servo->duty_cycle.tv_nsec = (pos * coeff) + offset;
	servo->duty_cycle.tv_sec = 0;
	//printf("pos is: %d, duty(ns) is: %d\n", pos, (pos * coeff) + offset);

	servo->low_interval.tv_nsec = period - servo->duty_cycle.tv_nsec;
	servo->low_interval.tv_sec = 0;
	pthread_mutex_unlock(&servo->wave_mutex);

	// requirements specify to add a wait of 200ms per move in order to allow servos the time to travel
	wait_time = 200 * num_positions_moved;
	added_wait_time.tv_sec = wait_time / 1000;
	tmp = wait_time % 1000;
	added_wait_time.tv_nsec = (wait_time % 1000) * 1000000;
	nanosleep(&added_wait_time, NULL);
}
