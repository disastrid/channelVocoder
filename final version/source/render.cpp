/*
 * Final project - Astrid Bin, RTDSP 2015
 * A real-time channel vocoder.
 *
 *
 * This code runs on BeagleBone Black with the BeagleRT environment.
 *
 * 2015
 * Andrew McPherson and Victor Zappi
 * Queen Mary University of London
 */


#include "../include/render.h"
#include "../include/Utilities.h"
#include <rtdk.h>
#include <cmath>

/* TODO:
 * Implement band pass filters, logarithmically
 * Generate carrier signal
 * Take in audio
 * Implement envelope follower
 * Output result.
 *
 */

/* Global variables */

#define SAMPLE_RATE 44100.0
#define SQRT_2 1.41421356237
#define PI 3.14159265359


// Bring this in from gpio.cpp. Starts true, and switches to whatever it
// isn't when the external button is pressed. This means that it switches
// between one and one-third octave resolution.
extern bool gOneOctave;


// One set of variables for each carrier frequency:
float gCarrierFreq_01; // can't make this any lower than 35, because then it's a fart machine.
int gNumberOfSamplesPassed_01 = 0;
float gSawCarrierOut_01 = 0;
float gNumSamplesPerTooth_01;
int gCarrierFrequency_01;
float gPhaseAtEndOfPrevFrame_01 = 0;
float gIntervalOfStepsToTopOfTooth_01;

float gCarrierFreq_02; // can't make this any lower than 35, because then it's a fart machine.
int gNumberOfSamplesPassed_02 = 0;
float gSawCarrierOut_02 = 0;
float gNumSamplesPerTooth_02;
int gCarrierFrequency_02;
float gPhaseAtEndOfPrevFrame_02 = 0;
float gIntervalOfStepsToTopOfTooth_02;

float gSawCarrierOut_average;


int gNumOfBands_oneOctave = 10;
int gNumOfBands_oneThirdOctave = 28;

// Variables for coefficients! One set for oneOctave and one set for oneThirdOctave.
/* VARIABLES FOR ONE OCTAVE BANDS */
float gGeoMeans_oneOctave[10];
float gCutoffFreqs_oneOctave[11] = {22, 44, 88, 177, 355,
                          710, 1420, 2840, 5680,
                          11360, 22720};
float gCoeffs_oneOctave[10][5]; // 10 bands, 5 coeffs - a1, a2, b0, b1, b2. index 0 is lowest freq band.
float gCarrierY_oneOctave[10][2];
float gModulatorY_oneOctave[10][2];
float gCarrierX_oneOctave[2];
float gModulatorX_oneOctave[2];
float gPeaks_oneOctave[10];
float gBandOutputsToBeSummed_oneOctave[10];
float gSummedBands_oneOctave;
int gWritePointer_oneOctave = 0;

/* VARIABLES FOR ONE THIRD OCTAVE BANDS */
float gGeoMeans_oneThirdOctave[28];
float gCutoffFreqs_oneThirdOctave[29] = {22.4, 28.2, 35.5,
                          47.7, 56.2, 70.8,
                          89.1, 112, 141,
                          178, 224, 282,
                          355, 447, 562,
                          708, 891, 1122,
                          1413, 1778, 2239,
                          2818, 3548, 4467,
                          5623, 7079, 8913,
                          11220, 14130};
float gCoeffs_oneThirdOctave[28][5]; // 10 bands, 5 coeffs - a1, a2, b0, b1, b2. index 0 is lowest freq band.
float gCarrierY_oneThirdOctave[28][2];
float gModulatorY_oneThirdOctave[28][2];
float gCarrierX_oneThirdOctave[2];
float gModulatorX_oneThirdOctave[2];
float gPeaks_oneThirdOctave[28];
float gBandOutputsToBeSummed_oneThirdOctave[28];
float gSummedBands_oneThirdOctave;
int gWritePointer_oneThirdOctave = 0;




