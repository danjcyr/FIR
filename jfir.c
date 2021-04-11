#include "fir1.h"

#include <jack/jack.h>
#include <libconfig.h>

//jack_port_t *input_port;
//jack_port_t *output_port;
//jack_client_t *client;
const char *server_name = NULL;
jack_options_t options = JackNullOption;
jack_status_t status;


int
process_jack_callback(jack_nframes_t nframes, void *arg)
{
	ConvEngine *ce = (ConvEngine *)arg;

	jack_default_audio_sample_t *in;
	in = jack_port_get_buffer (ce->input_port, nframes);

	ConvEngine_AddData(ce,nframes,(float*)in);
  jack_default_audio_sample_t *o;
	for(int i=0;i<ce->num_fir_filters;i++)
	{
		o = (jack_default_audio_sample_t *) jack_port_get_buffer (ce->output_ports[i], nframes);
		ce->output_buffers[i]=o;
	}
	ConvEngine_GetData(ce,nframes,ce->output_buffers);
	return 0;
}

int
processbq2 (jack_nframes_t nframes, void *arg)
{
	ConvEngine *ce = (ConvEngine *)arg;

	jack_default_audio_sample_t *in;
	jack_default_audio_sample_t *out;

	in = jack_port_get_buffer (ce->input_port, nframes);
	ConvEngine_AddData(ce,nframes,(float*)in);
	out = jack_port_get_buffer (ce->output_ports[0], nframes);
	ce->output_buffers[0] = out;
	ConvEngine_GetData(ce,nframes,ce->output_buffers);

//	CircBuffer_GetData(ce->fir_filters[0]->outBuffer,nframes,(float*)out);
  return 0;
}


fir_conf_t ParseArgs(char **argv)
{
	// assume argc = 6

  fir_conf_t cfg;

	cfg.client_name = argv[1];
	cfg.input_name = argv[2];
	cfg.num_outputs=3;
	cfg.sample_rate=atol(argv[3]);
	cfg.filter_len = atol(argv[4]);
	cfg.fft_len=atol(argv[5]);
	cfg.extra_samples = 4096;
  cfg.filter_file_names = (char**)malloc(sizeof(char *)*3);
	cfg.filter_file_names[0] = argv[2];
	cfg.filter_file_names[1] = "SimWoofer.txt";
	cfg.filter_file_names[2] = "SimMidrange.txt";
	cfg.output_portnames = (char**)malloc(sizeof(char *)*3);
	cfg.output_portnames[0]= "output1";
	cfg.output_portnames[1]= "output2";
	cfg.output_portnames[2]= "output3";

	return cfg;
}

fir_conf_t ProcessConfig(char *file_name)
{
	fir_conf_t conf;

	config_t cfg;
	config_setting_t *setting;

	config_init(&cfg);
	if(!config_read_file(&cfg,file_name))
	{
		fprintf(stderr,"Error reading config %s  - %s:%d -%s\n",file_name,  config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg) );
	}
	if(config_lookup_string(&cfg, "client_name", &conf.client_name))
	{
//		printf("client_name = %s\n",conf.client_name);
	}
	if(config_lookup_int(&cfg, "FFT_Length", &conf.fft_len))
	{
//		printf("FFt_len = %d\n",conf.fft_len);
	}
	if(config_lookup_int(&cfg, "filter_length", &conf.filter_len))
	{
//		printf("filter_len = %d\n",conf.filter_len);
	}
	if(config_lookup_int(&cfg, "sample_rate", &conf.sample_rate))
	{
//		printf("sample_rate = %d\n",conf.sample_rate);
	}
	if(config_lookup_string(&cfg, "input_name", &conf.input_name))
	{
//		printf("input_name = %s\n",conf.input_name);
	}
	if(config_lookup_int(&cfg, "extra_sample_buffer", &conf.extra_samples))
	{
//		printf("extra_sample_buffer = %d\n",conf.extra_samples);
	}
	setting = config_lookup(&cfg, "outputs");
	if(setting != NULL)
	  {
	    conf.num_outputs = config_setting_length(setting);
			conf.filter_file_names = (char **)malloc(sizeof(char*)*conf.num_outputs);
			conf.output_portnames = (char **)malloc(sizeof(char*)*conf.num_outputs);
	    int i;
//			printf(" %d outputs\n",conf.num_outputs);

			for(i=0;i<conf.num_outputs;i++)
			{
				config_setting_t *ouput = config_setting_get_elem(setting,i);
				const char *port_name;
				const char *impulse_name;
				int number_biquads;
				if (
						(config_setting_lookup_string(ouput,"port_name",&port_name)) &&
    				(config_setting_lookup_string(ouput,"impulse_file_name",&impulse_name)) &&
						(config_setting_lookup_int(ouput,"number_biquads",&number_biquads))
					)
				{
//					printf("output #%d : %s %s %d\n",i,port_name,impulse_name,number_biquads);
					conf.output_portnames[i]=(char *)malloc(sizeof(char *)*strlen(impulse_name));
					strcpy(conf.output_portnames[i],port_name);

					conf.filter_file_names[i] = (char *)malloc(sizeof(char *)*strlen(impulse_name));
					strcpy(conf.filter_file_names[i],impulse_name);
				}
			} //end for i

		}// end if setting

