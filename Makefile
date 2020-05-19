LD_FLAGS+=  -L$(V8_PATH)/out.gn/x64.release.sample/obj/ -lv8_monolith -lpthread -lstdc++fs
CXX_FLAGS+= -std=c++17 -fvisibility=hidden -fPIC -O0 -g -isystem $(V8_PATH)/include -DV8_COMPRESS_POINTERS

MODULE_OBJS = js.o module.o

modjs.so: $(MODULE_OBJS) | check-env 
	$(CXX) -shared -o $@ $^ $(LD_FLAGS)

check-env:
ifndef V8_PATH
	$(error V8_PATH is undefined)
endif

.PHONY: check-env

%.o: %.cpp
	$(CXX) -c $(CXX_FLAGS) -o $@ $<

clean:
	rm -f userland.js
	rm -f *.o
	rm -f *.so
