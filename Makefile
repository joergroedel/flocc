OBJS=$(patsubst %.cc, %.o, $(wildcard *.cc))
DEPS=$(patsubst %.cc, %.d, $(wildcard *.cc))
CXX=g++
CXXFLAGS=-Wall -O3 -std=c++11 -flto
TARGET=flocc
MANPAGE=$(TARGET).1
INSTALL_DIR ?= /usr/local/
BIN_DIR     ?= $(INSTALL_DIR)/bin/
MAN_DIR     ?= $(INSTALL_DIR)/man/
RELEASE=0.1

all: $(DEPS) $(TARGET) $(MANPAGE)

version.h: Makefile
	./gen-version-h.sh $(RELEASE)

-include $(DEPS)

$(TARGET): $(OBJS)
	$(CXX) -flto -o $@ $+ -lstdc++fs -lgit2

%.d: %.cc version.h
	g++ -MM -c $(CXXFLAGS) $< > $@

$(MANPAGE): flocc.pod
	pod2man -c "Development Tools" -n $(TARGET) -r "$(TARGET) version $(RELEASE)" $+ > $@

install: $(TARGET)
	install -m 755 $(TARGET) $(BIN_DIR)
	install -m 644 $(MANPAGE) $(MAN_DIR)/man1/

userinstall:
	install -m 755 $(TARGET) ~/bin/

clean:
	rm -f $(TARGET) $(OBJS) $(MANPAGE) $(DEPS)

