CXXFLAGS=-g -O2 -Wall -std=c++20 -I . -I include -I libfontlist/include $(shell pkg-config --cflags fontconfig sdl3 sdl3-image sdl3-ttf wayland-client)
LDFLAGS=-L . $(shell pkg-config --libs fontconfig sdl3 sdl3-image sdl3-ttf wayland-client)
OBJ=$(shell find -maxdepth 1 -name "*.cc" | sed -e 's/\.cc$$/.o/g') $(shell find libfontlist/src -name "*.cpp" | sed -e 's/\.cpp$$/.o/g') $(shell find -name "*.c" | sed -e 's/\.c$$/.o/g')
TARGET=ai_builtin.exe

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJ)
