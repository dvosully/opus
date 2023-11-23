#ifndef NNDSP_H
#define NNDSP_H

#include "opus_types.h"
#include "nnet.h"
#include <string.h>


#define ADACONV_MAX_KERNEL_SIZE 15
#define ADACONV_MAX_INPUT_CHANNELS 2
#define ADACONV_MAX_OUTPUT_CHANNELS 2
#define ADACONV_MAX_FRAME_SIZE 80
#define ADACONV_MAX_OVERLAP_SIZE 40

#define ADACOMB_MAX_LAG 300
#define ADACOMB_MAX_KERNEL_SIZE 15
#define ADACOMB_MAX_FRAME_SIZE 80
#define ADACOMB_MAX_OVERLAP_SIZE 40

// #define DEBUG_NNDSP
#ifdef DEBUG_NNDSP
#include <stdio.h>
#endif

void print_float_vector(const char* name, float *vec, int length);

typedef struct {
    float history[ADACONV_MAX_KERNEL_SIZE * ADACONV_MAX_INPUT_CHANNELS];
    float last_kernel[ADACONV_MAX_KERNEL_SIZE * ADACONV_MAX_INPUT_CHANNELS * ADACONV_MAX_OUTPUT_CHANNELS];
    float last_gain;
} AdaConvState;


typedef struct {
    float history[ADACOMB_MAX_KERNEL_SIZE + ADACOMB_MAX_LAG];
    float last_kernel[ADACOMB_MAX_KERNEL_SIZE];
    float last_gain;
    float last_global_gain;
} AdaCombState;


void init_adaconv_state(AdaConvState *hAdaConv);

void init_adacomb_state(AdaCombState *hAdaComb);

void adaconv_process_frame(
    AdaConvState* hAdaConv,
    float *x_out,
    float *x_in,
    float *features,
    LinearLayer *kernel_layer,
    LinearLayer *gain_layer,
    int feature_dim, // not strictly necessary
    int frame_size,
    int overlap_size,
    int in_channels,
    int out_channels,
    int kernel_size,
    int left_padding,
    float filter_gain_a,
    float filter_gain_b,
    float shape_gain,
    float *window
);

void adacomb_process_frame(
    AdaCombState* hAdaComb,
    float *x_out,
    float *x_in,
    float *features,
    int frame_size,
    int overlap_size,
    float filter_gain_a,
    float filter_gain_b,
    float log_gain_limit,
    float *window
);

void adashape_process_frame(void);

#endif