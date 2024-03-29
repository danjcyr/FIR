#include <fftw3.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "fir1.h"
FILE *fpin, *fpin2, *fpout;

int FirBuffer_Init(FirBuffer **pbuf, int segment_len, int fft_length)

{
	printf("initfb - 0\n");
	*pbuf = (FirBuffer*) malloc(sizeof(FirBuffer));
	FirBuffer *buf = *pbuf;
	assert(buf != NULL);
	buf->segment_len = segment_len;
	buf->max_size = fft_length;
	buf->num_in_buffer = 0;	// also the next write index
	buf->b1 = fftwf_alloc_real(fft_length);
	buf->b2 = fftwf_alloc_real(fft_length);
	for (int i = 0; i < buf->max_size; i++)
	{
		buf->b1[i] = 0;
		buf->b2[i] = 0;
	}
	buf->write_buffer = buf->b1;	// setup to write into buffer 1
	buf->read_buffer = NULL;	// not ready for processing
	buf->buffer_ready=0;
	return 0;
}

int FirBuffer_AddData(FirBuffer *buf, int len, float *data)
{
	// if entire write fits in current buffer - write and return

	if ((buf->num_in_buffer + len) < buf->segment_len)
	{
		memcpy(&buf->write_buffer[buf->num_in_buffer], data, sizeof(float) *len);
		buf->num_in_buffer += len;
//    printf("%s  %d\n"," firbuff add data no signal",buf->read_buffer );
		return 0;
	}
	int what_fits = buf->segment_len - buf->num_in_buffer;
	int remains = len - what_fits;
	memcpy(&buf->write_buffer[buf->num_in_buffer], data, sizeof(float) *what_fits);
	for(int i=buf->segment_len;i < buf->max_size; i++) buf->write_buffer[i]=0;
	if (buf->read_buffer != NULL) printf("THIS IS BAD\n");
	if (buf->write_buffer == buf->b1)
	{
		buf->write_buffer = buf->b2;
		buf->read_buffer = buf->b1;
		buf->buffer_ready=1;

	}
	else
	{
		buf->write_buffer = buf->b1;
		buf->read_buffer = buf->b2;
		buf->buffer_ready=2;
	}
	memcpy(buf->write_buffer, &data[what_fits], sizeof(float) *remains);
	buf->num_in_buffer = remains;
	return 0;
}

int FirBuffer_Destroy(FirBuffer *buf)
{
	if (buf != NULL)
	{
		if (buf->b1 != NULL)
		{
			free(buf->b1);
		}
		if (buf->b2 != NULL)
		{
			free(buf->b2);
		}
		free(buf);
		buf = NULL;
	}
	return 0;
}

// circular buffer Init()
int CircBuffer_Init(CircBuffer **pcbuf, int capacity)
{
	CircBuffer * cbuf;

	*pcbuf = (CircBuffer*) malloc(sizeof(CircBuffer));
	cbuf = *pcbuf;
	cbuf->max_size = capacity;
	cbuf->num_in_buffer = 0;
	cbuf->b = (float*) malloc(sizeof(float) *capacity);
	for (int i = 0; i < capacity; i++) cbuf->b[i] = 0.0;
	cbuf->next_write_idx = 0;
	cbuf->next_read_idx = 0;

	return 0;
}

// function will add data (if it fits) into circular buffer
int CircBuffer_AddData(CircBuffer *cbuf, int len, float *data)
{
	int new_size = cbuf->num_in_buffer + len;
	if (new_size > cbuf->max_size)
	{
		fprintf(stderr, "Cicular buffer overflow \n");
		exit(-1);
	}
	// if all data fits before the end of the buffer - copy and return
	if ((cbuf->next_write_idx + len) < cbuf->max_size)
	{
		memcpy(&cbuf->b[cbuf->next_write_idx], data, sizeof(float) *len);
		cbuf->next_write_idx += len;
		cbuf->num_in_buffer += len;
		return 0;
	}
	int what_fits = cbuf->max_size - cbuf->next_write_idx;
	int remains = len - what_fits;

	// first copy what fits (may be zero bytes actually!)
	memcpy(&cbuf->b[cbuf->next_write_idx], data, sizeof(float) *what_fits);
	// now copy remains and update indices
	memcpy(cbuf->b, &data[what_fits], remains* sizeof(float));
	cbuf->next_write_idx = remains;
	cbuf->num_in_buffer += len;

	return 0;
}

