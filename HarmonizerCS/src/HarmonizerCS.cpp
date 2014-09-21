#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include <complex>
#include "PitchDetection.h"
#include "shift.h"
#include "window.h"
#include "angle.h"
#include <fftw3.h>

/**********************************************************************************************************************************************************/

#define PLUGIN_URI "http://portalmod.com/plugins/mod-devel/HarmonizerCS"
#define TAMANHO_DO_BUFFER 64
enum {IN, OUT_1, OUT_2, TONE, STEP_0, STEP_1, STEP_2, STEP_3, STEP_4, STEP_5, STEP_6, STEP_7, STEP_8, STEP_9, STEP_10, STEP_11, LOWNOTE, GAIN_1, GAIN_2, PLUGIN_PORT_COUNT};

/**********************************************************************************************************************************************************/

class PitchShifter
{
public:
    PitchShifter() {}
    ~PitchShifter() {}
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void activate(LV2_Handle instance);
    static void deactivate(LV2_Handle instance);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
    static void run(LV2_Handle instance, uint32_t n_samples);
    static void cleanup(LV2_Handle instance);
    static const void* extension_data(const char* uri);
    //Ports
    float *in;
    float *out_1;
    float *out_2;
    float *tone;
    float *step_0;
    float *step_1;
    float *step_2;
    float *step_3;
    float *step_4;
    float *step_5;
    float *step_6;
    float *step_7;
    float *step_8;
    float *step_9;
    float *step_10;
    float *step_11;
    float *lownote;
    float *gain_1;
    float *gain_2;
    
    int nBuffers;
    int nBuffers2;
    int Qcolumn;
    int hopa;
    int N;
    int N2;
    int cont;
    
    double s;
    float g1;
    float g2;
    
    int *Hops;
    double *frames;
    double *ysaida;
    double *ysaida2;
    double *yshift;
    double **b;
    
	float *frames2;
	float *q;
    fftwf_complex *fXa;
    fftwf_complex *fXs;
    
    cx_vec Xa;
    cx_vec Xs;
    cx_vec XaPrevious;
    vec Xa_arg;
    vec XaPrevious_arg;
    vec Phi;
    vec PhiPrevious;
    vec d_phi;
    vec d_phi_prime;
    vec d_phi_wrapped;
    vec omega_true_sobre_fs;
    vec AUX;
    vec Xa_abs;
    vec I;
    vec w;
    
    float *frames3;
    float *q2;
    fftwf_complex *fXa2;
    fftwf_complex *fXs2;
    cx_vec Xa2;
    cx_vec Xs2;
        
    vec R;
	vec NORM;
	vec F;
	vec AUTO;
	
	int note;
	int oitava;
	
	double SampleRate;
        
    fftwf_plan p;
	fftwf_plan p2;
	fftwf_plan p3;
	fftwf_plan p4;
};

/**********************************************************************************************************************************************************/

static const LV2_Descriptor Descriptor = {
    PLUGIN_URI,
    PitchShifter::instantiate,
    PitchShifter::connect_port,
    PitchShifter::activate,
    PitchShifter::run,
    PitchShifter::deactivate,
    PitchShifter::cleanup,
    PitchShifter::extension_data
};

/**********************************************************************************************************************************************************/

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;
}

/**********************************************************************************************************************************************************/

