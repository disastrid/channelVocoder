/*
 * assignment2_drums
 *
 * Second assignment for ECS732 RTDSP, to create a sequencer-based
 * drum machine which plays sampled drum sounds in loops.
 *
 * This code runs on BeagleBone Black with the BeagleRT environment.
 *
 * 2015
 * Andrew McPherson and Victor Zappi
 * Queen Mary University of London
 */


/* Needed to made usleep declaration visible with C99 */
#define _BSD_SOURCE

#include "../include/render.h"
#include "../include/Utilities.h"
#include <cstdio>
#include <rtdk.h>
#include <pthread.h>
#include <unistd.h>
#include "SimpleGPIO.h"


int gLastButtonState = HIGH;
int gOneOctave = 1;


int gButtonPin = 69; // P8 pin 09
int gLedPin01 = 68; // P8 pin 10
int gLedPin02 = 45; // P8 pin 11
int gLedPin03 = 44; // P8 pin 12
int gLedPin04 = 23; // P8 pin 13
int gLedPin05 = 26; // P8 pin 14
int gLedPin06 = 47; // P8 pin 15
int gLedPin07 = 46; // P8 pin 16

extern float gAudioOut; // main render file checks this

/* Main function for thread which reads GPIO data.
 * This function will run on its own
 * thread and will run as long as the audio is running.
 * It should be used to gather data from buttons and
 * send values to the LEDs.
 */
void *gpioLoop(void *data) {
	unsigned int latestValue;

	/* Export and set direction of all pins. 8 in total: button + 7 LEDs. */

	gpio_export(gButtonPin);
	gpio_export(gLedPin01);
	gpio_export(gLedPin02);
	gpio_export(gLedPin03);
	gpio_export(gLedPin04);
	gpio_export(gLedPin05);
	gpio_export(gLedPin06);
	gpio_export(gLedPin07);

	// Set direction of all 8 pins.
	gpio_set_dir(gButtonPin, INPUT_PIN);
	gpio_set_dir(gLedPin01, OUTPUT_PIN);
	gpio_set_dir(gLedPin02, OUTPUT_PIN);
	gpio_set_dir(gLedPin03, OUTPUT_PIN);
	gpio_set_dir(gLedPin04, OUTPUT_PIN);
	gpio_set_dir(gLedPin05, OUTPUT_PIN);
	gpio_set_dir(gLedPin06, OUTPUT_PIN);
	gpio_set_dir(gLedPin07, OUTPUT_PIN);


	// Read the button.
	gpio_get_value(gButtonPin, &latestValue);
	usleep(1000);	/* Wait 10ms to avoid checking too quickly */

	// WHEN BUTTON PRESSED, latestValue IS 0. WHEN UNPRESSED, IT'S 1.
	// Read the button pin:
	if (latestValue == LOW && gLastButtonState == HIGH) {
		// When the button is pressed, change gOneOctave to whatever it currently *isn't* -
		// in other words if it's set to one octave, change it to one third and vice versa.
		/*if (gOneOctave == 1)
			gOneOctave = 0;
		else if (gOneOctave == 0)
			gOneOctave = 1;
		printf("oneOctave is now: %d\n", gOneOctave);*/
		printf("PRESS BUTAN\n");
	}
	gLastButtonState = latestValue;

	// Set LED lights.
	// If the value is between 0-0.25, light up the middle LED, 04, and leave others off.
	/*
	if (gAudioOut > 0 && gAudioOut <= 0.25) {
		gpio_set_value(gLedPin01, LOW);
		gpio_set_value(gLedPin02, LOW);
		gpio_set_value(gLedPin03, LOW);
		gpio_set_value(gLedPin04, HIGH);
		gpio_set_value(gLedPin05, LOW);
		gpio_set_value(gLedPin06, LOW);
		gpio_set_value(gLedPin07, LOW);
	} else if (gAudioOut > 0.25 && gAudioOut <= 0.5) {
		gpio_set_value(gLedPin01, LOW);
		gpio_set_value(gLedPin02, LOW);
		gpio_set_value(gLedPin03, HIGH);
		gpio_set_value(gLedPin04, HIGH);
		gpio_set_value(gLedPin05, HIGH);
		gpio_set_value(gLedPin06, LOW);
		gpio_set_value(gLedPin07, LOW);
	} else if ((gAudioOut > 0.5) && (gAudioOut <= 0.75)) {
		gpio_set_value(gLedPin01, LOW);
		gpio_set_value(gLedPin02, HIGH);
		gpio_set_value(gLedPin03, HIGH);
		gpio_set_value(gLedPin04, HIGH);
		gpio_set_value(gLedPin05, HIGH);
		gpio_set_value(gLedPin06, HIGH);
		gpio_set_value(gLedPin07, LOW);
	} else {
		gpio_set_value(gLedPin01, HIGH);
		gpio_set_value(gLedPin02, HIGH);
		gpio_set_value(gLedPin03, HIGH);
		gpio_set_value(gLedPin04, HIGH);
		gpio_set_value(gLedPin05, HIGH);
		gpio_set_value(gLedPin06, HIGH);
		gpio_set_value(gLedPin07, HIGH);
	} // end if else statements
	*/

	/* cleanup. */

	gpio_unexport(gButtonPin);
	gpio_unexport(gLedPin01);
	gpio_unexport(gLedPin02);
	gpio_unexport(gLedPin03);
	gpio_unexport(gLedPin04);
	gpio_unexport(gLedPin05);
	gpio_unexport(gLedPin06);
	gpio_unexport(gLedPin07);

	pthread_exit(NULL); /* Don't change this */
}
