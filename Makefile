CXX=g++-6
CXXFLAGS=-Wall -O3 -std=c++11
TARGET=floc

floc: floc.o md4.o
	$(CXX) -o $@ $+ -lstdc++fs -lgit2

clean:
	rm floc floc.o md4.o
