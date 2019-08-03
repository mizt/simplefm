#include <emscripten.h>
#include <math.h>


#define kSineSize 8192
#define TWO_PI 3.14159265358979323846*2.0

double SAMPLE_RATE = 48000.0;

const long kSineMask = kSineSize-1;
const double kSinePhaseScale = kSineSize/(TWO_PI);

const float cpstoinc = kSineSize * (1./SAMPLE_RATE) * 65536;
const unsigned int lomask = (kSineSize-1)<<3;

float gSine[kSineSize+1];
float gSineWavetable[2*kSineSize];
	
void SignalAsWavetable(float *signal, float *wavetable, long inSize) {
	
	float val1, val2;
	
	float *in = signal;
	float *out = wavetable-1;
	for (int i = 0; i<inSize-1; i++) {
		val1 = in[i];
		val2 = in[i+1];
		*++out = 2.f*val1-val2;
		*++out = val2-val1;
	}
	val1 = in[inSize-1];
	val2 = in[0];
	*++out = 2.f*val1-val2;
	*++out = val2-val1;
}

inline float PhaseFrac1(unsigned int phase) {
	union {
		unsigned int itemp;
		float ftemp;
	} u;
	u.itemp = 0x3F800000 | (0x007FFF80 & ((phase)<<7));
	return u.ftemp;
}

inline float lookupi1(float *table0,float *table1, unsigned int phase, unsigned int lomask) {
	float pfrac = PhaseFrac1(phase);
	unsigned int index = ((phase>>13) & (unsigned int)lomask);
	float val1 = *(const float*)((const char*)table0 + index);
	float val2 = *(const float*)((const char*)table1 + index);
	return val1 + val2 * pfrac;
}

void initialize() {
	
	double sineIndexToPhase = (TWO_PI)/(double)kSineSize;
	
	for (int i=0; i<=kSineSize; ++i) {
		double phase = i*sineIndexToPhase;
		float d = sin(phase);
		gSine[i] = d;
	}
		
	SignalAsWavetable(gSine,gSineWavetable,kSineSize);	
}

// https://github.com/supercollider/supercollider/blob/f806ace7bd8565dd174e7d47a1b32aaa4175a46e/server/plugins/OscUGens.cpp
class SinOsc {
	
	private:
		
		int phaseinc = 0;
		unsigned int phase = 0;
		
	public:
	
		SinOsc(float freq) {
			this->set(freq);
		}
		
		void set(float freq) {
			//this->phaseinc = (int)(cpstoinc*freq);
			this->phaseinc = fabsf(cpstoinc*freq)<2147483648.0f?(int)(cpstoinc*freq):-2147483648;
		}
	
		float next() {
			float wav = lookupi1(gSineWavetable,gSineWavetable+1,this->phase,lomask);
			this->phase+=this->phaseinc;
			return wav;
		}
};


namespace Function {
	
	typedef struct {
		float *points;
		unsigned long length;	
	} Data;

}

namespace Line {

	class Object {
	
		private:
		
			bool isFunction = false;
			bool isUpdate = false;
			
			bool isRouteList = false;
			
			// cache
			float size = 0;
			float points[512*3]; // xyz
		
			float x[512], y[512], z[512];
			float y0;
		
			long   seek = 0;
			float head = 0;
			
			float first, last;
			
			int length = 0;
			
			float p0, p1;
			float q0, q1;
			
			float weight = 0.0;

			float currentValue = 0;
			float defaultValue = 0;
		
			void reset() {
				this->seek = 0;
				this->head = 0;
			}
		
			void update() { this->head+=1000.0/SAMPLE_RATE; }
		
		public:
				
			void bang() {
									
				if(this->isFunction==false) return;
				
				this->reset();
				
				this->length = this->size/3;
									
				if(this->isUpdate==true) {		
	
					for(int k=0; k<this->length; k++) {
						this->x[k] = this->points[k*3+0];
						this->y[k] = this->points[k*3+1];
						this->z[k] = this->points[k*3+2];
					}
			
					if(!isRouteList) {
						this->currentValue = this->y0 = this->y[0];
					}
					
					this->isUpdate = false;
				}
				
				//NSLog(@"%f",this->y0);
							
				this->first = this->y[0] = (!isRouteList)?this->y0:this->currentValue;
				this->last  = this->y[this->length-1];
			
			}
		
