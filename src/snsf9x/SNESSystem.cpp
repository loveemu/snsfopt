#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#include <zlib.h>

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"

#include "SNESSystem.h"

void WinSetDefaultValues ()
{
	// TODO: delete the parts that are already covered by the default values in WinRegisterConfigItems

	// ROM Options
	memset (&Settings, 0, sizeof (Settings));

	// Tracing options
	Settings.TraceDMA =	false;
	Settings.TraceHDMA = false;
	Settings.TraceVRAM = false;
	Settings.TraceUnknownRegisters = false;
	Settings.TraceDSP =	false;

	// ROM timing options (see also	H_Max above)
	Settings.PAL = false;
	Settings.FrameTimePAL =	20000;
	Settings.FrameTimeNTSC = 16667;
	Settings.FrameTime = 16667;

	// CPU options
	Settings.Paused	= false;

	Settings.TakeScreenshot=false;

}

static void InitSnes9X()
{
	WinSetDefaultValues ();

	// Sound options
	Settings.SoundSync = TRUE;
	Settings.Mute =	FALSE;
	Settings.SoundPlaybackRate = 32000;
	Settings.SixteenBitSound = TRUE;
	Settings.Stereo	= TRUE;
	Settings.ReverseStereo = FALSE;

    Memory.Init();

    S9xInitAPU();
	S9xInitSound(10, 0);
}

void S9xMessage(int type, int message_no, const char *str)
{
#ifdef WIN32
	OutputDebugStringA(str);
#endif
}

bool8 S9xOpenSoundDevice(void)
{
	return TRUE;
}

SNESSystem::SNESSystem() :
	rom(NULL),
	rom_size(0),
	rom_refs(NULL),
	rom_refs_histogram(NULL),
	m_output(NULL)
{
	sound_buffer = new uint8_t[2 * 2 * 48000 / 5];
}

SNESSystem::~SNESSystem()
{
	Term();

	delete[] sound_buffer;
}

bool SNESSystem::Load(const uint8_t * rom, uint32_t romsize, const uint8_t * sram, uint32_t sramsize)
{
	Term();

	InitSnes9X();

	if (!Memory.LoadROMSNSF(rom, romsize, sram, sramsize))
		return false;

	//S9xSetPlaybackRate(Settings.SoundPlaybackRate);
	S9xSetSoundMute(FALSE);

	// bad hack for gradius3snsf.rar
	//Settings.TurboMode = true;

	return true;
}

void SNESSystem::SoundInit(SNESSoundOut * output)
{
	m_output = output;
}

void SNESSystem::Init()
{
}

void SNESSystem::Reset()
{
	S9xReset();

	rom = Memory.ROM;
	rom_size = Memory.CalculatedSize;
	rom_refs = Memory.ROMCoverage;
	rom_refs_histogram = Memory.ROMCoverageHistogram;
}

void SNESSystem::Term()
{
    Memory.Deinit();
    S9xDeinitAPU();
}

void SNESSystem::CPULoop()
{
	S9xSyncSound();
	S9xMainLoop();

	unsigned bytes = (S9xGetSampleCount() << 1) & ~3;
	ZeroMemory(sound_buffer, bytes);
	S9xMixSamples(sound_buffer, bytes >> 1);

	if (m_output != NULL)
	{
		m_output->write(sound_buffer, bytes);
	}
}

bool SNESSystem::IsHiROM() const
{
	return (Memory.HiROM != 0);
}

uint32_t SNESSystem::GetROMCoverageSize() const
{
	return Memory.ROMCoverageSize;
}
