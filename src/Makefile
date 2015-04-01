#!/bin/make
CC = gcc
CFLAGS = -O2 -DHAVE_STDINT_H -DSNSFOPT -I ./snsf9x/snes9x
CXX = g++
CXXFLAGS = -O2 -DHAVE_STDINT_H -DSNSFOPT -I ./snsf9x/snes9x
LDFLAGS = -lm -lz
TARGET = snsfopt
SRCS = $(TARGET).cpp PSFFile.cpp ZlibReader.cpp ZlibWriter.cpp snsf9x/SNESSystem.cpp snsf9x/snes9x/cpu.cpp snsf9x/snes9x/cpuexec.cpp snsf9x/snes9x/cpuops.cpp snsf9x/snes9x/dma.cpp snsf9x/snes9x/globals.cpp snsf9x/snes9x/memmap.cpp snsf9x/snes9x/ppu.cpp snsf9x/snes9x/sdd1.cpp snsf9x/snes9x/apu/apu.cpp snsf9x/snes9x/apu/SNES_SPC.cpp snsf9x/snes9x/apu/SNES_SPC_misc.cpp snsf9x/snes9x/apu/SNES_SPC_state.cpp snsf9x/snes9x/apu/SPC_DSP.cpp snsf9x/snes9x/apu/SPC_Filter.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean
.SUFFIXES: .c .cpp .o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $^
.cpp.o:
	$(CXX) $(CXXFLAGS) -o $@ -c $^
