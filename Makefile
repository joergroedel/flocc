CXX=g++
CXXFLAGS=-Wall -O3 -std=c++11 -flto
TARGET=flocc
MANPAGE=$(TARGET).1
OBJS=flocc.o md4.o counters.o filetree.o classifier.o
RELEASE=0.0

all: $(TARGET) $(MANPAGE)

version.h:
	./gen-version-h.sh $(RELEASE)

$(TARGET): $(OBJS) version.h
	$(CXX) -flto -o $@ $+ -lstdc++fs -lgit2

$(MANPAGE): flocc.pod
	pod2man -c "Development Tools" -n $(TARGET) -r "$(TARGET) version $(RELEASE)" $+ > $@

install: $(TARGET)
	cp $(TARGET) ~/bin/

clean:
	rm -f $(TARGET) $(OBJS) $(MANPAGE)

.PHONY: version.h clean
