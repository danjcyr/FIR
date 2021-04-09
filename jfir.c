#include "fir1.h"

#include <jack/jack.h>

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;

void ProcessConfig(char *file_name,ConvEngine **ce)
{
	printf("nothing to see here\n");
}
int
processbq2 (jack_nframes_t nframes, void *arg)
{
	ConvEngine *ce = (ConvEngine *)arg;

	jack_default_audio_sample_t *in, *out;
	in = jack_port_get_buffer (input_port, nframes);
	ConvEngine_AddData(ce,nframes,(float*)in);
	out = jack_port_get_buffer (output_port, nframes);
	ConvEngine_GetData(ce,nframes,&out);

//	CircBuffer_GetData(ce->fir_filters[0]->outBuffer,nframes,(float*)out);
  return 0;
}

 int filter_len = 16384;
 int fft_len = 32768;

int
main(int argc, char **argv)
{
	char *client_name;
	char *imp_file_name;
	int sample_rate;

	ConvEngine *ce=NULL;

	if(!(argc==6 ||  argc==3))
	{
		printf("Usage : $ %s <jack_client_name> <impulse-file> <sample_rate> <filter_len> <fft_len>\n",argv[0] );
		printf("Usage : $ %s <jack_client_name> <config_file>\n",argv[0] );
		exit(0);
	}

  if(argc==3)
	{

		ProcessConfig(argv[2], &ce);
	}
	else
	{
	client_name=argv[1];
	imp_file_name=argv[2];
	sample_rate = atol(argv[3]);
	filter_len = atol(argv[4]);
	fft_len = atol(argv[5]);

	printf(" Initializing Engine %s with impulse %s, SR=%d, filter_len = %d, fft_len=%d\n",client_name, imp_file_name, sample_rate, filter_len,fft_len);
//  const char **ports;
//const char *client_name = "FIR1";
const char *server_name = NULL;
jack_options_t options = JackNullOption;
jack_status_t status;

ConvEngine_Init(&ce, filter_len,fft_len,1,sample_rate,filter_len/2);


//FILE *fpf = fopen("LR4300.txt","r");
FILE *fpf = fopen(imp_file_name,"r");
//FILE *fpf = fopen("Dirac.txt","r");
float* filter_buffer;
filter_buffer = fftwf_alloc_real(filter_len);
for (int i = 0; i < filter_len; i++) fscanf(fpf,"%f\n",&filter_buffer[i]);
fclose(fpf);
printf("read in filter  - len = %d\n" , filter_len);
FirFilter_LoadFilter(ce->fir_filters[0],filter_len,filter_buffer);
//FILE *fpf2 = fopen("sent","w");
//for (int i = 0; i < filter_len; i++) fprintf(fpf2,"%f\n",filter_buffer[i]);



printf("loaded filter\n");
//FirFilter_DiracFilter(ce->fir_filters[0]);


/* open a client connection to the JACK server */
//client = jack_client_open (client_name, options, &status, server_name);
client = jack_client_open (argv[1], options, &status, server_name);
jack_set_process_callback (client, processbq2, ce);
input_port = jack_port_register (client, "input",
         JACK_DEFAULT_AUDIO_TYPE,
         JackPortIsInput, 0);
output_port = jack_port_register (client, "output",
          JACK_DEFAULT_AUDIO_TYPE,
          JackPortIsOutput, 0);
printf("act\n");
  jack_activate (client);
}

  sleep (-1);
return 0;
}
