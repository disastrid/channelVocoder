/* test doc for bandpass coefficients*/

#include <stdio.h>

#define SAMPLE_RATE 44100.0
#define SQRT_2 1.41421356237

int main() {

    // Great, we have geometric means. Now we need a for loop to calculate the coeffs for each band.
    // First we need some numbers or something:
    float t = 1/SAMPLE_RATE;
    float t2 = t*t;
    float q = SQRT_2; // q is sqrt(2) for 10 bands and 10 bands is all I can handle.

    // GET GEOMETRIC MEANS:
    float upperCutoff = 44.0;
    float lowerCutoff = 22.0; 
    float mean;
    mean = sqrt(upperCutoff * lowerCutoff);

    // NOW CALCULATE:
    float a1, a2, b0, b1, b2;
    float w0 = mean;
    float w02 = w0*w0;
    float norm = 4+ (w0*2*t/q) + w0*t*2;
    a1 = (-8 + w0*2*t2)/norm;
    a2 = (4-w0*t*2/q + w0*t2)/norm;
    b0 = (w0*2*t/q)/norm;
    b1 = 0;
    b2 = (-1*w0*2*t/q)/norm;
    
    printf("geometric mean: %f\n", mean);
    printf("coefficients:\n");
    printf("a1: %f a2: %f\n", a1, a2);
    printf("b0: %f b1: %f b2: %f\n", b0, b1, b2);    

    return 0;

}