// Variables for envelope follower:
int gAttackInMs;
int gReleaseInMs;


// Variables for the LED:
float gAudioOut; // this needs to be global so lights can turn on/off in response

bool initialise_render(int numMatrixChannels, int numAudioChannels,
                       int numMatrixFramesPerPeriod,
                       int numAudioFramesPerPeriod,
                       float matrixSampleRate, float audioSampleRate,
                       void *userData)
{
    // ** START INITIALISING BUFFERS ** //
    // Initialise the delay buffers to 1 so there's no 0 coefficients.
    for (int i = 0; i < 2; i++) {
        gModulatorX_oneOctave[i] = 1.0;
        gCarrierX_oneOctave[i] = 1.0;
    }
    // Initialise the one octave 2D sample buffers to 1.
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 2; j++) {
            gCarrierY_oneOctave[i][j] = 1.0;
            gModulatorY_oneOctave[i][j] = 1.0;
        }
    }
    // ** END INITIALISING BUFFERS **//

    // ** START CALCULATING COEFFICIENTS ** //
    // CALCULATE ONE OCTAVE BANDS //

    // FIND GEOMETRIC MEANS
    for (int i = 0; i < 10; i++) { // < 10 because of the i+1 .. if it's <11 we'll go out of range
        float t = 1/SAMPLE_RATE;
        float mean, result;
        // PREWARP:
        mean = 2/t * tan(gCutoffFreqs_oneOctave[i]*2*PI*t/2) * 2/t * tan(gCutoffFreqs_oneOctave[i+1]*2*PI*t/2);
        result = sqrt(mean);
        gGeoMeans_oneOctave[i] = result;
    }

    // DECALRE COMMON VARIABLES
    float t = 1/SAMPLE_RATE;
    float t2 = t*t;
    float w0;

    for (int i = 0; i < 10; i++) {
        // IF THE FIRST BAND, IT'S A LOW PASS.
        if (i == 0) { // calculate lowpass filter at the bottom
            // PREWARP:
            w0 = 2/t * tan(gCutoffFreqs_oneOctave[i]*2*PI*t/2);
            float w02 = w0*w0;
            float q = SQRT_2/2;
            float norm = 4+((w0*2*t)/q) + w0*t2;

            gCoeffs_oneOctave[i][0] = (-8 + 2*w02*t2)/norm;
            gCoeffs_oneOctave[i][1] = (4-((w0*2*t)/q)+w0*t2)/norm;
            gCoeffs_oneOctave[i][2] = (w02*t2) / norm;
            gCoeffs_oneOctave[i][3] = (2*w02*t2) / norm;
            gCoeffs_oneOctave[i][4] = gCoeffs_oneOctave[i][2];

        }
        // IF IT'S THE LAST BAND, IT'S A HIGH PASS.
        else if (i == 9) { // calculate highpass filter at the top
            // PREWARP:
            w0 = 2/t * tan(gCutoffFreqs_oneOctave[i]*PI*2*t/2) ;
            float w02 = w0*w0;
            float q = SQRT_2/2;
            float norm = 4 + ((w0*2*t)/q) + w02*t2;

            gCoeffs_oneOctave[i][0] = (-8 + 2*w02*t2)/norm;
            gCoeffs_oneOctave[i][1] = (4-((w0*2*t)/q)+w02*t2)/norm;
            gCoeffs_oneOctave[i][2] = 4 / norm;
            gCoeffs_oneOctave[i][3] = -8 / norm;
            gCoeffs_oneOctave[i][4] = gCoeffs_oneOctave[i][2];

        }
        // OTHERWISE, IT'S A BAND PASS.
        else { // if not the first or last band, calculate as a bandpass.
            float bpfQ = SQRT_2; // Q for BPF
            w0 = gGeoMeans_oneOctave[i];
            float w02 = w0 * w0;
            float bpfNorm = 4 + (w0*2*t/bpfQ) + w02*t2;

            gCoeffs_oneOctave[i][0] = (-8 + w02*t2*2)/bpfNorm;
            gCoeffs_oneOctave[i][1] = (4-(w0*t*2/bpfQ) + w02*t2)/bpfNorm;
            gCoeffs_oneOctave[i][2] = (w0*2*t/bpfQ)/bpfNorm;
            gCoeffs_oneOctave[i][3] = 0;
            gCoeffs_oneOctave[i][4] = (-w0*2*t/bpfQ)/bpfNorm;
        } // end if/else statements
    }// end for loop for one octave calculations
    // ** END CALCULATING ONE OCTAVE BANDS **//
    /* PRINT RESULTS FOR MATLAB */

    rt_printf("MATLAB RESULTS: One octave\n");
    for (int i = 0; i < 10; i++) {
        rt_printf("%f, %f, %f, %f, %f\n", gCoeffs_oneOctave[i][0], gCoeffs_oneOctave[i][1], gCoeffs_oneOctave[i][2], gCoeffs_oneOctave[i][3], gCoeffs_oneOctave[i][4]);
    }

    /* Start calculating one third octave coefficients */
    for (int i = 0; i < 28; i++) {
            float mean, result;
            // PREWARP:
            mean = 2/t * tan(gCutoffFreqs_oneThirdOctave[i]*2*PI*t/2) * 2/t * tan(gCutoffFreqs_oneThirdOctave[i+1]*2*PI*t/2);
            result = sqrt(mean);
            gGeoMeans_oneThirdOctave[i] = result;
        }


        for (int i = 0; i < 28; i++) {
            if (i == 0) {
                w0 = 2/t * tan(gCutoffFreqs_oneThirdOctave[i]*2*PI*t/2);
                float w02 = w0*w0;
                float q = SQRT_2/2;
                float norm = 4+((w0*2*t)/q) + w0*t2;
                gCoeffs_oneThirdOctave[i][0] = (-8 + 2*w02*t2)/norm;
                gCoeffs_oneThirdOctave[i][1] = (4-((w0*2*t)/q)+w0*t2)/norm;
                gCoeffs_oneThirdOctave[i][2] = (w02*t2) / norm;
                gCoeffs_oneThirdOctave[i][3] = (2*w02*t2) / norm;
                gCoeffs_oneThirdOctave[i][4] = gCoeffs_oneThirdOctave[i][2];
            }
            else if (i == 27) {
                w0 = 2/t * tan(gCutoffFreqs_oneThirdOctave[i]*PI*2*t/2);
                float w02 = w0*w0;
                float q = SQRT_2/2;
                float norm = 4 + ((w0*2*t)/q) + w02*t2;
                gCoeffs_oneThirdOctave[i][0] = (-8 + 2*w02*t2)/norm;
                gCoeffs_oneThirdOctave[i][1] = (4-((w0*2*t)/q)+w02*t2)/norm;
                gCoeffs_oneThirdOctave[i][2] = 4 / norm;
                gCoeffs_oneThirdOctave[i][3] = -8 / norm;
                gCoeffs_oneThirdOctave[i][4] = gCoeffs_oneThirdOctave[i][2];
            } else {
				float bpfQ = 4.32; // Q for BPF at one third octave
				w0 = gGeoMeans_oneThirdOctave[i];
				float w02 = w0 * w0;
				float bpfNorm = 4 + (w0*2*t/bpfQ) + w02*t2;

				gCoeffs_oneThirdOctave[i][0] = (-8 + w02*t2*2)/bpfNorm;
				gCoeffs_oneThirdOctave[i][1] = (4-(w0*t*2/bpfQ) + w02*t2)/bpfNorm;
				gCoeffs_oneThirdOctave[i][2] = (w0*2*t/bpfQ)/bpfNorm;
				gCoeffs_oneThirdOctave[i][3] = 0;
				gCoeffs_oneThirdOctave[i][4] = (-w0*2*t/bpfQ)/bpfNorm;
            } // end if/else statements

        /* PRINT RESULTS FOR MATLAB */
        rt_printf("MATLAB RESULTS: One third octave\n");
        for (int i = 0; i < 28; i++) {
        	rt_printf("%f, %f, %f, %f, %f\n", gCoeffs_oneThirdOctave[i][0], gCoeffs_oneThirdOctave[i][1], gCoeffs_oneThirdOctave[i][2], gCoeffs_oneThirdOctave[i][3], gCoeffs_oneThirdOctave[i][4]);
        }
} // end for loop for one third octave

    return true;
}

