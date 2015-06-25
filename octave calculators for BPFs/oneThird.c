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

float gGeoMeans[28];
// webpage explaining the calculations of various bands: sengpielaudio.com/calculator-octave.htm
// L = 10 * log(1/3) - 1/3 octaves


float gCutoffFreqs[28] = {22.4, 28.2, 35.5, 
                          47.7, 56.2, 70.8, 
                          89.1, 112, 141, 
                          178, 224, 282, 
                          355, 447, 562, 
                          708, 891, 1122, 
                          1413, 1778, 2239, 
                          2818, 3548, 4467,
                          5623, 7079, 8913, 
                          11220};

float gCoeffs[28][5]; // 10 bands, 5 coeffs - a1, a2, b0, b1, b2. index 0 is lowest freq band.
// Buffers for prev output samples - one set for each goddamn band, one array per modulator and carrier:
float gBandwidths[10];


int main( int argc, char *argv[] )
{
    float t = 1/SAMPLE_RATE;
    // Find the geometric mean for each band:
    for (int i = 0; i < 28; i++) { // < 10 because of the i+1 .. if it's <11 we'll go out of range
        float mean, result;
        mean = 2/t * tan(gCutoffFreqs[i]*2*PI*t/2) * 2/t * tan(gCutoffFreqs[i]*2*PI*t/2);
        result = sqrt(mean);
        gGeoMeans[i] = result;
    }
    // Great, we have geometric means. Now we need a for loop to calculate the coeffs for each band.
    // First we need some numbers or something:
    
    float t2 = t*t;
    //float q = SQRT_2; // q is sqrt(2) for 10 bands and 10 bands is all I can handle.
    float w0;

    for (int i = 0; i < 28; i++) {
        
            
        if (i == 0) { // calculate lowpass filter at the bottom
            
            w0 = 2/t * tan(gCutoffFreqs[i]*2*PI*t/2); // this will be used when we're taking in the input from the command line
            float w02 = w0*w0; 
            float q = SQRT_2/2;
            float norm = 4+((w0*2*t)/q) + w0*t2; // normalising factor, as explained in the accompanying doc
            
            gCoeffs[i][0] = (-8 + 2*w02*t2)/norm; // calculate LPF a1.
            gCoeffs[i][1] = (4-((w0*2*t)/q)+w0*t2)/norm; // LPF a2
            gCoeffs[i][2] = (w02*t2) / norm;
            gCoeffs[i][3] = (2*w02*t2) / norm;
            gCoeffs[i][4] = gCoeffs[i][2];
            printf("%f, %f, %f, %f, %f\n", gCoeffs[i][0], gCoeffs[i][1], gCoeffs[i][2], gCoeffs[i][3], gCoeffs[i][4]);
        
        } else if (i == 27) { // calculate highpass filter at the top
            w0 = 2/t * tan(gCutoffFreqs[i]*PI*2*t/2) ; // this will be used when we're taking in the input from the command line
            float w02 = w0*w0; // our cutoff frequency squared - saves calculating over and over
            float q = SQRT_2/2;
            float norm = 4 + ((w0*2*t)/q) + w02*t2; // normalising factor, as explained in the accompanying doc
            
            gCoeffs[i][0] = (-8 + 2*w02*t2)/norm;// hpf a1
            gCoeffs[i][1] = (4-((w0*2*t)/q)+w02*t2)/norm;// hpf a2
            gCoeffs[i][2] = 4 / norm;
            gCoeffs[i][3] = -8 / norm;
            gCoeffs[i][4] = gCoeffs[i][2];
            printf("%f, %f, %f, %f, %f\n", gCoeffs[i][0], gCoeffs[i][1], gCoeffs[i][2], gCoeffs[i][3], gCoeffs[i][4]);
        } else {
        
        float bpfQ = 4.32; // Q for BPF
        w0 = gGeoMeans[i]; // convert w0 to radians
        float w02 = w0 * w0;
        float bpfNorm = 4 + (w0*2*t/bpfQ) + w02*t2; // calculate normalising factor

            gCoeffs[i][0] = (-8 + w02*t2*2)/bpfNorm;
            gCoeffs[i][1] = (4-(w0*t*2/bpfQ) + w02*t2)/bpfNorm;
            gCoeffs[i][2] = (w0*2*t/bpfQ)/bpfNorm;
            gCoeffs[i][3] = 0;
            gCoeffs[i][4] = (-w0*2*t/bpfQ)/bpfNorm;
        printf("%f, %f, %f, %f, %f\n", gCoeffs[i][0], gCoeffs[i][1], gCoeffs[i][2], gCoeffs[i][3], gCoeffs[i][4]);

        }
        
    }



    return(0);
}
