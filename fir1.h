#ifndef FIR1_H
#define FIR1_H

#include <fftw3.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>


// simple buffer without wrap - used for input buffer while convolution processing
typedef struct buffer {
   int max_size;        // maximum size of Buffer
   int num_in_buffer;   // current number of buffer (also next write index)
   float *b;            // actual buffer data
} Buffer;

// circular buffer for output of each filter
typedef struct circular_buffer {
  int max_size;       // maximum size of buffer
  int num_in_buffer;  // current number of samples in buffer
  float *b;           // actual buffer data
  int next_write_idx; // index for the next write of data to buffer
  int next_read_idx;  // index for the next read of data from buffer
} CircBuffer;

// simple biquad chaining for IIR is supported.
typedef struct biquad {
  double dn;
  double x1, x2, y1, y2;
  double b0, b1, b2, a1, a2;
}Biquad;

// FIR filter structure
typedef struct fir_filter{
  int len;          // filter length for this FIR filter
  int fft_len;      // Length of FFT processed by engine
  double oo_fft_len;  // 1/length of fft
  int segment_len;  // segment LENGTH
  int overlap_size; // overlap size
  int num_extra_frames; // number of frames extra to reduce chance of xrun

  float *re;        // real samples of filter
  float *hc;        // forward transformed filter input (half-complex)
  float *in_hc;     // input segment for filter shared amongst all filters from parent ConvEngine
  float *out_hc;    // half-complex after freq domain multiply
  float *out_r;     // real output of filter
  float *overlap_r; // portion saved for next block

  int num_output_biquads;  // number of biquad sections for IIR filtering on output of filter
  Biquad *biquads;  // array of biquads executed on filter output

  CircBuffer *outBuffer; // circular output buffer

} FirFilter;

// convolution engine  1 input and N  (# fir filters) output. Output data in outBuffer of FirFilter
struct conv_engine {
  int fft_len;        // length of fft
  double oo_fft_len;  // 1/length of fft
  int filter_len;     // len of filter (shoudl be <= 1/2 fft_len for performance)
  int segment_len;    // len of segement

  float *in_r;        // input samples real
  float_*in_hc;       // input data forward transformed
  Buffer *inBuffer;   // buffer for input data when convolution engine processing a block

  int num_input_biquads;    // number of biqaud sections on input channel
  Biquad *biquads;    // array of biquads executed on input signal

  int num_fir_filters;  // number of unique outputs for single input
  FirFilter **fir_filters; // array of pointers to FirFilter structs
  int num_extra_frames; // number of frames extra to reduce chance of xrun

  bool conv_processing;  // variable used to indicate Convolution engine is busy processing
  pthread_mutex_t mut_conv;  // mutex for protecting writing of conv_processing

  pthread_mutex_t mut_in_buffer;  //mutex to hold exclusive access to input buffer
  pthread_mutex_t mut_out_buffer; //mutex to hold exclusive access to output buffer

  pthread_cond_t cond_input_segment_ready; // condition variable for thread signalling for input segment contains enough data to run convolution block
  pthread_cond_t cond_output_segment_ready; // condition variable when output segment is ready

} ConvEngine;


typedef struct biquad {
  double dn;
  double x1, x2, y1, y2;
  double b0, b1, b2, a1, a2;
}Biquad;

// one shot buffer function prototypes
int Buffer_Init(Buffer *buf, int capacity);
int Buffer_Destroy(Buffer *buf);

// circular buffer prototypes
int CircBuffer_Init(CircBuffer* cbuf, int capacity);
int CircBuffer_Destroy(CircBuffer* cbuf);

// FirFilter prototypes
int FirFilter_Init(FirFilter *f, int len, int fft_len, int num_extra_frames, int num_biquads, Biquad *biquads);
int FirFilter_Destroy(FirFilter *f);
int FirFilter_LoadFilter(FirFilter *f, int len, float *impulse);
int FirFilter_DiracFilter(FirFilter *f);

// ConvEngine prototypes
int ConvEngine_Init(ConvEngine *ce, int filter_len, int fft_len, int num_fir_filters, int num_input_biquads );
int
#endf //FIR1_H