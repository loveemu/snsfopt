#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#include <zlib.h>

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"

#include "../SPCFile.h"
#include "SNESSystem.h"

extern SNESSystem * spc_snessystem; // apu.cpp

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
	OutputDebugStringA("\n");
#endif
}

bool8 S9xOpenSoundDevice(void)
{
	return TRUE;
}

SNESSystem::SNESSystem() :
	rom_size(0),
	m_output(NULL),
	spc_dump_requested(false),
	spc_dump_succeeded(true)
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

	spc_snessystem = this;

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

	rom_size = Memory.CalculatedSize;
}

void SNESSystem::Term()
{
	spc_snessystem = NULL;

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

bool SNESSystem::IsLoaded() const
{
	return (Memory.ROM != NULL);
}

bool SNESSystem::IsHiROM() const
{
	return (Memory.HiROM != 0);
}

uint8_t * GetROMPointer()
{
	return Memory.ROM;
}

uint32_t SNESSystem::GetFileOffset(uint32_t mem_offset) const
{
	return Memory.ROMToFileOffsetMap[mem_offset];
}

uint32_t SNESSystem::GetMemoryOffset(uint32_t file_offset) const
{
	return Memory.FileToROMOffsetMap[file_offset];
}

void SNESSystem::ReadROM(void * buffer, size_t size, uint32_t file_offset) const
{
	uint32_t buf_offset = 0;
	for (uint32_t off = file_offset; off < file_offset + size; off++)
	{
		uint32_t mem_offset = Memory.FileToROMOffsetMap[off];
		((uint8_t *)buffer)[buf_offset] = Memory.ROM[mem_offset];
		buf_offset++;
	}
}

void SNESSystem::WriteROM(const void * buffer, size_t size, uint32_t file_offset)
{
	uint32_t buf_offset = 0;
	for (uint32_t off = file_offset; off < file_offset + size; off++)
	{
		uint32_t mem_offset = Memory.FileToROMOffsetMap[off];
		Memory.ROM[mem_offset] = ((uint8_t *)buffer)[buf_offset];
		buf_offset++;
	}
}

void SNESSystem::DumpSPCSnapshot(const std::string & filename)
{
	spc_dump_filename = filename;
	spc_dump_requested = true;
	S9xDumpSPCSnapshot();
}

bool SNESSystem::HasSPCDumpFinished(void) const
{
	return !spc_dump_requested;
}

bool SNESSystem::HasSPCDumpSucceeded(void) const
{
	return spc_dump_succeeded;
}

void SNESSystem::SPCSnapshotCallback(SPCFile * spc_file)
{
	spc_dump_succeeded = false;

	if (spc_file == NULL) {
		// spc dump attempt has been failed
		spc_dump_requested = false;
		return;
	}

	// remove emulator name if provided
	spc_file->tags.erase(SPCFile::XID6ItemId::XID6_DUMPER_NAME);
	spc_file->ImportPSFTag(spc_tags);

	if (!spc_file->Save(spc_dump_filename)) {
		delete spc_file;

		spc_dump_requested = false;
		return;
	}

	delete spc_file;

	spc_dump_succeeded = true;
	spc_dump_requested = false;
	return;
}

std::map<std::string, std::string> SNESSystem::GetSPCTags(void) const
{
	return spc_tags;
}

void SNESSystem::SetSPCTags(const std::map<std::string, std::string> & tags)
{
	spc_tags = tags;
}

void SNESSystem::ClearSPCTags(void)
{
	spc_tags.clear();
}

const uint8_t * SNESSystem::GetROMCoverage() const
{
	return Memory.ROMCoverage;
}

uint32_t SNESSystem::GetROMCoverageSize() const
{
	return Memory.ROMCoverageSize;
}

const uint32_t * SNESSystem::GetROMCoverageHistogram() const
{
	return Memory.ROMCoverageHistogram;
}

const uint8_t * SNESSystem::GetAPURAMCoverage() const
{
	return spc_core->get_ram_coverage();
}

uint32_t SNESSystem::GetAPURAMCoverageSize() const
{
	return spc_core->get_ram_coverage_size();
}

const uint32_t * SNESSystem::GetAPURAMCoverageHistogram() const
{
	return (const uint32_t *)spc_core->get_ram_coverage_histogram();
}
