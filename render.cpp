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
#include "drums.h"
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



// cutoff frequencies (w0) should be the geometric mean of the lower and upper cutoffs.
// geometric mean = sqrt(f1 * f2)

float gGeoMeans[10];
// webpage explaining the calculations of various bands: sengpielaudio.com/calculator-octave.htm
// L = 10 * log(1/3) - 1/3 octaves


float gCutoffFreqs[11];

int gCarrierFreq = 440;
int gNumberOfSamplesPassed = 0;
float gSawCarrierOut = 0;
int gNumSamplesPerTooth;
float gPhaseAtEndOfPrevFrame = 0;
float gIntervalOfStepsToTopOfTooth;
int gNumOfBands = 10;
float gCoeffs[10][5]; // 10 bands, 5 coeffs - a1, a2, b0, b1, b2. index 0 is lowest freq band.
// Buffers for prev output samples - one set for each goddamn band, one array per modulator and carrier:
float gCarrierY[10][2];
float gModulatorY[10][2];
// Input samples are the same for each band, so no 2D screwiness:
float gCarrierX[2];
float gModulatorX[2];
float gPeaks[10];
float gReleaseCoefficient = 0.9;
float gBandOutputsToBeSummed[10];

// One lonely little write pointer:
int gWritePointer;

bool initialise_render(int numMatrixChannels, int numAudioChannels,
                       int numMatrixFramesPerPeriod,
                       int numAudioFramesPerPeriod,
                       float matrixSampleRate, float audioSampleRate,
                       void *userData)
{
    // STUFF FOR CARRIER SIGNAL.
    // Maybe move this to render() to make it adjustable by the potentiometer through matrixIn?
    gCarrierFreq = *(float *)userData; // if no user data, it starts at 440. I would like it to be
                                        // selectable via a potentiometer but let's not get crazy.
    gNumSamplesPerTooth = SAMPLE_RATE / gCarrierFreq; // calculate that for the sawtooth wave
    gIntervalOfStepsToTopOfTooth = 2.0/float(gNumSamplesPerTooth); // range of 2: -1 to 1. ie for 50hz this would be ~0.0022675


    // SCRUB A DUB DUB: Initialise the delay buffers to 1 so there's no 0 coefficients.
    for (int i = 0; i < 2; i++) {
        gModulatorX[i] = 1;
        gCarrierX[i] = 1;
    }
    // Initialise the 2D sample buffers to 1.
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 2; j++) {
            gCarrierY[i][j] = 1;
            gModulatorY[i][j] = 1;
        }
    }
    // Initialise peaks to zero.
    for (int i = 0; i < 10; i++) {
        gPeaks[i] = 0;
    }

    // Find the upper and lower frequencies for each band:
    for (int i = 0; i < 10; i++) {
        float freq = 22.0;
        gCutoffFreqs[i] = freq;
        freq = freq * 2;
    }

    // Find the geometric mean for each band:
    for (int i = 0; i < 10; i++) { // < 9 because of the i+1 .. if it's <11 we'll go out of range
        float mean;
        mean = sqrt(gCutoffFreqs[i] * gCutoffFreqs[i+1]);
        gGeoMeans[i] = mean;
    }
    // Great, we have geometric means. Now we need a for loop to calculate the coeffs for each band.
    // First we need some numbers or something:
    float t = 1/SAMPLE_RATE;
    float t2 = t*t;
    float q = SQRT_2; // q is sqrt(2) for 10 bands and 10 bands is all I can handle.

    // NOW CALCULATE:
    for (int i = 0; i < 10; i++) {
        float w0 = gGeoMeans[i];
        float w02 = w0*w0;
        float norm = 4+ (w0*2*t/q) + w0*t*2;
        for (int j = 0; j < 5; j++) {
            switch (j) {
            case 0:
                // calculate a1.
                gCoeffs[i][j] = (-8 + w0*2*t2)/norm;
                break;
            case 1:
                // calculate a2.
                gCoeffs[i][j] = (4-w0*t*2/q + w0*t2)/norm;
                break;
            case 2:
                // calculate b0.
                gCoeffs[i][j] = (w0*2*t/q)/norm;
                break;
            case 3:
                // b1 will always be 0.
                gCoeffs[i][j] = 0;
                break;
            case 4:
                // calculate b2.
                gCoeffs[i][j] = (-1*w0*2*t/q)/norm;
                break;
            default:
                // something is seriously wrong if it's something other than the above. Evacuate!
                break;
            }
        }
    }

    return true;
}

// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numMatrixFrames
// will be 0.

void render(int numMatrixFrames, int numAudioFrames, float *audioIn, float *audioOut,
            float *matrixIn, float *matrixOut)
{
    /*
     *  POSSIBLE SOLUTION FOR USING POTENTIOMETER TO CONTROL TEMPO
     *  // First, get the value from the analog pin (channel 0):
     *  float potentiometerReading = matrixIn[(n/2)*8+0];
     *  // Our range is about 0.0-0.82. Map that to the range 50-2000 to get a range of Hz for the carrier:
     *  float reading = map(potentiometerReading, 0.0, 0.82, 50, 2000);
     *  // Cast that mapped float value to an int:
     *  int carrierFrequency = reading;
     */

    /*
     * TESTING BANDS
     *
     * for (int n = 0; n < numAudioFrames; n++) {
     *      // Generate carrier wave.
     *      if (n == 0)
     *          gSawCarrierOut = gPhaseAtEndOfPrevFrame;
     *      gSawCarrierOut += gIntervalOfStepsToTopOfTooth;
     *      if (gSawCarrierOut >= 1.0)
     *          gSawCarrierOut = -1.0;
     *      if (n >= (numAudioFrames-1))
     *          gPhaseAtEndOfPrevFrame = gSawCarrierOut;
     *
     *      // Now, put that sample through 2 band filters.
     *      float carrierSamplesOut[10];
     *      for (int i = 0; i < 10; i++) {
     *          carrierSamplesOut[i] = gCoeffs[i][2] * gSawCarrierOut + gCoeffs[i][3] * gCarrierX[(gWritePointer + 2 - 1) % 2] + gCoeffs[i][4] * gCarrierX[gWritePointer] - gCoeffs[i][0] * gCarrierY[i][(gWritePointer + 2 - 1) % 2] - gCoeffs[i][1] * gCarrierY[i][gWritePointer];
     *      // carrier output buffers:
     *      gCarrierY[i][gWritePointer] = modulatorSamplesOut[i];
     *      }
     *
     *      // Put the input in the X delay buffer:
     *      gCarrierX[gWritePointer] = gSawCarrierOut;
     *
     *      // Put the first band's output on the right channel:
     *      audioOut[n*2] = carrierSamplesOut[0];
     *      // Put the second band's output on the left channel:
     *      audioOut[n*2+1] = carrierSamplesOut[1];
     *
     *      // Finally, advance the write pointer:
     *      gWritePointer++;
     *          if (gWritePointer >= 2)
     *      gWritePointer = 0;
     *
     *
     * }
     */

    for (int n = 0; n < numAudioFrames; n++) {
        // GET A CARRIER SAMPLE.
        if (n == 0) {
            // If we're on the first frame, set the value to whatever the last value was of the last frame:
            gSawCarrierOut = gPhaseAtEndOfPrevFrame;
        }
            // ... otherwise, business as usual:
        gSawCarrierOut += gIntervalOfStepsToTopOfTooth;
        // OVERFLOW CONTROL: If we reach 1.0, start at -1.0 again and repeat the process:
        if (gSawCarrierOut >= 1.0) { // or should this be gSawCarrierOut >= 1?
            gSawCarrierOut = -1.0;
        }
        // KEEPING PHASE: If we're on the last sample of the frame (n=numFrames-1) then set gPhaseAtEndOfPrevFrame
        // to whatever value we're on now:
        if (n >= (numAudioFrames-1)) {
            gPhaseAtEndOfPrevFrame = gSawCarrierOut;
        }


        // GET A MODULATION (VOICE) SAMPLE.
        float monoVoiceSample = (audioIn[n*2]+audioIn[n*2+1])/2;

        // TO BE CONTINUED:
        // Shove the voice sample through each band using a for loop.
        // Store the output samples.
        // Implement circular buffer thing for storage and retrieval.

        float carrierSamplesOut[10];
        float modulatorSamplesOut[10];
        for (int i = 0; i < 10; i++) { // for each band ...
            // do the filtering on both the carrier and modulator signals.
            // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
            // Two 2D buffers for the output - one for carrier signal, one for modulator signal:
            carrierSamplesOut[i] = gCoeffs[i][2] * gSawCarrierOut + gCoeffs[i][3] * gCarrierX[(gWritePointer + 2 - 1) % 2] + gCoeffs[i][4] * gCarrierX[gWritePointer] - gCoeffs[i][0] * gCarrierY[i][(gWritePointer + 2 - 1) % 2] - gCoeffs[i][1] * gCarrierY[i][gWritePointer];
            modulatorSamplesOut[i] = gCoeffs[i][2] * monoVoiceSample + gCoeffs[i][3] * gModulatorX[(gWritePointer + 2 - 1) % 2] + gCoeffs[i][4] * gModulatorX[gWritePointer] - gCoeffs[i][0] * gModulatorY[i][(gWritePointer + 2 - 1) % 2] - gCoeffs[i][1] * gModulatorY[i][gWritePointer];
            // ENVELOPE FOLLOWER:
            // (This needs a level detector)
            if (abs(modulatorSamplesOut[i]) > gPeaks[i]) {
                gPeaks[i] = modulatorSamplesOut[i];
            } else {
                gPeaks[i] -= gReleaseCoefficient;
            }
            // Put each output in the delay buffer, replacing the oldest sample:
            gModulatorY[i][gWritePointer] = carrierSamplesOut[i];
            gCarrierY[i][gWritePointer] = modulatorSamplesOut[i];
            gBandOutputsToBeSummed[i] = carrierSamplesOut[i] * gPeaks[i];
        }

        // Add up the signals on all 10 bands:
        float summedBands;
        for (int i = 0; i < 10; i++) {
            summedBands = gBandOutputsToBeSummed[i];
        }

        // OUTPUT
        // Put the summed values of all bands on the output.

        audioOut[n*2] = summedBands;
        audioOut[n*2+1] = summedBands;


        // NOW put the current X samples in their buffers, to replace the oldest samples
        gCarrierX[gWritePointer] = gSawCarrierOut;
        gModulatorX[gWritePointer] = monoVoiceSample;

        // ADVANCE THE WRITE POINTER
        gWritePointer++;
        if (gWritePointer >= 2)
            gWritePointer = 0;


    } // end big FOR loop that goes through samples
        // Updated to do list:

        // Find a way to check values of band pass - goddammit MATLAB.
        // Use scope and get readings for carrier signal in various bands with test written at top of render()
        //      (comment out the rest).
        // Finesse level detector - find a better algorithm.
        // White noise for sibilance.
        // Potentiometer - possible solution at top of render()

        // MAKE A SAWTOOTH CARRIER SIGNAL. - Kind of there.
        // CALCULATE COEFFS. - Done. Need to double check values with MATLAB (Mar 30 - tried to double check with MATLAB
        //      but I have no idea how to get sane values out of MATLAB for bandpass filters.)
        // SHOVE THEM BOTH THROUGH THE BANK OF BAND PASS FILTERS USING COEFFS ABOVE. - Done
        // ENVELOPE FOLLOWER - elementary one in place, needs finesse.
        // WHITE NOISE FOR SIBILANCE - omg how
        // ADJUST CARRIER FREQUENCY WITH POT OR SOMETHING - Maybe ridiculous.
        // FIGURE OUT HOW TO CALCULATE FINAL OUTPUT - Done!
        // ???
} // end render function

// cleanup_render() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in initialise_render().

void cleanup_render()
{

}