int CircBuffer_GetData(CircBuffer *cbuf, int len, float *dst)
{
	if (cbuf->num_in_buffer < len)
	{
		fprintf(stderr, "CircBuffer_GetData underrun\n");
	}
	if ((cbuf->next_read_idx + len) < cbuf->max_size)
	{
		memcpy(dst, &cbuf->b[cbuf->next_read_idx], sizeof(float) *len);
		cbuf->next_read_idx += len;
		cbuf->num_in_buffer -= len;
		return 0;
	}

	int sz_first = cbuf->max_size - cbuf->next_read_idx;
	int remains = len - sz_first;

	memcpy(dst, &cbuf->b[cbuf->next_read_idx], sizeof(float) *sz_first);
	memcpy(&dst[sz_first], cbuf->b, sizeof(float) *remains);
	cbuf->next_read_idx = remains;
	cbuf->num_in_buffer -= len;
	return 0;
}

int CircBuffer_Destroy(CircBuffer *cbuf)
{
	assert(cbuf != NULL);
	assert(cbuf->b != NULL);
	free(cbuf->b);
	free(cbuf);

	return 0;
}

// FirFilter prototypes
int FirFilter_Init(FirFilter **pf, int len, int fft_len, int num_extra_frames, int sample_rate)
{
	*pf = (FirFilter*) malloc(sizeof(FirFilter));
	FirFilter *f = *pf;
	f->len = len;
	f->fft_len = fft_len;
	f->oo_fft_len = 1.0 / (double) f->fft_len;
	f->sample_rate = sample_rate;
	f->num_extra_frames = num_extra_frames;
	f->overlap_size = len - 1;
	f->segment_len = f->fft_len - f->len + 1;
	// do nothing for biquads here Initialized elswhere
	f->num_output_biquads = 0;
	f->re = fftwf_alloc_real(f->len);
	f->hc = fftwf_alloc_real(f->len);
	f->plan_filter = NULL;
	//  f->in_hc = fftwf_alloc_real(f->len);
	f->out_hc = fftwf_alloc_real(f->fft_len);
	f->out_r = fftwf_alloc_real(f->fft_len);
	f->overlap_r = fftwf_alloc_real(f->overlap_size);
  for(int i=0;i<f->overlap_size;i++)f->overlap_r[i]=0;
	f->biquads = NULL;

	f->plan_output = fftwf_plan_r2r_1d(f->fft_len, f->out_hc, f->out_r, FFTW_HC2R,
		  FFTW_ESTIMATE);

	CircBuffer_Init(&f->outBuffer, 2 *f->segment_len + f->num_extra_frames);
	f->outBuffer->next_write_idx = f->segment_len + f->num_extra_frames;
	f->outBuffer->num_in_buffer = f->segment_len + f->num_extra_frames;

	return 0;
}

int FirFilter_Destroy(FirFilter *f)
{
	if (f->outBuffer) CircBuffer_Destroy(f->outBuffer);
	if (f->overlap_r) fftwf_free(f->overlap_r);
	if (f->out_r) fftwf_free(f->out_r);
	if (f->out_hc) fftwf_free(f->out_hc);
	// more cleanup needed
	return 0;
}

int FirFilter_LoadFilter(FirFilter *f, int len, float *impulse)
{
	FILE *fpf = fopen("filter_lf_in.txt","w");
	FILE *fpf1 = fopen("filter_lf_hc.txt","w");
	if (f->re) fftwf_free(f->re);
	if (f->hc) fftwf_free(f->hc);

	f->len = len;
	f->re = fftwf_alloc_real(f->fft_len);
	f->hc = fftwf_alloc_real(f->fft_len);

	if (f->plan_filter != NULL) fftwf_destroy_plan(f->plan_filter);
	f->plan_filter = fftwf_plan_r2r_1d(f->fft_len, f->re, f->hc,
		FFTW_R2HC, FFTW_DESTROY_INPUT |  FFTW_ESTIMATE);
	for (int i=0;i<f->fft_len;i++) f->re[i]=0;

	memcpy(f->re, impulse, sizeof(float) *f->len);
	for (int i = 0; i < f->fft_len; i++) fprintf(fpf,"%f\n",f->re[i]);
	printf("FF_LF - len= %d  FFt_len = %d\n",f->len,f->fft_len);

	fftwf_execute(f->plan_filter);
	for (int i = 0; i < f->fft_len; i++) fprintf(fpf1,"%f\n",f->hc[i]);
	fclose(fpf);
	fclose(fpf1);

	return 0;
}

