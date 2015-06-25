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



#include <stdio.h>
#include <math.h>

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
#define PI 3.141592653



// cutoff frequencies (w0) should be the geometric mean of the lower and upper cutoffs.
// geometric mean = sqrt(f1 * f2)

float gGeoMeans[10];
// webpage explaining the calculations of various bands: sengpielaudio.com/calculator-octave.htm
// L = 10 * log(1/3) - 1/3 octaves


float gCutoffFreqs[11] = {22, 44, 88, 177, 355, 710, 1420, 2840, 5680, 11360};

float gCoeffs[10][5]; // 10 bands, 5 coeffs - a1, a2, b0, b1, b2. index 0 is lowest freq band.
// Buffers for prev output samples - one set for each goddamn band, one array per modulator and carrier:
float gBandwidths[10];


int main( int argc, char *argv[] )
{

    // Find the geometric mean for each band:
    for (int i = 0; i < 10; i++) { // < 10 because of the i+1 .. if it's <11 we'll go out of range
        float mean, result;
        // Prewarp the upper and lower cutoffs before multiplying together:
        mean = 2/t * tan(gCutoffFreqs[i]*2*PI*t/2) * 2/t * tan(gCutoffFreqs[i+1]*2*PI*t/2);
        result = sqrt(mean);
        gGeoMeans[i] = result;
    }
    // Great, we have geometric means. Now we need a for loop to calculate the coeffs for each band.
    // First we need some numbers or something:
    float t = 1/SAMPLE_RATE;
    float t2 = t*t;
    float q = SQRT_2; // q is sqrt(2) for 10 bands and 10 bands is all I can handle.

    for (int i = 0; i < 10; i++) {
        float t = 1/SAMPLE_RATE; // T is the sampling period, ie the amount of time between samples. We get this by calculating 1/sample rate
        
        float t2 = t*t; // again, just saves calculating this over and over!
            
        if (i == 0) { // calculate lowpass filter at the bottom
            
            float w0 = gGeoMeans[i]; // this will be used when we're taking in the input from the command line
            float w02 = w0*w0; 
            float q = SQRT_2/2;
            float norm = 4+((w0*2*t)/q) + w0*t2; // normalising factor, as explained in the accompanying doc
            
            gCoeffs[i][0] = (-8 + 2*w0*t2)/norm; // calculate LPF a1.
            gCoeffs[i][1] = (4-((w0*2*t)/q)+w0*t2)/norm; // LPF a2
            gCoeffs[i][2] = (w02*t2) / norm;
            gCoeffs[i][3] = (2*w02*t2) / norm;
            gCoeffs[i][4] = gCoeffs[i][2];
        
        } else if (i == 9) { // calculate highpass filter at the top
            float w0 = 2/t * tan(gCutoffFreqs[i]*PI*2*t/2) ; // this will be used when we're taking in the input from the command line
            float w02 = w0*w0; // our cutoff frequency squared - saves calculating over and over
            float q = SQRT_2/2;
            float norm = 4 + ((w0*2*t)/q) + w0*t2; // normalising factor, as explained in the accompanying doc
            
            gCoeffs[i][0] = (-8 + 2*w0*t2)/norm;// hpf a1
            gCoeffs[i][1] = (4-((w0*2*t)/q)+w0*t2)/norm;// hpf a2
            gCoeffs[i][2] = 4 / norm;
            gCoeffs[i][3] = -8 / norm;
            gCoeffs[i][4] = gCoeffs[i][2];
        } else {
        
        float bpfQ = SQRT_2; // Q for BPF
        float w0 = 2/t*tan(gGeoMeans[i]*PI*2*t/2); // convert w0 to radians
        float w02 = w0 * w0;
        float bpfNorm = 4 + (w0*2*t/bpfQ) + w02*t2; // calculate normalising factor
        for (int j = 0; j < 5; j++) {
            if (j == 0)
                // calculate a1.
                gCoeffs[i][j] = (-8 + w02*t2*2)/bpfNorm;
            if (j == 1)
                // calculate a2.
                gCoeffs[i][j] = (4-(w0*t*2/bpfQ) + w02*t2)/bpfNorm;
            if (j == 2)
                // calculate b0.
                gCoeffs[i][j] = (w0*2*t/bpfQ)/bpfNorm;
            if (j == 3)
                // b1 will always be 0.
                gCoeffs[i][j] = 0;
            if (j == 4)
                // calculate b2.
                gCoeffs[i][j] = (-w0*2*t/bpfQ)/bpfNorm;
        }
    }
}

    for (int i = 0; i < 10; i++) {
        printf(". . . . \n");
        if (i == 0) {
            printf("THIS IS THE LOWPASS FILTER\n");
        }
        if (i == 9) {
            printf("THIS IS THE HIGHPASS FILTER\n");
        }
        
        printf("BAND %d:\n", i);
        if (i > 0 && i < 9) {
        printf("lower cutoff: %f high cutoff: %f\n", gCutoffFreqs[i], gCutoffFreqs[i+1]);
        printf("Geo mean: %f\n", gGeoMeans[i]);
        } else {
        printf("cutoff: %f\n", gCutoffFreqs[i]);
        
        }
        printf("COEFFICIENTS: \n");
        printf("a1: %f a2: %f\n", gCoeffs[i][0], gCoeffs[i][1]);
        printf("b0: %f b1: %f b2: %f\n", gCoeffs[i][2], gCoeffs[i][3], gCoeffs[i][4]);
    }
    return(0);
}