return conf;

}

void DumpConf(fir_conf_t c)
{
	printf("\n\nConfiguration : \n");
	printf("client = %s   input = %s  NumOutput = %d SR = %d filter_len = %d fft_len = %d extra = %d\n",
	c.client_name,c.input_name,c.num_outputs,c.sample_rate,c.filter_len,c.fft_len,c.extra_samples);
  for(int i=0;i<c.num_outputs;i++)
	{
		printf("i=%d  filterfile = %s  port = %s\n",i,c.filter_file_names[i],c.output_portnames[i]);
	}
printf("\n\n");
}
void SetupConvEngine(fir_conf_t cfg, ConvEngine **pce)
{
	DumpConf(cfg);
	ConvEngine_Init(pce,cfg);
	ConvEngine *ce = (*pce);

//	printf("ce client name = %s\n",ce->client_name);
//	printf("ce input name = %s\n",ce->input_name);
	ce->client = jack_client_open (ce->client_name, options, &status, server_name);
	jack_set_process_callback (ce->client, process_jack_callback, ce);
	ce->input_port = jack_port_register (ce->client, ce->input_name,
	         JACK_DEFAULT_AUDIO_TYPE,
	         JackPortIsInput, 0);

//  printf("made client reg input port\n");

	float* filter_buffer;
	filter_buffer = fftwf_alloc_real(cfg.filter_len);
//	printf("cfg fitler len = %d\n",cfg.filter_len);
	for(int i=0;i<cfg.num_outputs;i++)
	{
//		printf("filter name = %s\n",cfg.filter_file_names[i]);
		FILE *fpf = fopen(cfg.filter_file_names[i],"r");

		for (int j = 0; j < cfg.filter_len; j++)
		{
			double v;
		 	fscanf(fpf,"%lf\n",&v);
			filter_buffer[j]=(float)v;
	  }
		fclose(fpf);
//		printf("read in filter  - len = %d\n" , cfg.filter_len);
		FirFilter_LoadFilter(ce->fir_filters[i],cfg.filter_len,filter_buffer);

		ce->output_ports[i] = jack_port_register (ce->client, ce->output_port_names[i],
		          JACK_DEFAULT_AUDIO_TYPE,
		          JackPortIsOutput, 0);

		printf("Setup output %d : portname = %s\n",i,ce->output_port_names[i]);
	}
	fftwf_free(filter_buffer);
	/* open a client connection to the JACK server */
	printf("Ready to start client\n\n");
}


int
main(int argc, char **argv)
{

	ConvEngine *ce=NULL;

	if(!(argc==6 ||  argc==2))
	{
		printf("Usage : $ %s <jack_client_name> <impulse-file> <sample_rate> <filter_len> <fft_len>\n",argv[0] );
		printf("Usage : $ %s <config_file>\n",argv[0] );
		exit(0);
	}


  fir_conf_t cfg;
  if(argc==2)
	{
		cfg=ProcessConfig(argv[1]);
	}
	else
	{
	  cfg = ParseArgs(argv);
	}

  SetupConvEngine(cfg,&ce);
//	ConvEngine_Dump(ce);
	printf("Activating Jack Client \n");
	jack_activate (ce->client);

  sleep (-1);
	return 0;
}
