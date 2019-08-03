### build

	em++ \
		-O3 \
		-std=c++11 \
		-Wc++11-extensions \
		-s VERBOSE=1 \
		--memory-init-file 0 \
		-s WASM=0 \
	 	-s EXPORTED_FUNCTIONS="['_setup','_calc','_bang']" \
		-s EXTRA_EXPORTED_RUNTIME_METHODS="['cwrap']" \
		-s TOTAL_MEMORY=16777216 \
		-s PRECISE_F32=1. \
		./sc.cpp \
	 	-o ./sc.js

### License

This branch use a part of SuperCollider's SinOsc code.  
SuperCollider is free software available under Version 3 the GNU General Public License. See [COPYING](https://github.com/supercollider/supercollider/blob/develop/COPYING) for details.