LV2_Handle PitchShifter::instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features)
{
    PitchShifter *plugin = new PitchShifter();
    
    plugin->SampleRate = samplerate;
        
    plugin->nBuffers = 32;
    plugin->nBuffers2 = 16;
    plugin->Qcolumn = 1*plugin->nBuffers;
    plugin->hopa = TAMANHO_DO_BUFFER;
    plugin->N = plugin->nBuffers*plugin->hopa;
    plugin->N2 = plugin->nBuffers2*plugin->hopa;
    plugin->cont = 0;
    
    plugin->s = 0;
    plugin->g1 = 1;
    plugin->g2 = 1;  
    
    plugin->Hops = (int*)calloc(plugin->Qcolumn,sizeof(int));
    for (int i=0; i<plugin->Qcolumn; i++)
		plugin->Hops[i] = plugin->hopa;
        
    plugin->frames = (double*)calloc(plugin->N,sizeof(double));
    plugin->ysaida = (double*)calloc(2*(plugin->N + 2*(plugin->Qcolumn-1)*plugin->hopa),sizeof(double));
    plugin->yshift = (double*)calloc(plugin->hopa,sizeof(double));
    plugin->b = (double**)calloc(plugin->hopa,sizeof(double*));
    
    plugin->frames2 = fftwf_alloc_real(plugin->N);
    plugin->q = fftwf_alloc_real(plugin->N);
    plugin->fXa = fftwf_alloc_complex(plugin->N/2 + 1);
	plugin->fXs = fftwf_alloc_complex(plugin->N/2 + 1);
	
    
    plugin->Xa.zeros(plugin->N/2 + 1); 
	plugin->Xs.zeros(plugin->N/2 + 1);
	plugin->Xa2.zeros(plugin->N + 1);
	plugin->Xs2.zeros(plugin->N + 1);
	plugin->XaPrevious.zeros(plugin->N/2 + 1);
	plugin->Xa_arg.zeros(plugin->N/2 + 1);
	plugin->XaPrevious_arg.zeros(plugin->N/2 + 1);
	plugin->Phi.zeros(plugin->N/2 + 1);
	plugin->PhiPrevious.zeros(plugin->N/2 + 1);
    plugin->d_phi.zeros(plugin->N/2 + 1);
	plugin->d_phi_prime.zeros(plugin->N/2 + 1);
	plugin->d_phi_wrapped.zeros(plugin->N/2 + 1);
	plugin->omega_true_sobre_fs.zeros(plugin->N/2 + 1);
	plugin->AUX.zeros(plugin->N/2 + 1);
	plugin->Xa_abs.zeros(plugin->N/2 + 1);
	plugin->w.zeros(plugin->N); hann(plugin->N,&plugin->w);
	plugin->I.zeros(plugin->N/2 + 1); plugin->I = linspace(0, plugin->N/2,plugin->N/2 + 1);
	
	plugin->frames3 = fftwf_alloc_real(2*plugin->N2); memset(plugin->frames3, 0, 2*plugin->N2 );
	plugin->q2 = fftwf_alloc_real(2*plugin->N2);	
	plugin->fXa2 = fftwf_alloc_complex(plugin->N2 + 1);
	plugin->fXs2 = fftwf_alloc_complex(plugin->N2 + 1);
	
	plugin->R.zeros(plugin->N2);
	plugin->NORM.zeros(plugin->N2);
	plugin->F.zeros(plugin->N2);
	plugin->AUTO.zeros(plugin->N2);
    
    for (int i=0; i<(plugin->nBuffers); i++)
		plugin->b[i] = &plugin->frames[i*plugin->hopa];
	
	plugin->p = fftwf_plan_dft_r2c_1d(plugin->N, plugin->frames2, plugin->fXa, FFTW_ESTIMATE);
	plugin->p2 = fftwf_plan_dft_c2r_1d(plugin->N, plugin->fXs, plugin->q, FFTW_ESTIMATE);
	plugin->p3 = fftwf_plan_dft_r2c_1d(2*plugin->N2, plugin->frames3, plugin->fXa2, FFTW_ESTIMATE);
	plugin->p4 = fftwf_plan_dft_c2r_1d(2*plugin->N2, plugin->fXs2, plugin->q2, FFTW_ESTIMATE);
	
    return (LV2_Handle)plugin;
}

/**********************************************************************************************************************************************************/

void PitchShifter::activate(LV2_Handle instance)
{
    // TODO: include the activate function code here
}

/**********************************************************************************************************************************************************/

void PitchShifter::deactivate(LV2_Handle instance)
{
    // TODO: include the deactivate function code here
}

/**********************************************************************************************************************************************************/

