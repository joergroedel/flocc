OBJS=$(patsubst %.cc, %.o, $(wildcard *.cc))
DEPS=$(patsubst %.cc, %.d, $(wildcard *.cc))
CXX=g++
CXXFLAGS=-Wall -O3 -std=c++11 -flto
TARGET=flocc
MANPAGE=$(TARGET).1
RELEASE=0.0

all: $(TARGET) $(MANPAGE)

-include $(DEPS)

version.h:
	./gen-version-h.sh $(RELEASE)

$(TARGET): $(OBJS) version.h
	$(CXX) -flto -o $@ $+ -lstdc++fs -lgit2

%.d: %.cc
	g++ -MM -c $(CXXFLAGS) $< > $@

$(MANPAGE): flocc.pod
	pod2man -c "Development Tools" -n $(TARGET) -r "$(TARGET) version $(RELEASE)" $+ > $@

install: $(TARGET)
	cp $(TARGET) ~/bin/

clean:
	rm -f $(TARGET) $(OBJS) $(MANPAGE) $(DEPS)

.PHONY: version.h clean