int FirFilter_DiracFilter(FirFilter *f)
{
	FILE *fpf = fopen("filter_dirac_in.txt","w");
	FILE *fpf1 = fopen("filter_dirac_hc.txt","w");
	if (f->re) fftwf_free(f->re);
	if (f->hc) fftwf_free(f->hc);

	f->re = fftwf_alloc_real(f->fft_len);
	f->hc = fftwf_alloc_real(f->fft_len);

  if (f->plan_filter != NULL) fftwf_destroy_plan(f->plan_filter);
  f->plan_filter = fftwf_plan_r2r_1d(f->fft_len, f->re, f->hc,
		FFTW_R2HC, FFTW_DESTROY_INPUT|  FFTW_ESTIMATE);
	for (int i=0;i<f->fft_len;i++) f->re[i]=0;
	printf("FF_DF - len= %d  FFt_len = %d \n",f->len,f->fft_len);
	f->re[f->len / 2] = 1.0;

	for (int i = 0; i < f->fft_len; i++) fprintf(fpf,"%f\n",f->re[i]);


	fftwf_execute(f->plan_filter);
  for (int i = 0; i < f->fft_len; i++) fprintf(fpf1,"%f\n",f->hc[i]);
	fclose(fpf);
	fclose(fpf1);
	return 0;
}

void Biquad_Init(Biquad *b)
{
	//initialize
	b->x1 = 0.0;
	b->x2 = 0.0;
	b->y1 = 0;
	b->y2 = 0;
	b->dn = 0;
}


