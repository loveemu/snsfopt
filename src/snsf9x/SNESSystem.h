
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

	const uint8_t * GetROMCoverage() const;
	uint32_t GetROMCoverageSize() const;
	const uint32_t * GetROMCoverageHistogram() const;

	const uint8_t * GetAPURAMCoverage() const;
	uint32_t GetAPURAMCoverageSize() const;
	const uint32_t * GetAPURAMCoverageHistogram() const;

	uint8_t * rom;
	uint32_t rom_size;

protected:
	SNESSoundOut * m_output;

private:
	uint8_t * sound_buffer;
};