void render(int numMatrixFrames, int numAudioFrames, float *audioIn, float *audioOut,
            float *matrixIn, float *matrixOut)
{
    // RENDERING EACH SAMPLE.
     for (int n = 0; n < numAudioFrames; n++) {
        // CARRIER FREQUENCIES.
        // 3.1 Calculate values for carrier frequency.
        float potentiometerReading_01 = matrixIn[(n/2)*8+0]; // carrier 1 on analog pin 0.
        float reading_01 = map(potentiometerReading_01, 0.0, 0.82, 100, 1200);

        // Cast that mapped float value to an int:
        gCarrierFrequency_01 = reading_01;
        gNumSamplesPerTooth_01 = SAMPLE_RATE / gCarrierFrequency_01; // calculate that for the sawtooth wave
        gIntervalOfStepsToTopOfTooth_01 = 2.0/gNumSamplesPerTooth_01; // range of 2: -1 to 1. ie for 50hz this would be ~0.0022675

        gSawCarrierOut_01 += gIntervalOfStepsToTopOfTooth_01;
        if (gSawCarrierOut_01 >= 1.0) {
            gSawCarrierOut_01 -= 2.0;
        }
        // GET ATTACK/RELEASE IN MS FROM ANALOG PINS 4 AND 5 - hard coded for testing, didn't get it working

        float potentiometerReading_attack = matrixIn[(n/2)*8+3]; // carrier 1 on analog pin 0.
        float attack_reading = map(potentiometerReading_01, 0.0, 0.82, 50, 1000);
        // Cast that mapped float value to an int:
        gAttackInMs = 1200;

        float potentiometerReading_release = matrixIn[(n/2)*8+4]; // carrier 1 on analog pin 0.
        float release_reading = map(potentiometerReading_01, 0.0, 0.82, 50, 1000);
        // Cast that mapped float value to an int:
        gReleaseInMs = 5000;



        // GET A MODULATION (VOICE) SAMPLE.
        float monoVoiceSample = audioIn[n*2] + audioIn[n*2+1]; // audio comes in on the red wire

        monoVoiceSample *= 0.02; // scale because of megaGain

        if (gOneOctave) {
        // calculate output for one octave resolution
        	float carrierSamplesOut_oneOctave[10];
        	float modulatorSamplesOut_oneOctave[10];

        // ENVELOPE FOLLOWER:
        // (This needs a level detector)
        // inspired by this: http://www.musicdsp.org/archive.php?classid=2#136
        // This should probably be declared outside the loop but i'm leaving it here for now:

        // Calculate the attack/release coefficients only every 100 samples

        float coefficientForAttack = exp(log(0.01)/(gAttackInMs * SAMPLE_RATE * 0.001));
        float coefficientForRelease = exp(log(0.01)/(gReleaseInMs * SAMPLE_RATE * 0.001));

        //}
        for (int i = 0; i < 10; i++) { // for each band ...

                // do the filtering on both the carrier and modulator signals.
                // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
                // Two 2D buffers for the output - one for carrier signal, one for modulator signal:
                carrierSamplesOut_oneOctave[i] = (gCoeffs_oneOctave[i][2] * gSawCarrierOut_01) +
                								 (gCoeffs_oneOctave[i][3] * gCarrierX_oneOctave[(gWritePointer_oneOctave + 2 - 1) % 2]) +
                								 (gCoeffs_oneOctave[i][4] * gCarrierX_oneOctave[gWritePointer_oneOctave]) -
                								 (gCoeffs_oneOctave[i][0] * gCarrierY_oneOctave[i][(gWritePointer_oneOctave + 2 - 1) % 2]) -
                								 (gCoeffs_oneOctave[i][1] * gCarrierY_oneOctave[i][gWritePointer_oneOctave]);
                modulatorSamplesOut_oneOctave[i] = (gCoeffs_oneOctave[i][2] * monoVoiceSample) +
                								   (gCoeffs_oneOctave[i][3] * gModulatorX_oneOctave[(gWritePointer_oneOctave + 2 - 1) % 2]) +
                								   (gCoeffs_oneOctave[i][4] * gModulatorX_oneOctave[gWritePointer_oneOctave]) -
                								   (gCoeffs_oneOctave[i][0] * gModulatorY_oneOctave[i][(gWritePointer_oneOctave + 2 - 1) % 2]) -
                								   (gCoeffs_oneOctave[i][1] * gModulatorY_oneOctave[i][gWritePointer_oneOctave]);

                gModulatorY_oneOctave[i][gWritePointer_oneOctave] = modulatorSamplesOut_oneOctave[i];
                gCarrierY_oneOctave[i][gWritePointer_oneOctave] = carrierSamplesOut_oneOctave[i];

                float tmp = fabs(modulatorSamplesOut_oneOctave[i]);
                float envelope = 0.0;
                if (tmp > envelope) {
                    envelope = tmp; //coefficientForAttack * (envelope - tmp) + tmp; // alternatively envelope = tmp;
                } else {
                    envelope -= coefficientForRelease;// * (envelope - tmp) + tmp;	// alternatively envelope * coefficientForRelease;
                }

                // Put each output in the delay buffer, replacing the oldest sample:


                // Get the value on each band, ready to be summed:
                gBandOutputsToBeSummed_oneOctave[i] = carrierSamplesOut_oneOctave[i] * envelope;

        } // end band loop
        gCarrierX_oneOctave[gWritePointer_oneOctave] = gSawCarrierOut_average;
        gModulatorX_oneOctave[gWritePointer_oneOctave] = monoVoiceSample;

        		//if (n%5000==0) {
        			//for (int i = 0; i < 10; i++){
        			//	float value = fabs(modulatorSamplesOut_oneOctave[i]);
        				//rt_printf("value of voice sample: %f\n", monoVoiceSample);
        		//}
        		//}
                    // Add up the signals on all 10 bands:
                float summedBands = 0.0;
                for (int i = 0; i < 10; i++) {
                    summedBands += gBandOutputsToBeSummed_oneOctave[i];
                }


                // OUTPUT
                // Put the summed values of all bands on the output.
                audioOut[n*2] = summedBands;
                audioOut[n*2+1] = summedBands;

                // The following is to give the LEDs a value to which to respond:
                gAudioOut = fabs(summedBands);

                // NOW put the current X samples in their buffers, to replace the oldest samples


                // ADVANCE THE WRITE POINTER
                gWritePointer_oneOctave++;
                if (gWritePointer_oneOctave >= 2) {
                    gWritePointer_oneOctave = 0;
                }

     } // end one octave*/

        	// calculate output for one third resolution
            // calculate output for one octave resolution
        else {
        	float carrierSamplesOut_oneThirdOctave[28];
            float modulatorSamplesOut_oneThirdOctave[28];

            // ENVELOPE FOLLOWER:

            float coefficientForAttack = exp(log(0.01)/(gAttackInMs * SAMPLE_RATE * 0.001));
            float coefficientForRelease = exp(log(0.01)/(gReleaseInMs * SAMPLE_RATE * 0.001));

    	    for (int i = 0; i < 28; i++) { // for each band ...

    	    // do the filtering on both the carrier and modulator signals.
    	    // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    	    // Two 2D buffers for the output - one for carrier signal, one for modulator signal:
    	    carrierSamplesOut_oneThirdOctave[i] = gCoeffs_oneThirdOctave[i][2] * gSawCarrierOut_01 +
    	    									  gCoeffs_oneThirdOctave[i][3] * gCarrierX_oneThirdOctave[(gWritePointer_oneThirdOctave + 2 - 1) % 2] +
    	    									  gCoeffs_oneThirdOctave[i][4] * gCarrierX_oneThirdOctave[gWritePointer_oneThirdOctave] -
    	    									  gCoeffs_oneThirdOctave[i][0] * gCarrierY_oneThirdOctave[i][(gWritePointer_oneThirdOctave + 2 - 1) % 2] -
    	    									  gCoeffs_oneThirdOctave[i][1] * gCarrierY_oneThirdOctave[i][gWritePointer_oneThirdOctave];
    	    modulatorSamplesOut_oneThirdOctave[i] = gCoeffs_oneThirdOctave[i][2] * monoVoiceSample +
    	    									  gCoeffs_oneThirdOctave[i][3] * gModulatorX_oneThirdOctave[(gWritePointer_oneThirdOctave + 2 - 1) % 2] +
    	    									  gCoeffs_oneThirdOctave[i][4] * gModulatorX_oneThirdOctave[gWritePointer_oneThirdOctave] -
    	    									  gCoeffs_oneThirdOctave[i][0] * gModulatorY_oneThirdOctave[i][(gWritePointer_oneThirdOctave + 2 - 1) % 2] -
    	    									  gCoeffs_oneThirdOctave[i][1] * gModulatorY_oneThirdOctave[i][gWritePointer_oneThirdOctave];

    	    gModulatorY_oneThirdOctave[i][gWritePointer_oneThirdOctave] = modulatorSamplesOut_oneThirdOctave[i];
    	    gCarrierY_oneThirdOctave[i][gWritePointer_oneThirdOctave] = carrierSamplesOut_oneThirdOctave[i];

            float tmp = fabs(modulatorSamplesOut_oneThirdOctave[i]);
            float envelope = 0.0;
            if (tmp > envelope) {
                envelope = tmp; //coefficientForAttack * (envelope - tmp) + tmp; // alternatively envelope = tmp;
            } else {
                envelope = coefficientForRelease * (envelope - tmp) + tmp;	// alternatively envelope * coefficientForRelease;
            }
            // Get the value on each band, ready to be summed:
            gBandOutputsToBeSummed_oneThirdOctave[i] = carrierSamplesOut_oneThirdOctave[i] * envelope;

    } // end band loop
			gCarrierX_oneOctave[gWritePointer_oneThirdOctave] = gSawCarrierOut_average;
			gModulatorX_oneOctave[gWritePointer_oneThirdOctave] = monoVoiceSample;

			// Add up the signals on all bands:
			float summedBands = 0.0;
			for (int i = 0; i < 10; i++) {
				summedBands += gBandOutputsToBeSummed_oneThirdOctave[i];
			}

            // OUTPUT
            // Put the summed values of all bands on the output.
            audioOut[n*2] = summedBands;
            audioOut[n*2+1] = summedBands;

            // The following is to give the LEDs a value to which to respond:
            gAudioOut = fabs(summedBands);

            // ADVANCE THE WRITE POINTER
            gWritePointer_oneThirdOctave++;
            if (gWritePointer_oneThirdOctave >= 2) {
                gWritePointer_oneThirdOctave = 0;
            }

        }// end one third octave if statement
     }// end big for loop

}// end render function

// cleanup_render() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in initialise_render().

void cleanup_render()
{

}
