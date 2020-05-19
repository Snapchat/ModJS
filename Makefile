LD_FLAGS+=  -L/home/john/repos/v8/v8/out.gn/x64.release.sample/obj/ -lv8_monolith -lpthread -lstdc++fs
CXX_FLAGS+= -std=c++17 -fvisibility=hidden -fPIC -O2 -g -isystem /home/john/repos/v8/v8/include -DV8_COMPRESS_POINTERS

MODULE_OBJS = js.o module.o

modjs.so: $(MODULE_OBJS)
	$(CXX) -shared -o $@ $^ $(LD_FLAGS)

%.o: %.cpp
	$(CXX) -c $(CXX_FLAGS) -o $@ $<

clean:
	rm -f userland.js
	rm -f *.o
	rm -f *.so
