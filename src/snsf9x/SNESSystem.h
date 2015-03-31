
#pragma once

#include <stdint.h>

// Callback class, passed the audio data from the emulator
struct SNESSoundOut
{
	virtual ~SNESSoundOut() { }
	// Receives signed 16-bit stereo audio and a byte count
	virtual void write(const void * samples, unsigned long bytes) = 0;
};

class SNESSystem
{
public:
	SNESSystem();
	virtual ~SNESSystem();

	bool Load(const uint8_t * rom, uint32_t romsize, const uint8_t * sram, uint32_t sramsize);
	void SoundInit(SNESSoundOut * output);
	void Init();
	void Reset();
	void Term();

	void CPULoop();

	bool IsHiROM() const;
	uint32_t GetROMCoverageSize() const;

	uint8_t * rom;
	uint32_t rom_size;
	uint8_t * rom_refs;
	uint32_t * rom_refs_histogram;

protected:
	SNESSoundOut * m_output;

private:
	uint8_t * sound_buffer;
};
