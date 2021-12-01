CXX=g++
CXXFLAGS=-Wall -O3 -std=c++11 -flto
TARGET=flocc
OBJS=flocc.o md4.o counters.o filetree.o classifier.o

$(TARGET): $(OBJS)
	$(CXX) -flto -o $@ $+ -lstdc++fs -lgit2

install: $(TARGET)
	cp $(TARGET) ~/bin/

clean:
	rm -f $(TARGET) $(OBJS)