// ConvEngine prototypes
int ConvEngine_Init(ConvEngine **pce, fir_conf_t cfg)
{
  printf("CE(init) fil_len = %d fft_len = %d  numfir = %d extra=%d\n",
		cfg.filter_len, cfg.fft_len, cfg.num_outputs, cfg.extra_samples);
	(*pce) = (ConvEngine*) malloc(sizeof(ConvEngine));
	ConvEngine *ce = (*pce);
	ce->fft_len = cfg.fft_len;
	ce->filter_len = cfg.filter_len;
	ce->sample_rate = cfg.sample_rate;
	ce->oo_fft_len = 1.0 / (double) ce->filter_len;
	ce->segment_len = ce->fft_len - ce->filter_len + 1;
	ce->overlap_size = ce->filter_len - 1;
	printf("olap size = %d  seg_len = %d\n",ce->overlap_size, ce->segment_len);

	ce->in_hc = fftwf_alloc_real(ce->fft_len);
	ce->fir_buffer = (FirBuffer*) malloc(sizeof(FirBuffer));
	FirBuffer_Init(&(ce->fir_buffer),  ce->segment_len,ce->fft_len);
	ce->plan_input1 = fftwf_plan_r2r_1d(ce->fft_len, ce->fir_buffer->b1, ce->in_hc,
		FFTW_R2HC,  FFTW_DESTROY_INPUT| FFTW_ESTIMATE);
		ce->plan_input2 = fftwf_plan_r2r_1d(ce->fft_len, ce->fir_buffer->b2, ce->in_hc,
			FFTW_R2HC,  FFTW_DESTROY_INPUT| FFTW_ESTIMATE);

	ce->num_input_biquads = cfg.num_input_biquads;
	printf("num in bq = %d\n",ce->num_input_biquads);
	if(cfg.num_input_biquads>0)
	{
		ce->biquads = (Biquad *)malloc(sizeof(Biquad)*ce->num_input_biquads);
		for(int b=0;b<ce->num_input_biquads;b++)
		{
			ce->biquads[b]=cfg.input_Biquads[b];
		}
	}
	else
	{
		ce->biquads = NULL;
	}
	ce->num_fir_filters = cfg.num_outputs;
	ce->fir_filters = (FirFilter **) malloc(sizeof(FirFilter*) *ce->num_fir_filters);
	ce->num_extra_frames = cfg.extra_samples;
	ce->output_buffers = (jack_default_audio_sample_t**)malloc(sizeof(jack_default_audio_sample_t*)* ce->num_fir_filters);
	ce->input_name=(char *)malloc(strlen(cfg.input_name));
	strcpy(ce->input_name,cfg.input_name);
	ce->client_name = (char *)malloc(strlen(cfg.client_name));
	strcpy(ce->client_name,cfg.client_name);
	ce->output_ports=(jack_port_t**)malloc(sizeof(jack_port_t *)*ce->num_fir_filters);
	ce->output_port_names = (char**)malloc(sizeof(char*)*ce->num_fir_filters);
	ce->output_buffers=(jack_default_audio_sample_t **)malloc(sizeof(jack_default_audio_sample_t*)*ce->num_fir_filters);
	for (int i = 0; i < ce->num_fir_filters; i++)
	{
		ce->output_port_names[i]=cfg.output_portnames[i];
		ce->fir_filters[i] = (FirFilter*) malloc(sizeof(FirFilter));
		FirFilter_Init(&ce->fir_filters[i], ce->filter_len, ce->fft_len, ce->num_extra_frames, ce->sample_rate);
		ce->fir_filters[i]->in_hc = ce->in_hc;
		printf("init fir %d\n",i);
		ce->fir_filters[i]->num_output_biquads=cfg.num_output_biquads[i];
		printf("fir # %d - num bq = %d \n", i,ce->fir_filters[i]->num_output_biquads);
		if(cfg.num_output_biquads[i]>0)
		{
			ce->fir_filters[i]->biquads = (Biquad *)malloc(sizeof(Biquad)*cfg.num_output_biquads[i]);
			for(int b=0;b<cfg.num_output_biquads[i];b++)
			{
				ce->fir_filters[i]->biquads[b]=cfg.output_Biquads[i][b];
			}
		}
	}
	pthread_mutex_init(&ce->mut_conv, NULL);
	pthread_mutex_init(&ce->mut_in_buffer, NULL);
	pthread_mutex_init(&ce->mut_out_buffer, NULL);
//	printf("CI mutin=%d\n",&ce->mut_in_buffer);
	printf("done\n");
	ce->conv_processing = false;
	pthread_create(&ce->thread_conv, NULL, ConvEngine_ConvThread, ce);

	return 0;
}

void ConvEngine_Dump(ConvEngine *ce)
{
	printf("\n\nConvengine\n");
	printf("num fir filters %d\n",ce->num_fir_filters);
	for(int i=0;i<ce->num_fir_filters;i++)
	{
//		printf("i=%d : outport= %d name=%s outbuff=%d\n",i,ce->output_ports[i],ce->output_port_names[i],ce->output_buffers[i]);
	}
}

void ProcessBQ(int nframes, Biquad *bqs,int nbq_used,float *data)
{
	double x,y;

	for(int i=0;i<nframes;i++)
    {
        double ph = data[i];
        for(int j=0;j<nbq_used;j++)
        {
            Biquad *f=&bqs[j];
            x=ph;
            y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2 + f->dn;
            f->dn = -f->dn;
            f->x2 = f->x1;
            f->x1 = x;
            f->y2 = f->y1;
            f->y1 = y;
            ph=y;
        }

        data[i] = (jack_default_audio_sample_t)ph;
	}
}

int ConvEngine_AddData(ConvEngine *ce, int len, float *data)
{
	if(ce->num_input_biquads>0)
	{
		ProcessBQ(len,ce->biquads,ce->num_input_biquads,data);
	}
//	printf("in add data - portname = %s \n" , ce->input_name);
//	printf("in add data - portname = %s mutex= %d \n" , ce->input_name,&ce->mut_in_buffer);
	pthread_mutex_lock(&ce->mut_in_buffer);
//	printf("in add data\n");
	FirBuffer_AddData(ce->fir_buffer, len, data);
//	printf("in add data 2\n");
	if (ce->fir_buffer->buffer_ready > 0)	// ready for FFT processing - signal
	{
  	pthread_cond_signal(&ce->cond_input_segment_ready);
	}
  pthread_mutex_unlock(&ce->mut_in_buffer);

	return 0;
}

