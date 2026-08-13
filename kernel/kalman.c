#include "kalman.h"

void kalman_init(kalman_t *k, float initial_estimate, float process_noise, float measurement_noise) {
    k->x = initial_estimate;
    k->p = 1.0f;
    k->q = process_noise;
    k->r = measurement_noise;
}

float kalman_update(kalman_t *k, float measurement) {
    
    k->p = k->p + k->q;

    
    float gain = k->p / (k->p + k->r);

    k->x = k->x + gain * (measurement - k->x);
    k->p = (1.0f - gain) * k->p;

    return k->x;
}