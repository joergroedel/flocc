CXX=g++-6
CXXFLAGS=-O3 -std=c++11
TARGET=floc

floc: floc.o
	$(CXX) -o $@ $< -lstdc++fs

clean:
	rm floc floc.o