// hc3 = hc1*hc2 (complex)
int FreqDomMultiply(FirFilter *f)
{
	register double x, y, u, v;
	// (x+yi)*(u+vi) = (xu-yv) + (xv+yu)i
	int n = f->fft_len;
	float *hc1 = f->in_hc;
	float *hc2 = f->hc;
	float *hc3 = f->out_hc;
	// k = 0
	x = hc1[0];
	u = hc2[0];
	hc3[0] = x * u;

	for (int k = 1; k < n/2; k++)
	{
		x = hc1[k];
		y = hc1[n - k];
		u = hc2[k];
		v = hc2[n - k];

		hc3[k] = (x *u - y *v);
		hc3[n - k] = (x *v + y *u);
	}
	// k=len/2=n
	x = hc1[n/2];
	u = hc2[n/2];
	hc3[n/2] = x * u;

	return 0;
}

int ConvEngine_GetData(ConvEngine *ce,int len, float** dst)
{
  pthread_mutex_lock(&ce->mut_out_buffer);
  for(int i=0;i<ce->num_fir_filters;i++)
  {
//		printf("CEgeting data i = %d \n",i);
    CircBuffer_GetData(ce->fir_filters[i]->outBuffer,len,dst[i]);
  }
  pthread_mutex_unlock(&ce->mut_out_buffer);
	for(int i=0;i<ce->num_fir_filters;i++)
  {
		if(ce->fir_filters[i]->num_output_biquads>0)
		{
			ProcessBQ(len,ce->fir_filters[i]->biquads,ce->fir_filters[i]->num_output_biquads,dst[i]);
		}
	}
  return 0;
}

static ConvEngine *SCE;

void ConvEngine_Initialize(int fft_len, int filter_len, float *filter_real)
{
  SCE=(ConvEngine*)malloc(sizeof(ConvEngine));
}
void ConvEngine_Convolve(float *input_segment, float *output) {}

void *ConvEngine_ConvThread(void *v)
{
	ConvEngine *ce = (ConvEngine*) v;
	FirBuffer *fb = ce->fir_buffer;
	while (true)
	{
//		printf("locking on in\n");
		pthread_mutex_lock(&ce->mut_in_buffer);
		pthread_cond_wait(&ce->cond_input_segment_ready, &ce->mut_in_buffer);
		if (fb->buffer_ready >0)
		{

			// perform forward convolution on input
			if(fb->buffer_ready==1)
			{
				fftwf_execute(ce->plan_input1);
			}
			else
			{
				fftwf_execute(ce->plan_input2);
			}
			ce->fir_buffer->read_buffer = NULL;
			ce->fir_buffer->buffer_ready=0;
      pthread_mutex_unlock(&ce->mut_in_buffer);
			for (int i = 0; i < ce->num_fir_filters; i++)
			{
				FirFilter *f = ce->fir_filters[i];
				FreqDomMultiply(f);
				fftwf_execute(f->plan_output);
				for (int j = 0; j < f->fft_len; j++)
				{
					f->out_r[j] = f->oo_fft_len * f->out_r[j];
					if(j<f->overlap_size) f->out_r[j] +=  f->overlap_r[j];
				}
				memcpy(f->overlap_r, &f->out_r[f->fft_len-f->overlap_size], sizeof(float) *f->overlap_size);
			}
			pthread_mutex_lock(&ce->mut_out_buffer);
			for (int i = 0; i < ce->num_fir_filters; i++)
			{
				CircBuffer_AddData(ce->fir_filters[i]->outBuffer, ce->segment_len, ce->fir_filters[i]->out_r);
			}
		}
		pthread_mutex_unlock(&ce->mut_out_buffer);
	}
}

int ConvEngine_Destroy(ConvEngine *ce)
{

	return 0;
}
