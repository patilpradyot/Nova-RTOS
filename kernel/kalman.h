#ifndef KALMAN_H
#define KALMAN_H

typedef struct {
    float x;   // current estimate
    float p;   // estimate error covariance
    float q;   // process noise (system kitna "drift" karta hai)
    float r;   // measurement noise (sensor kitna "noisy" hai)
} kalman_t;

void kalman_init(kalman_t *k, float initial_estimate, float process_noise, float measurement_noise);
float kalman_update(kalman_t *k, float measurement);

#endif