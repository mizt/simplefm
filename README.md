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
		./msp.cpp \
	 	-o ./msp.js