void PitchShifter::connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    PitchShifter *plugin;
    plugin = (PitchShifter *) instance;

    switch (port)
    {
        case IN:
            plugin->in = (float*) data;
            break;
        case OUT_1:
            plugin->out_1 = (float*) data;
            break;
        case OUT_2:
            plugin->out_2 = (float*) data;
            break;
        case TONE:
            plugin->tone = (float*) data;
            break;
        case STEP_0:
            plugin->step_0 = (float*) data;
            break;
        case STEP_1:
            plugin->step_1 = (float*) data;
            break;
        case STEP_2:
            plugin->step_2 = (float*) data;
            break;
        case STEP_3:
            plugin->step_3 = (float*) data;
            break;
        case STEP_4:
            plugin->step_4 = (float*) data;
            break;
        case STEP_5:
            plugin->step_5 = (float*) data;
            break;
        case STEP_6:
            plugin->step_6 = (float*) data;
            break;
        case STEP_7:
            plugin->step_7 = (float*) data;
            break;
        case STEP_8:
            plugin->step_8 = (float*) data;
            break;
        case STEP_9:
            plugin->step_9 = (float*) data;
            break;
        case STEP_10:
            plugin->step_10 = (float*) data;
            break;
        case STEP_11:
            plugin->step_11 = (float*) data;
            break;
        case LOWNOTE:
            plugin->lownote = (float*) data;
            break;
        case GAIN_1:
            plugin->gain_1 = (float*) data;
            break;
        case GAIN_2:
            plugin->gain_2 = (float*) data;
            break;
    }
}

/**********************************************************************************************************************************************************/

