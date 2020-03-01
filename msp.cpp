#include <emscripten.h>
#include <math.h>

#define TWO_PI 3.14159265358979323846*2.0

double SAMPLE_RATE = 44100.0;

void initialize() {
	

}

// https://github.com/supercollider/supercollider/blob/f806ace7bd8565dd174e7d47a1b32aaa4175a46e/server/plugins/OscUGens.cpp
class SinOsc {
	
	private:
		
		double phaseinc = 0;
		double phase = 0;
		
	public:
	
		SinOsc(double freq) {
			this->set(freq);
		}
		
		void set(double freq) {
			//this->phaseinc = (int)(cpstoinc*freq);
			this->phaseinc =  2.0*M_PI*(freq/SAMPLE_RATE);
		}
	
		float next() {
			this->phase+=this->phaseinc;
			return sin(this->phase);
		}
};

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