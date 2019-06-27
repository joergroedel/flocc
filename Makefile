CXX=g++
CXXFLAGS=-Wall -O3 -std=c++11 -flto
TARGET=floc

floc: floc.o md4.o counters.o filetree.o classifier.o
	$(CXX) -flto -o $@ $+ -lstdc++fs -lgit2

clean:
	rm floc floc.o md4.o counters.o filetree.o classifier.o