void PitchShifter::run(LV2_Handle instance, uint32_t n_samples)
{
    PitchShifter *plugin;
    plugin = (PitchShifter *) instance;
    
    if ( ((plugin->hopa) != (int)n_samples) )
    {
		
		switch ((int)n_samples)
		{
			case 64:
				plugin->nBuffers = 32;
				plugin->nBuffers2 = 16;
				break;
			case 128:
				plugin->nBuffers = 16;
				plugin->nBuffers2 = 8;
				break;
			case 256:
				plugin->nBuffers = 8;
				plugin->nBuffers2 = 4;
				break;
			case 512:
				plugin->nBuffers = 4;
				plugin->nBuffers2 = 2;
				break;
		}
		
		plugin->Qcolumn = plugin->nBuffers;
		
		plugin->hopa = n_samples;
		plugin->N = plugin->nBuffers*plugin->hopa;
		plugin->N2 = plugin->nBuffers2*plugin->hopa;
		
		plugin->Hops = (int*)realloc(plugin->Hops,plugin->Qcolumn*sizeof(int));
		for (int i=0; i<plugin->Qcolumn; i++)
			plugin->Hops[i] = plugin->hopa;
		
		plugin->frames = (double*)realloc(plugin->frames,plugin->N*sizeof(double));                                          memset(plugin->frames, 0, plugin->N*sizeof(double) );
		plugin->ysaida = (double*)realloc(plugin->ysaida,2*(plugin->N + 2*(plugin->Qcolumn-1)*plugin->hopa)*sizeof(double)); memset(plugin->ysaida, 0, 2*(plugin->N + 2*(plugin->Qcolumn-1)*plugin->hopa)*sizeof(double) );
		plugin->yshift = (double*)realloc(plugin->yshift,plugin->hopa*sizeof(double));                                       memset(plugin->yshift, 0, plugin->hopa*sizeof(double) );
		plugin->b = (double**)realloc(plugin->b,plugin->hopa*sizeof(double*));
		
		fftwf_free(plugin->frames2); plugin->frames2 = fftwf_alloc_real(plugin->N);
		fftwf_free(plugin->q);       plugin->q = fftwf_alloc_real(plugin->N);
		fftwf_free(plugin->fXa);     plugin->fXa = fftwf_alloc_complex(plugin->N/2 + 1);
		fftwf_free(plugin->fXs);     plugin->fXs = fftwf_alloc_complex(plugin->N/2 + 1);
		
		plugin->Xa.zeros(plugin->N/2 + 1); 
		plugin->Xs.zeros(plugin->N/2 + 1); 
		plugin->XaPrevious.zeros(plugin->N/2 + 1);
		plugin->Xa_arg.zeros(plugin->N/2 + 1);
		plugin->XaPrevious_arg.zeros(plugin->N/2 + 1);
		plugin->Phi.zeros(plugin->N/2 + 1);
		plugin->PhiPrevious.zeros(plugin->N/2 + 1);
		plugin->d_phi.zeros(plugin->N/2 + 1);
		plugin->d_phi_prime.zeros(plugin->N/2 + 1);
		plugin->d_phi_wrapped.zeros(plugin->N/2 + 1);
		plugin->omega_true_sobre_fs.zeros(plugin->N/2 + 1);
		plugin->AUX.zeros(plugin->N/2 + 1);
		plugin->Xa_abs.zeros(plugin->N/2 + 1);
		plugin->w.zeros(plugin->N); hann(plugin->N,&plugin->w);
		plugin->I.zeros(plugin->N/2 + 1); plugin->I = linspace(0, plugin->N/2,plugin->N/2 + 1);
		
		fftwf_free(plugin->frames3); plugin->frames3 = fftwf_alloc_real(2*plugin->N2); memset(plugin->frames3, 0, 2*plugin->N2 );
		fftwf_free(plugin->q2);      plugin->q2 = fftwf_alloc_real(2*plugin->N2);
		fftwf_free(plugin->fXa2);    plugin->fXa2 = fftwf_alloc_complex(plugin->N2 + 1);
		fftwf_free(plugin->fXs2);    plugin->fXs2 = fftwf_alloc_complex(plugin->N2 + 1);		
		
		plugin->R.zeros(plugin->N2);
		plugin->NORM.zeros(plugin->N2);
		plugin->F.zeros(plugin->N2);
		plugin->AUTO.zeros(plugin->N2);	
		
		for (int i=0; i<plugin->nBuffers; i++)
			plugin->b[i] = &plugin->frames[i*plugin->hopa];
		
		fftwf_destroy_plan(plugin->p);  plugin->p = fftwf_plan_dft_r2c_1d(plugin->N, plugin->frames2, plugin->fXa, FFTW_ESTIMATE);
		fftwf_destroy_plan(plugin->p2); plugin->p2 = fftwf_plan_dft_c2r_1d(plugin->N, plugin->fXs, plugin->q, FFTW_ESTIMATE);
		fftwf_destroy_plan(plugin->p3); plugin->p3 = fftwf_plan_dft_r2c_1d(2*plugin->N2, plugin->frames3, plugin->fXa2, FFTW_ESTIMATE);
		fftwf_destroy_plan(plugin->p4); plugin->p4 = fftwf_plan_dft_c2r_1d(2*plugin->N2, plugin->fXs2, plugin->q2, FFTW_ESTIMATE);
		
		return;
	}

    float media = 0;
    
    for (uint32_t i=0; i<n_samples; i++)
		media += abs(plugin->in[i]);
	
	if (media == 0)
	{
		for (uint32_t i=0; i<n_samples; i++)
		{
			plugin->out_1[i] = 0;
			plugin->out_2[i] = 0;
		}
	}
	else
	{
		
		int Tone = (int)(*(plugin->tone));
		int LowNote = (int)(*(plugin->lownote));

		int s_0 = (int)(*(plugin->step_0));
		int s_1 = (int)(*(plugin->step_1));
		int s_2 = (int)(*(plugin->step_2));
		int s_3 = (int)(*(plugin->step_3));
		int s_4 = (int)(*(plugin->step_4));
		int s_5 = (int)(*(plugin->step_5));
		int s_6 = (int)(*(plugin->step_6));
		int s_7 = (int)(*(plugin->step_7));
		int s_8 = (int)(*(plugin->step_8));
		int s_9 = (int)(*(plugin->step_9));
		int s_10 = (int)(*(plugin->step_10));
		int s_11 = (int)(*(plugin->step_11));
				
		double g1_before = plugin->g1;
		double g2_before = plugin->g2;
		plugin->g1 = pow(10, (float)(*(plugin->gain_1))/20.0);
		plugin->g2 = pow(10, (float)(*(plugin->gain_2))/20.0);
		
		for (int k=0; k<plugin->Qcolumn-1; k++)
			plugin->Hops[k] = plugin->Hops[k+1];
    
		for (int i=0; i<plugin->hopa; i++)
		{
			for (int j=0; j<(plugin->nBuffers-1); j++)
				plugin->b[j][i] = plugin->b[j+1][i];

			plugin->b[plugin->nBuffers-1][i] = plugin->in[i];
		}

		if ( plugin->cont < plugin->nBuffers-1)
		{
			plugin->cont++;
		}
		else
		{
			FindNote(plugin->N2, plugin->frames, plugin->frames3, &plugin->Xa2, &plugin->Xs2, plugin->q2, plugin->Qcolumn, plugin->p3, plugin->p4, plugin->fXa2, plugin->fXs2, &plugin->R, &plugin->NORM, &plugin->F, &plugin->AUTO, plugin->SampleRate, &plugin->note, &plugin->oitava );

			FindStepCS(plugin->note, plugin->oitava, Tone, s_0, s_1, s_2, s_3, s_4, s_5, s_6, s_7, s_8, s_9, s_10, s_11, LowNote, plugin->hopa, plugin->Qcolumn, &plugin->s, plugin->Hops);

			shift(plugin->N, plugin->hopa, plugin->Hops, plugin->frames, plugin->frames2, &plugin->w, &plugin->XaPrevious, &plugin->Xa_arg, &plugin->Xa_abs, &plugin->XaPrevious_arg, &plugin->PhiPrevious, plugin->yshift, &plugin->Xa, &plugin->Xs, plugin->q, &plugin->Phi, plugin->ysaida, plugin->ysaida2,  plugin->Qcolumn, &plugin->d_phi, &plugin->d_phi_prime, &plugin->d_phi_wrapped, &plugin->omega_true_sobre_fs, &plugin->I, &plugin->AUX, plugin->p, plugin->p2, plugin->fXa, plugin->fXs);
			
			for (int i=0; i<plugin->hopa; i++)
			{
				plugin->out_1[i] = (g1_before + i*((plugin->g1 - g1_before)/(plugin->hopa - 1)))*(float)plugin->frames[i];
				plugin->out_2[i] = (g2_before + i*((plugin->g2 - g2_before)/(plugin->hopa - 1)))*(float)plugin->yshift[i];
			}
		}
	}
}