			void set(float *f1,float f2) {
								
				if(f2>=512*3) f2 = 512*3;
				this->size = f2;
										
				for(int k=0; k<this->size; k++) this->points[k] = f1[k];	
				
				this->isFunction = true;
				this->isUpdate=true;
			}
			
			float next() {
				
				if(this->length>0) {
					
					for(long k=this->seek; k<this->length; k++) {
						
						if(this->head<=this->x[k]) {
							
							this->seek = k;
								
							q0 = this->x[(this->seek)];
							q1 = this->y[(this->seek)];
								
							if(this->seek!=0&&(this->x[(this->seek-1)]!=this->x[(this->seek)])) {
																			
								p0 = this->x[(this->seek-1)];
								p1 = this->y[(this->seek-1)];
									
								this->weight = 1.0/(q0-p0);
									
							}
							else {
									
								p0 = this->x[(this->seek)];
								p1 = this->y[(this->seek)];
									
								this->weight = 1.0;
									
							}
								
							if(((this->seek+1)<this->length)&&(this->x[(this->seek)]!=this->x[(this->seek+1)])) break;
						}
						else if(k==this->length-1) {
							this->seek++;
						}
					}
					
					if(this->seek==0) {
						this->update();
						this->currentValue = this->first;
						return this->currentValue;
					}
					else if(this->seek>=this->length) {
						this->update();
						this->currentValue = this->last;
						return this->currentValue;
					}
					else {
						float wet = (this->head-p0)*this->weight;
						float dry = 1-wet;
						this->update();
						this->currentValue = this->p1*dry+this->q1*wet;
						return this->currentValue;
					}
				}
				
				return this->defaultValue;
			}
			
			Object(float *data,unsigned long length,float value=0.) {
				
				this->isFunction = true;
				
				if(length>512*3) length = 512*3;
				this->size = length;
				
				for(int k=0; k<this->size; k++) this->points[k] = data[k];
				
				unsigned long len = this->size/3;
				
				for(int k=0; k<len; k++) {
					this->x[k] = this->points[k*3+0];
					this->y[k] = this->points[k*3+1];
					this->z[k] = this->points[k*3+2];
				}
			
				this->currentValue = this->y0 = this->y[0];
				this->defaultValue = value;
			}
		
			Object(float value=0.) {
				
				this->isFunction = false;

				this->x[0] = this->x[1] = 0;
				this->y[0] = this->y[1] = value;
				
				this->reset();
				this->length = 2;
				this->defaultValue = this->first = this->last = this->currentValue = value;
				
			}
	};
};

SinOsc *sinOsc[2] = {nullptr,nullptr};
Line::Object *mod = new Line::Object();
Line::Object *amp = new Line::Object();
float sig[2] = {0,0};

#include "stdio.h"


extern "C" {
	
	void setup() {
		
		
		initialize();
		sinOsc[0] = new SinOsc(0);
		sinOsc[1] = new SinOsc(0);
	}
	
	void calc(float *L, float *R,int len) {
		if(sinOsc[0]&&sinOsc[1]) {
			
			double freq = sig[0]*sig[1];
			
			for(int k=0; k<len; k++) {						
				sinOsc[0]->set(freq);
				sinOsc[1]->set(sig[0]+sinOsc[0]->next()*freq*mod->next());
				R[k] = L[k] = sinOsc[1]->next()*amp->next();
			}	
		}
		else {
			for(int k=0; k<len; k++) {
				R[k] = L[k] = 0;
			}
		}
	}
	
	void bang(float carrier,float harmonicity, float *pMod,int modLen,float *pAmp,int ampLen) {
		
		sig[0] = carrier;
		sig[1] = harmonicity;
		
		mod->set(pMod,modLen);
		mod->bang();

		amp->set(pAmp,ampLen);
		amp->bang();
		
				
	}
}