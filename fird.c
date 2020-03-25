#include "fir1.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


const int frame_size=256;
const int sample_rate=48000;

const double frame_time_usec = 1E6*frame_size/(double)sample_rate;


FILE *fp1;
FILE *fp2;
clock_t time0;
clock_t time1;
ConvEngine *CE;
int
main(int argc, char **argv)
{

    fp1=fopen("signal_in.txt","w");
    fp2=fopen("signal_out.txt","w");

    // argv[1]=filterlen argv[2] = fft_len
    int filter_len = 64;
    int fft_len = 128;
    if(argc==3)
    {
        filter_len = atol(argv[1]);
        fft_len = atol(argv[2]);
        if(fft_len < (filter_len+8))
        {
            printf("ERROR FILTER TOO SMALL COMPARED TO FFT LENGTH\n");
            exit(-1);
        }
    }

      ConvEngine_Init(&CE,filter_len,fft_len,1,48000,8*1024);


    time1 = clock()-time0;
    double sec = ((double) time1) /CLOCKS_PER_SEC;
    printf("DONE init FIR %lf secs\n",sec);

//    LoadFilter(myfir,"impulse.txt");
    FirFilter_DiracFilter(CE->fir_filters[0]);
    int seg_len = 1024;
    float *inbuff=(float *)malloc(sizeof(float)*seg_len);
    float *outbuff=(float *)malloc(sizeof(float)*seg_len);


double t=0;
double dt = 1.0/48000.0;
    for(int i=0;i<2010;i++)
    {
      for(int j=0;j<seg_len;j++)
      {
//        inbuff[j]=1.0*sin(30*t);
      inbuff[j]=0.01*sin(.3*t);
      t+=dt;
      }
      if(i%113==10)inbuff[seg_len/2]=1.0;
      for(int k=0;k<seg_len;k++)
      {
        fprintf(fp1,"%f\n",inbuff[k]);
      }
      ConvEngine_AddData(CE,seg_len,inbuff);
      ConvEngine_GetData(CE,seg_len,&outbuff);
      usleep(400);
      for(int k=0;k<seg_len;k++)
      {
        fprintf(fp2,"%f\n",outbuff[k]);
      }

    }


}