/**********************************************************************************************************************************************************/

void PitchShifter::cleanup(LV2_Handle instance)
{
	PitchShifter *plugin;
	plugin = (PitchShifter *) instance;
	
	free(plugin->Hops);
	free(plugin->frames);
	free(plugin->ysaida);
	free(plugin->yshift);
	free(plugin->b);
	
	fftwf_free(plugin->frames2);
	fftwf_free(plugin->q);
	fftwf_free(plugin->fXa);
	fftwf_free(plugin->fXs);
	
	fftwf_free(plugin->frames3);
	fftwf_free(plugin->q2);
	fftwf_free(plugin->fXa2);
	fftwf_free(plugin->fXs2);
	
	plugin->Xa.clear();
	plugin->Xs.clear();
	plugin->Xa2.clear();
	plugin->Xs2.clear();
	plugin->XaPrevious.clear();
	plugin->Xa_arg.clear();
	plugin->XaPrevious_arg.clear();
	plugin->Phi.clear();
	plugin->PhiPrevious.clear();
    plugin->d_phi.clear();
	plugin->d_phi_prime.clear();
	plugin->d_phi_wrapped.clear();
	plugin->omega_true_sobre_fs.clear();
	plugin->AUX.clear();
	plugin->Xa_abs.clear();
	plugin->w.clear();
	plugin->I.clear();
	
	plugin->R.clear();
	plugin->NORM.clear();
	plugin->F.clear();
	plugin->AUTO.clear();
	
	fftwf_destroy_plan(plugin->p);
	fftwf_destroy_plan(plugin->p2);
	fftwf_destroy_plan(plugin->p3);
	fftwf_destroy_plan(plugin->p4);
	
    delete ((PitchShifter *) instance);
}

/**********************************************************************************************************************************************************/

const void* PitchShifter::extension_data(const char* uri)
{
    return NULL;
}