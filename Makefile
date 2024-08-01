CXX = g++
CXXFLAGS = -I./include -DUSE_WEBSOCKET
LDFLAGS = -lpthread -ldl -lrt -liio -lfftw3f -lm
LIBS = lib/libcivetweb.a

TARGET = spectra_sieve
SRCS = $(wildcard src/*.cpp)
OBJS = $(patsubst src/%.cpp,build/%.o,$(SRCS))

all: build $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

build:
	mkdir -p build

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
	rm -rf build

.PHONY: all clean