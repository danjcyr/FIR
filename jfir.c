#include "fir1.h"

#include <jack/jack.h>

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;

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

int
main()
{
//  const char **ports;
const char *client_name = "FIR1";
const char *server_name = NULL;
jack_options_t options = JackNullOption;
jack_status_t status;

ConvEngine *ce=NULL;
ConvEngine_Init(&ce, 16384,32768,1,48000,1024*8);
FirFilter_DiracFilter(ce->fir_filters[0]);
/* open a client connection to the JACK server */
client = jack_client_open (client_name, options, &status, server_name);
jack_set_process_callback (client, processbq2, ce);
input_port = jack_port_register (client, "input",
         JACK_DEFAULT_AUDIO_TYPE,
         JackPortIsInput, 0);
output_port = jack_port_register (client, "output",
          JACK_DEFAULT_AUDIO_TYPE,
          JackPortIsOutput, 0);
printf("act\n");
  jack_activate (client);

  sleep (-1);
return 0;
}
