
#define NOMINMAX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include <string>
#include <map>
#include <vector>
#include <iterator>
#include <limits>
#include <algorithm>

#include "snsfopt.h"
#include "cpath.h"
#include "ctimer.h"
#include "PSFFile.h"

#ifdef WIN32
#include <Windows.h>
#include <direct.h>
#include <float.h>
#define getcwd _getcwd
#define chdir _chdir
#define isnan _isnan
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif

#define APP_NAME    "snsfopt"
#define APP_VER     "[2015-04-01]"
#define APP_URL     "http://github.com/loveemu/snsfopt"

#define SNSF_PSF_VERSION		0x23
#define SNSF_EXE_HEADER_SIZE	8

#define SNES_HEADER_SIZE	0x800
#define MIN_SNES_ROM_SIZE	0x8000
#define MAX_SNES_ROM_SIZE	0x800000
#define MAX_SNSF_EXE_SIZE	(MAX_SNES_ROM_SIZE + SNSF_EXE_HEADER_SIZE)

#define MAX_SNES_SRAM_SIZE	0x20000

#define SNES_APU_RAM_SIZE	0x10000

SnsfOpt::SnsfOpt() :
	rom_bytes_used(0),
	apuram_bytes_used(0),
	optimize_timeout(5.0),
	optimize_progress_frequency(0.2),
	time_loop_based(false),
	target_loop_count(2),
	loop_verify_length(20.0),
	oneshot_verify_length(15),
	paranoid_bytes(0),
	snsf_base_offset(0)
{
	m_system = new SNESSystem;
	rom_refs = new uint8_t[SNES_HEADER_SIZE + MAX_SNES_ROM_SIZE];
	apuram_refs = new uint8_t[SNES_APU_RAM_SIZE];

	ResetOptimizer();
}

SnsfOpt::~SnsfOpt()
{
	if (m_system != NULL)
	{
		m_system->Term();
		delete m_system;
	}

	if (rom_refs != NULL)
	{
		delete[] rom_refs;
	}

	if (apuram_refs != NULL)
	{
		delete[] apuram_refs;
	}
}

std::string SnsfOpt::ToTimeString(double t, bool padding)
{
	if (isnan(t))
	{
		return "NaN";
	}

	double seconds = fmod(t, 60.0);
	unsigned int minutes = (unsigned int) (t - seconds) / 60;
	unsigned int hours = minutes / 60;
	minutes %= 60;
	hours %= 60;

	char str[64];
	if (hours != 0)
	{
		sprintf(str, "%u:%02u:%06.3f", hours, minutes, seconds);
	}
	else
	{
		if (padding || minutes != 0)
		{
			sprintf(str, "%u:%06.3f", minutes, seconds);
		}
		else
		{
			sprintf(str, "%.3f", seconds);
		}
	}

	if (!padding)
	{
		size_t len = strlen(str);
		for (size_t i = len - 1; i >= 0; i--)
		{
			if (str[i] == '0')
			{
				str[i] = '\0';
			}
			else
			{
				if (str[i] == '.')
				{
					str[i] = '\0';
				}
				break;
			}
		}
	}

	return str;
}

double SnsfOpt::ToTimeValue(const std::string& str)
{
	if (str.empty())
	{
		return 0.0;
	}

	// split by colons
	std::vector<std::string> tokens;
	size_t current = 0;
	size_t found;
	while((found = str.find_first_of(':', current)) != std::string::npos)
	{
		tokens.push_back(std::string(str, current, found - current));
		current = found + 1;
	}
	tokens.push_back(std::string(str, current, str.size() - current));

	// too many colons?
	if (tokens.size() > 3)
	{
		return std::numeric_limits<double>::quiet_NaN();
	}

	// set token for each fields
	const char * s_hours = "0";
	const char * s_minutes = "0";
	const char * s_seconds = "0";
	switch(tokens.size())
	{
		case 1:
			s_seconds = tokens[0].c_str();
			break;

		case 2:
			s_minutes = tokens[0].c_str();
			s_seconds = tokens[1].c_str();
			break;

		case 3:
			s_hours = tokens[0].c_str();
			s_minutes = tokens[1].c_str();
			s_seconds = tokens[2].c_str();
			break;
	}

	if (*s_hours == '\0' || *s_minutes == '\0' || *s_seconds == '\0' ||
		*s_hours == '+' || *s_minutes == '+' || *s_seconds == '+')
	{
		return std::numeric_limits<double>::quiet_NaN();
	}

	char * endptr = NULL;
	double n_seconds = strtod(s_seconds, &endptr);
	if (*endptr != '\0' || errno == ERANGE || n_seconds < 0)
	{
		return std::numeric_limits<double>::quiet_NaN();
	}

	long n_minutes = strtol(s_minutes, &endptr, 10);
	if (*endptr != '\0' || errno == ERANGE || n_minutes < 0)
	{
		return std::numeric_limits<double>::quiet_NaN();
	}

	long n_hours = strtol(s_hours, &endptr, 10);
	if (*endptr != '\0' || errno == ERANGE || n_hours < 0)
	{
		return std::numeric_limits<double>::quiet_NaN();
	}

	double tv = n_hours * 3600 + n_minutes * 60 + n_seconds;
	if (tv < 0)
	{
		return std::numeric_limits<double>::quiet_NaN();
	}
	return tv;
}

bool SnsfOpt::LoadROM(const uint8_t * rom, uint32_t romsize, const uint8_t * sram, uint32_t sramsize)
{
	rom_path = "";
	rom_filename = "";

	if (m_system->rom != NULL)
	{
		MergeRefs(rom_refs, m_system->GetROMCoverage(), GetROMSize());
		m_system->Term();
	}

	m_system->Load(rom, romsize, sram, sramsize);

	m_system->SoundInit(&m_output);
	m_output.reset_timer();

	m_system->Init();
	m_system->Reset();

	ResetOptimizerVariables();
	return true;
}

bool SnsfOpt::LoadROMFile(const std::string& filename)
{
	uint8_t * rom_buf = NULL;
	uint32_t rom_size;
	uint8_t * sram_buf = NULL;
	uint32_t sram_size;
	uint32_t base_offset;
	bool load_result = false;

	if (PSFFile::IsPSFFile(filename))
	{
		rom_buf = new uint8_t[SNES_HEADER_SIZE + MAX_SNES_ROM_SIZE];
		memset(rom_buf, 0, SNES_HEADER_SIZE + MAX_SNES_ROM_SIZE);

		sram_buf = new uint8_t[MAX_SNES_SRAM_SIZE];
		memset(sram_buf, 0xff, MAX_SNES_SRAM_SIZE);

		load_result = ReadSNSFFile(filename, 0, rom_buf, &rom_size, sram_buf, &sram_size, &base_offset);
		if (load_result)
		{
			load_result = LoadROM(rom_buf, rom_size, sram_buf, sram_size);
			if (load_result)
			{
				char tmppath[PATH_MAX];

				path_getabspath(filename.c_str(), tmppath);
				rom_path = tmppath;

				path_basename(tmppath);
				rom_filename = tmppath;

				snsf_base_offset = base_offset;
			}
		}
	}
	else
	{
		// Plain SNES ROM

		FILE *fp = NULL;
		off_t filesize;

		filesize = path_getfilesize(filename.c_str());
		if (filesize <= 0 || filesize > SNES_HEADER_SIZE + MAX_SNES_ROM_SIZE)
		{
			m_message = filename + " - " + "File size error";
			return false;
		}

		fp = fopen(filename.c_str(), "rb");
		if (fp == NULL)
		{
			m_message = filename + " - " + "File size error";
			return false;
		}

		rom_buf = new uint8_t[filesize];
		if (fread(rom_buf, 1, filesize, fp) != filesize)
		{
			m_message = filename + " - " + "Unable to load ROM data";
			fclose(fp);
			return false;
		}

		load_result = LoadROM(rom_buf, filesize, NULL, 0);
		if (load_result)
		{
			char tmppath[PATH_MAX];

			path_getabspath(filename.c_str(), tmppath);
			rom_path = tmppath;

			path_basename(tmppath);
			rom_filename = tmppath;
		}

		fclose(fp);
	}

	if (rom_buf != NULL)
	{
		delete[] rom_buf;
	}

	if (sram_buf != NULL)
	{
		delete[] sram_buf;
	}

	return load_result;
}

void SnsfOpt::PatchROM(uint32_t offset, const void * data, uint32_t size, bool apply_base_offset)
{
	if (m_system->rom == NULL)
	{
		return;
	}

	uint32_t max_rom_size = MAX_SNES_ROM_SIZE;
	uint8_t * snes_rom = m_system->rom;

	if (apply_base_offset)
	{
		offset += snsf_base_offset;
	}

	if (offset >= max_rom_size)
	{
		return;
	}
	if (offset + size > max_rom_size)
	{
		size = max_rom_size - offset;
	}

	memcpy(&snes_rom[offset], data, size);
}

void SnsfOpt::ResetGame()
{
	if (m_system->rom == NULL)
	{
		return;
	}

	MergeRefs(rom_refs, m_system->GetROMCoverage(), GetROMSize());
	m_system->Reset();
	m_output.reset_timer();
}

bool SnsfOpt::ReadSNSFFile(const std::string& filename, unsigned int nesting_level, uint8_t * rom_buf, uint32_t * ptr_rom_size, uint8_t * sram_buf, uint32_t * ptr_sram_size, uint32_t * ptr_base_offset)
{
	bool result;

	if (ptr_base_offset == NULL)
	{
		m_message = filename + " - " + "Internal error (base offset must not be null)";
		return false;
	}

	// end the nesting hell up
	if (nesting_level > 10)
	{
		m_message = filename + " - " + "Too many snsflibs";
		return false;
	}

	// change base directory
	char savedcwd[PATH_MAX];
	getcwd(savedcwd, PATH_MAX);
	char snsf_dir[PATH_MAX];
	strcpy(snsf_dir, filename.c_str());
	path_dirname(snsf_dir);
	if (snsf_dir[0] != '\0')
	{
		chdir(snsf_dir);
	}

	// open SNSF file
	PSFFile * snsf = PSFFile::load(filename);
	if (snsf == NULL)
	{
		m_message = filename + " - " + "PSF load error";
		chdir(savedcwd);
		return false;
	}

	// check version code
	if (snsf->version != SNSF_PSF_VERSION)
	{
		m_message = filename + " - " + "Mismatch PSF version";
		chdir(savedcwd);
		return false;
	}

	// top-level initialization
	if (nesting_level == 0)
	{
		if (ptr_rom_size != NULL)
		{
			*ptr_rom_size = 0;
		}

		if (ptr_sram_size != NULL)
		{
			*ptr_sram_size = 0;
		}

		// invalidate the base offset
		*ptr_base_offset = 0xffffffff;
	}

	// handle _lib file
	std::map<std::string, std::string>::iterator it_lib = snsf->tags.lower_bound("_lib");
	bool has_lib = (it_lib != snsf->tags.end() && it_lib->first == "_lib");
	if (has_lib)
	{
		if (!ReadSNSFFile(it_lib->second, nesting_level + 1, rom_buf, ptr_rom_size, sram_buf, ptr_sram_size, ptr_base_offset))
		{
			delete snsf;
			chdir(savedcwd);
			return false;
		}
	}

	// SNSF EXE header
	uint32_t rom_address;
	uint32_t rom_size;
	result = true;
	result &= snsf->compressed_exe.readInt(rom_address);
	result &= snsf->compressed_exe.readInt(rom_size);
	if (!result)
	{
		m_message = filename + " - " + "Read error at SNSF EXE header";
		delete snsf;
		chdir(savedcwd);
		return false;
	}

	// valid load address?
	uint32_t rom_offset;
	rom_offset = rom_address & 0x1FFFFFF;

	// first snsf/snsflib?
	if (*ptr_base_offset == 0xffffffff)
	{
		if (rom_offset > 0xff00) // should be 0x7f00 for LoROM, but I don't care
		{
			m_message = filename + " - " + "Base offset out of range";

			delete snsf;
			chdir(savedcwd);
			return false;
		}

		*ptr_base_offset = rom_offset;
	}
	else
	{
		rom_offset += *ptr_base_offset;
	}

	// check offset and size
	if (rom_offset + rom_size > (size_t)SNES_HEADER_SIZE + MAX_SNES_ROM_SIZE)
	{
		m_message = filename + " - " + "ROM size error";

		delete snsf;
		chdir(savedcwd);
		return false;
	}

	// update ROM size
	if (ptr_rom_size != NULL)
	{
		if (*ptr_rom_size < rom_offset + rom_size)
		{
			*ptr_rom_size = rom_offset + rom_size;
		}
	}

	// load ROM data
	if (snsf->compressed_exe.read(&rom_buf[rom_offset], rom_size) != rom_size)
	{
		m_message = filename + " - " + "Unable to load ROM data";

		delete snsf;
		chdir(savedcwd);
		return false;
	}

	// reserved section
	if (snsf->reserved.size() > 8)
	{
		uint32_t reserve_type = snsf->reserved[0] | (snsf->reserved[1] << 8) | (snsf->reserved[2] << 16) | (snsf->reserved[3] << 24);
		uint32_t reserve_size = snsf->reserved[4] | (snsf->reserved[5] << 8) | (snsf->reserved[6] << 16) | (snsf->reserved[7] << 24);

		if (reserve_type == 0)
		{
			// SRAM block
			if (reserve_size < 4)
			{
				m_message = filename + " - " + "Reserve section (SRAM) is too short";

				delete snsf;
				chdir(savedcwd);
				return false;
			}

			// check offset and size
			uint32_t sram_offset = snsf->reserved[8] | (snsf->reserved[9] << 8) | (snsf->reserved[10] << 16) | (snsf->reserved[11] << 24);
			uint32_t sram_patch_size = reserve_size - 4;
			if (sram_offset + sram_patch_size > MAX_SNES_SRAM_SIZE)
			{
				m_message = filename + " - " + "SRAM size error";

				delete snsf;
				chdir(savedcwd);
				return false;
			}

			// load SRAM data
			memcpy(&sram_buf[sram_offset], &snsf->reserved[12], sram_patch_size);

			// update SRAM size
			if (ptr_sram_size != NULL)
			{
				if (*ptr_sram_size < sram_offset + sram_patch_size)
				{
					*ptr_sram_size = sram_offset + sram_patch_size;
				}
			}
		}
		else
		{
			m_message = filename + " - " + "Unsupported reserve section type";

			delete snsf;
			chdir(savedcwd);
			return false;
		}
	}

	// unsupported tags
	if (snsf->tags.count("_memory") != 0)
	{
		fprintf(stderr, "Warning: _memory tag is not supported\n");
	}

	if (snsf->tags.count("_video") != 0)
	{
		fprintf(stderr, "Warning: _video tag is not supported\n");
	}

	if (snsf->tags.count("_sramfill") != 0)
	{
		fprintf(stderr, "Warning: _sramfill tag is not supported\n");
	}

	// handle _libN files
	int libN = 2;
	while (true)
	{
		char libNname[16];
		sprintf(libNname, "_lib%d", libN);

		std::map<std::string, std::string>::iterator it_libN = snsf->tags.lower_bound(libNname);
		if (it_libN == snsf->tags.end() || it_libN->first != libNname)
		{
			break;
		}

		if (!ReadSNSFFile(it_libN->second, nesting_level + 1, rom_buf, ptr_rom_size, sram_buf, ptr_sram_size, ptr_base_offset))
		{
			delete snsf;
			chdir(savedcwd);
			return false;
		}
	}

	m_message = filename + " - " + "Loaded successfully";
	delete snsf;
	chdir(savedcwd);
	return true;
}

void SnsfOpt::ResetOptimizer(void)
{
	memset(rom_refs, 0, MAX_SNES_ROM_SIZE);
	memset(rom_refs_histogram, 0, sizeof(rom_refs_histogram));
	rom_bytes_used = 0;

	memset(apuram_refs, 0, SNES_APU_RAM_SIZE);
	memset(apuram_refs_histogram, 0, sizeof(apuram_refs_histogram));
	apuram_bytes_used = 0;
}

void SnsfOpt::Optimize(void)
{
	timer_init();

	rom_bytes_used_old = m_system->GetROMCoverageSize();
	apuram_bytes_used_old = m_system->GetAPURAMCoverageSize();
	time_last_new_data = m_output.get_timer();

	for (int i = 0; i < 256; i++)
	{
		loop_point[i] = 0.0;
		loop_point_updated[i] = false;
	}
	loop_count = 0;
	oneshot_endpoint = 0.0;
	oneshot = false;

	double time_last_prog = 0.0;
	bool finished = false;

	do
	{
		rom_bytes_used_old = m_system->GetROMCoverageSize();
		m_system->CPULoop();

		// any updates?
		if (m_system->GetROMCoverageSize() != rom_bytes_used_old)
		{
			time_last_new_data = m_output.get_timer();
		}

		// loop detection
		DetectLoop();

		// oneshot detection
		DetectOneShot();

		// adjust endpoint
		AdjustOptimizationEndPoint();

		// is optimization (or loop detection) finished?
		if (m_output.get_timer() >= optimize_endpoint)
		{
			finished = true;
		}

		// show progress
		double time_current = timer_get();
		if (time_current >= time_last_prog + optimize_progress_frequency)
		{
			ShowOptimizeProgress();
			time_last_prog = time_current;
		}
	} while(!finished);

	timer_uninit();

	ShowOptimizeResult();
}

uint8_t SnsfOpt::ExpectPossibleLoopCount(const uint32_t * histogram, const uint32_t * new_histogram) const
{
	// detect possible maximum value of loop count at the moment
	uint8_t loop_count_expected_upper = 255;

	for (int count = 1; count < 256; count++)
	{
		if (histogram[count] != new_histogram[count])
		{
			loop_count_expected_upper = count - 1;
			break;
		}
	}

	return loop_count_expected_upper;
}

void SnsfOpt::DetectLoop()
{
	// detect possible maximum value of loop count at the moment
	uint8_t loop_count_expected_upper = ExpectPossibleLoopCount(rom_refs_histogram, m_system->GetROMCoverageHistogram());

	// check APU RAM as well, if timer is required
	if (time_loop_based) {
		uint8_t apu_loop_count_expected_upper = ExpectPossibleLoopCount(apuram_refs_histogram, m_system->GetAPURAMCoverageHistogram());
		if (apu_loop_count_expected_upper < loop_count_expected_upper) {
			loop_count_expected_upper = apu_loop_count_expected_upper;
		}
	}

	for (int count = 1; count < 256; count++)
	{
		if (rom_refs_histogram[count] != (m_system->GetROMCoverageHistogram())[count])
		{
			loop_count_expected_upper = count - 1;
			break;
		}
	}

	// update loop point of new loops
	for (int count = loop_count_expected_upper; count > 0; count--)
	{
		if (loop_point_updated[count])
		{
			loop_point[count] = m_output.get_timer();
			loop_point_updated[count] = false;
		}
	}

	// verify the loop
	for (int count = loop_count_expected_upper; count > 0; count--)
	{
		if (m_output.get_timer() - loop_point[count] >= loop_verify_length)
		{
			loop_count = count;
			break;
		}
	}

	// update invalid loop points
	for (int count = loop_count_expected_upper + 1; count < 256; count++)
	{
		loop_point[count] = m_output.get_timer();
		loop_point_updated[count] = true;
	}

	// update histogram
	memcpy(rom_refs_histogram, m_system->GetROMCoverageHistogram(), sizeof(rom_refs_histogram));
	memcpy(apuram_refs_histogram, m_system->GetAPURAMCoverageHistogram(), sizeof(apuram_refs_histogram));
}

void SnsfOpt::DetectOneShot()
{
	if (m_output.get_silence_length() >= oneshot_verify_length && loop_count != 0) {
		oneshot_endpoint = m_output.get_silence_start();
		oneshot = true;
	}
	else {
		oneshot = false;
	}
}

void SnsfOpt::AdjustOptimizationEndPoint()
{
	if (time_loop_based)
	{
		if (oneshot)
		{
			song_endpoint = oneshot_endpoint;
			optimize_endpoint = m_output.get_timer();
		}
		else
		{
			song_endpoint = loop_point[target_loop_count];
			optimize_endpoint = loop_point[target_loop_count] + std::max<double>(loop_verify_length, oneshot_verify_length);
		}
	}
	else
	{
		song_endpoint = time_last_new_data;
		optimize_endpoint = time_last_new_data + optimize_timeout;
	}
}

void SnsfOpt::ShowOptimizeProgress() const
{
	printf("%s: ", rom_filename.substr(0, 24).c_str());
	printf("Time = %s", ToTimeString(song_endpoint).c_str());

	printf(", Remaining = %s", ToTimeString(std::max(0.0, optimize_endpoint - m_output.get_timer())).c_str());
	if (!time_loop_based)
	{
		printf(", %d bytes", m_system->GetROMCoverageSize());
	}
	else
	{
		printf(", Loop = %d", loop_count + 1);
	}

	fflush(stdout);

	//       1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
}

void SnsfOpt::ShowOptimizeResult() const
{
	printf("%s: ", rom_filename.c_str());
	printf("Time = %s", ToTimeString(song_endpoint).c_str());

	if (!time_loop_based)
	{
		printf(", %d bytes", m_system->GetROMCoverageSize());
	}
	else
	{
		if (oneshot)
		{
			printf(" (One Shot)");
		}
		else
		{
			printf(" (%d Loops)", target_loop_count);
		}
	}

	printf("                                            ");
	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	printf("\n");
	fflush(stdout);
}

void SnsfOpt::ResetOptimizerVariables()
{
}

uint32_t SnsfOpt::MergeRefs(uint8_t * dst_refs, const uint8_t * src_refs, uint32_t size)
{
	uint32_t bytes_used = 0;
	for (uint32_t i = 0; i < size; i++)
	{
		if (dst_refs[i] + src_refs[i] <= 0xff)
		{
			dst_refs[i] += src_refs[i];
		}
		else
		{
			dst_refs[i] += 0xff;
		}

		if (dst_refs[i] != 0)
		{
			bytes_used++;
		}
	}
	return bytes_used;
}

bool SnsfOpt::GetROM(void * rom, uint32_t size, bool wipe_unused_data) const
{
	uint8_t * snes_rom = m_system->rom;
	uint32_t rom_size = GetROMSize();

	if (m_system->rom == NULL)
	{
		return false;
	}

	if (size > rom_size)
	{
		size = rom_size;
	}

	if (wipe_unused_data)
	{
		uint8_t * rom_refs = new uint8_t[size];
		memcpy(rom_refs, this->rom_refs, size);
		MergeRefs(rom_refs, m_system->GetROMCoverage(), size);

		uint32_t paranoid_count = 0;

		for (uint32_t offset = 0; offset < size; offset++)
		{
			bool preserved_area = false;

			if (m_system->IsHiROM())
			{
				// SNES header
				if (offset >= 0xffc0 && offset <= 0xffff) {
					preserved_area = true;
				}
			}
			else
			{
				// SNES header
				if (offset >= 0x7fc0 && offset <= 0x7fff) {
					preserved_area = true;
				}
			}

			if (preserved_area || rom_refs[offset] != 0 || paranoid_count > 0)
			{
				if (rom_refs[offset] != 0)
				{
					paranoid_count = paranoid_bytes;
				}
				else if (paranoid_count > 0)
				{
					paranoid_count--;
				}

				((uint8_t *)rom)[offset] = snes_rom[offset];
			}
			else
			{
				((uint8_t *)rom)[offset] = 0;
			}
		}

		delete [] rom_refs;
	}
	else
	{
		memcpy(rom, snes_rom, size);
	}

	return true;
}

bool SnsfOpt::SaveROM(const std::string& filename, bool wipe_unused_data) const
{
	uint32_t size = GetROMSize();
	uint8_t * rom = new uint8_t[size];
	FILE * fp = NULL;
	bool result = false;

	if (!GetROM(rom, size, wipe_unused_data))
	{
		delete [] rom;
		return false;
	}

	fp = fopen(filename.c_str(), "wb");
	if (fp == NULL)
	{
		delete [] rom;
		return false;
	}

	//while (size > 0 && rom[size - 1] == 0)
	//{
	//	size--;
	//}

	result = (fwrite(rom, 1, size, fp) == size);

	fclose(fp);
	delete [] rom;
	return result;
}

bool SnsfOpt::SaveSNSF(const std::string& filename, bool wipe_unused_data) const
{
	std::map<std::string, std::string> tags;
	return SaveSNSF(filename, snsf_base_offset, wipe_unused_data, tags);
}

bool SnsfOpt::SaveSNSF(const std::string& filename, uint32_t base_offset, bool wipe_unused_data) const
{
	std::map<std::string, std::string> tags;
	return SaveSNSF(filename, base_offset, wipe_unused_data, tags);
}

bool SnsfOpt::SaveSNSF(const std::string& filename, uint32_t base_offset, bool wipe_unused_data, std::map<std::string, std::string>& tags) const
{
	uint32_t size = GetROMSize();
	bool result = false;

	if (base_offset > 0xff00 || base_offset >= size)
	{
		return false;
	}

	uint8_t * rom = new uint8_t[size];

	if (!GetROM(rom, size, wipe_unused_data))
	{
		delete [] rom;
		return false;
	}

	uint32_t snsf_rom_size = size - base_offset;

	ZlibWriter exe(Z_BEST_COMPRESSION);

	result = true;
	result &= exe.writeInt(base_offset);
	result &= exe.writeInt(snsf_rom_size);
	if (!result)
	{
		delete [] rom;
		return false;
	}

	if (exe.write(&rom[base_offset], snsf_rom_size) != snsf_rom_size)
	{
		delete [] rom;
		return false;
	}

	result = PSFFile::save(filename, SNSF_PSF_VERSION, NULL, 0, exe, tags);

	delete [] rom;
	return result;
}

enum SnsfOptProcMode
{
	SNSFOPT_PROC_NONE = 0,
	SNSFOPT_PROC_F,
	SNSFOPT_PROC_L,
	SNSFOPT_PROC_R,
	SNSFOPT_PROC_S,
	SNSFOPT_PROC_T,
};

static void usage(const char * progname, bool extended)
{
	printf("%s %s\n", APP_NAME, APP_VER);
	printf("<%s>\n", APP_URL);
	printf("\n");
	printf("Usage\n");
	printf("-----\n");
	printf("\n");
	printf("Syntax: `%s [options] [-s or -l or -f or -t] [snsf files]`\n", progname);
	printf("\n");

	if (!extended)
	{
		printf("for detailed usage info, type %s -?\n", progname);
	}
	else
	{
		printf("### Options\n");
		printf("\n");
		printf("`-T [time]`\n");
		printf("  : Runs the emulation till no new data has been found for [time] specified.\n");
		printf("    Time is specified in mm:ss.nnn format   \n");
		printf("    mm = minutes, ss = seoconds, nnn = milliseconds\n");
		printf("\n");
		printf("`-P [bytes]`\n");
		printf("  : I am paranoid, and wish to assume that any data \n");
		printf("    within [bytes] bytes of a used byte, is also used\n");
		printf("\n");
		printf("`--offset [load offset]`\n");
		printf("  : Load offset of the base snsflib file.\n");
		printf("    (The option works only if the input is SNES ROM file)\n");
		printf("\n");
		printf("#### File Processing Modes (-s) (-l) (-f) (-r) (-t)\n");
		printf("\n");
		printf("`-f [snsf files]`\n");
		printf("  : Optimize single files, and in the process, convert\n");
		printf("    minisnsfs/snsflibs to single snsf files\n");
		printf("\n");
		printf("`-l [snsf files]`\n");
		printf("  : Optimize the snsflib using passed snsf files.\n");
		printf("\n");
		printf("`-r [snsf files]`\n");
		printf("  : Convert to Rom files, no optimization\n");
		printf("\n");
		printf("`-s [snsflib] [Hex offset] [Count]`\n");
		printf("  : Optimize snsflib using a known offset/count\n");
		printf("\n");
		printf("`-t [options] [snsf files]`\n");
		printf("  : Times the SNSF files. (for auto tagging, use the `-T` option)\n");
		printf("    Unlike psf playback, silence detection is MANDATORY\n");
		printf("    Do NOT try to evade this with an excessively long silence detect time.\n");
		printf("    (The max time is less than 2*Verify loops for silence detection)\n");
		printf("\n");
		printf("#### Options for -t\n");
		printf("\n");
		printf("`-V [time]`\n");
		printf("  : Length of verify loops at end point. (Default 20 seconds)\n");
		printf("\n");
		printf("`-L [count]`\n");
		printf("  : Number of loops to time for. (Default 2, max 255)\n");
		printf("\n");
		printf("`-T`\n");
		printf("  : Tag the songs with found time.\n");
		printf("    A Fade is also added if the song is not detected to be one shot.\n");
		printf("\n");
		printf("`-F [time]`\n");
		printf("  : Length of looping song fade. (default 10.000)\n");
		printf("\n");
		printf("`-f [time]`\n");
		printf("  : Length of one shot song postgap. (default 1.000)\n");
		printf("\n");
		printf("`-s [time]`\n");
		printf("  : Time in seconds for silence detection (default 15 seconds)\n");
		printf("    Max (2*Verify loop count) seconds.\n");
		printf("\n");
	}
}

int main(int argc, char *argv[])
{
	SnsfOpt opt;
	SnsfOptProcMode mode = SNSFOPT_PROC_NONE;

	std::string out_name;

	double loopFadeLength = 10.0;
	double oneshotPostgapLength = 1.0;
	bool addSNSFTags = false;

	long l;
	unsigned long ul;
	char * endptr = NULL;

	if (argc >= 2 && (strcmp(argv[1], "-?") == 0 || strcmp(argv[1], "--help") == 0))
	{
		usage(argv[0], true);
		return -1;
	}
	else if (argc == 1)
	{
		usage(argv[0], false);
		return -1;
	}

	int argi;
	for(argi = 1; argi < argc; argi++)
	{
		if (argv[argi][0] != '-')
		{
			break;
		}

		if (argv[argi][1] == 'f')  //regular .snsf optimization. this option doubles as minisnsf->snsf converter
		{
			mode = SNSFOPT_PROC_F;

			if (argc <= (argi + 1))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}
			argi++;
		}
		else if (argv[argi][1] == 'r')
		{
			mode = SNSFOPT_PROC_R;

			if (argc <= (argi + 1))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}
			argi++;
		}
		else if (argv[argi][1] == 's')  //Song value snsflib optimization.
		{
			mode = SNSFOPT_PROC_S;

			if (argc <= (argi + 3))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}
			argi++;
		}
		else if (argv[argi][1] == 'l')  //snsflib optimization.
		{
			mode = SNSFOPT_PROC_L;

			if (argc <= (argi + 1))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}
			argi++;
		}
		else if (argv[argi][1] == 't')
		{
			mode = SNSFOPT_PROC_T;
			opt.SetTimeLoopBased(true);

			if (argc <= (argi + 1))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}
			argi++;
		}
		else if (argv[argi][1] == 'T') // Optimize while no new data found for.
		{
			if (argc <= (argi + 1))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}

			opt.SetTimeout(SnsfOpt::ToTimeValue(argv[argi + 1]));
			argi++;
		}
		else if (argv[argi][1] == 'P') // I am paranoid. assume x bytes within a used byte is also used.
		{
			if (argc <= (argi + 1))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}

			l = strtol(argv[argi + 1], &endptr, 0);
			if (*endptr != '\0' || errno == ERANGE || l < 0)
			{
				fprintf(stderr, "Error: Number format error \"%s\"\n", argv[argi + 1]);
				return 1;
			}
			opt.SetParanoidSize(l);
			argi++;
		}
		else if (argv[argi][1] == 'o')
		{
			if (argc <= (argi + 1))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}

			out_name = argv[argi + 1];
			argi++;
		}
		else if (strcmp(argv[argi], "--offset") == 0)
		{
			if (argc <= (argi + 1))
			{
				fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
				return 1;
			}

			l = strtol(argv[argi + 1], &endptr, 0);
			if (*endptr != '\0' || errno == ERANGE || l < 0)
			{
				fprintf(stderr, "Error: Number format error \"%s\"\n", argv[argi + 1]);
				return 1;
			}
			opt.SetSNSFBaseOffset((uint32_t)l);
			argi++;
		}
		else
		{
			fprintf(stderr, "Error: Unknown option \"%s\"\n", argv[argi]);
			return 1;
		}

		if (mode != SNSFOPT_PROC_NONE)
		{
			if (mode == SNSFOPT_PROC_T)
			{
				for (; argi < argc; argi++)
				{
					if (argv[argi][0] != '-')
					{
						break;
					}

					switch(argv[argi][1])
					{
					case 'V':
						if (argc <= (argi + 1))
						{
							fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
							return 1;
						}
						opt.SetLoopVerifyLength(SnsfOpt::ToTimeValue(argv[argi + 1]));
						argi++;
						break;

					case 'L':
						if (argc <= (argi + 1))
						{
							fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
							return 1;
						}

						l = strtol(argv[argi + 1], &endptr, 0);
						if (*endptr != '\0' || errno == ERANGE || l < 0)
						{
							fprintf(stderr, "Error: Number format error \"%s\"\n", argv[argi + 1]);
							return 1;
						}
						if (l == 0 || l > 255)
						{
							fprintf(stderr, "Error: Loop count must be in range (1..255)\n");
							return 1;
						}
						opt.SetTargetLoopCount((uint8_t)l);
						argi++;
						break;

					case 'T':
						addSNSFTags = true;
						break;

					case 'F':
						if (argc <= (argi + 1))
						{
							fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
							return 1;
						}

						loopFadeLength = SnsfOpt::ToTimeValue(argv[argi + 1]);
						argi++;
						break;

					case 'f':
						if (argc <= (argi + 1))
						{
							fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
							return 1;
						}

						oneshotPostgapLength = SnsfOpt::ToTimeValue(argv[argi + 1]);
						argi++;
						break;

					case 's':
						if (argc <= (argi + 1))
						{
							fprintf(stderr, "Error: Too few arguments for \"%s\"\n", argv[argi]);
							return 1;
						}

						opt.SetOneShotVerifyLength(SnsfOpt::ToTimeValue(argv[argi + 1]));
						argi++;
						break;

					default:
						fprintf(stderr, "Error: Unknown option \"%s\"\n", argv[argi]);
						return 1;
					}
				}

				if (opt.GetOneShotVerifyLength() > (opt.GetLoopVerifyLength() * 2))
				{
					double oneshot_verify_length = opt.GetLoopVerifyLength() * 2;
					opt.SetOneShotVerifyLength(oneshot_verify_length);
					fprintf(stderr, "Warning: Max silence length is %s\n", SnsfOpt::ToTimeString(oneshot_verify_length).c_str());
				}
			}
			break;
		}
	}

	if (mode == SNSFOPT_PROC_NONE)
	{
		fprintf(stderr, "Error: You need to specify a processing mode, -f, -s, -l, -r, -t\n");
		return 1;
	}

	switch (mode)
	{
		case SNSFOPT_PROC_S:
		{
			ul = strtoul(argv[argi + 1], &endptr, 16);
			if (*endptr != '\0' || errno == ERANGE)
			{
				fprintf(stderr, "Error: Number format error \"%s\"\n", argv[argi + 1]);
				return 1;
			}
			uint32_t minisnsf_offset = ul & 0x1FFFFFF;

			l = strtol(argv[argi + 2], &endptr, 0);
			if (*endptr != '\0' || errno == ERANGE || l < 0)
			{
				fprintf(stderr, "Error: Number format error \"%s\"\n", argv[argi + 1]);
				return 1;
			}
			uint32_t minisnsf_count = (uint32_t)l;

			uint32_t minisnsf_size = 0;
			do
			{
				minisnsf_size++;
			} while (minisnsf_count >> (minisnsf_size * 8));

			// determine output filename
			std::string out_path;
			if (out_name.empty())
			{
				const char *ext = path_findext(argv[argi]);
				if (*ext == '\0')
				{
					out_path = argv[argi];
					out_path += ".snsflib";
				}
				else
				{
					out_path = std::string(argv[argi], ext - argv[argi]);
					out_path += ".snsflib";
				}
			}
			else
			{
				out_path = out_name;

				const char *ext = path_findext(out_name.c_str());
				if (*ext == '\0')
				{
					out_path += ".snsflib";
				}
			}

			// optimize
			opt.ResetOptimizer();
			if (!opt.LoadROMFile(argv[argi]))
			{
				fprintf(stderr, "Error: %s\n", opt.message().c_str());
				return 1;
			}
			for (uint32_t song = 0; song < minisnsf_count; song++)
			{
				printf("Optimizing %s  Song value %X\n", argv[argi], song);

				uint8_t patch[4] = {
					song & 0xff,
					(song >> 8) & 0xff,
					(song >> 16) & 0xff,
					(song >> 24) & 0xff,
				};
				opt.PatchROM(minisnsf_offset, patch, minisnsf_size, true);
				opt.ResetGame();

				opt.Optimize();
			}

			opt.SaveSNSF(out_path, true);
			break;
		}

		case SNSFOPT_PROC_L:
		{
			// determine output filename
			std::string out_path = out_name;
			if (out_name.empty())
			{
				const char *ext = path_findext(argv[argi]);
				if (*ext == '\0')
				{
					out_path = argv[argi];
					out_path += ".snsflib";
				}
				else
				{
					out_path = std::string(argv[argi], ext - argv[argi]);
					out_path += ".snsflib";
				}
			}
			else
			{
				out_path = out_name;

				const char *ext = path_findext(out_name.c_str());
				if (*ext == '\0')
				{
					out_path += ".snsflib";
				}
			}

			// optimize
			opt.ResetOptimizer();
			for (; argi < argc; argi++)
			{
				printf("Optimizing %s\n", argv[argi]);

				if (!opt.LoadROMFile(argv[argi]))
				{
					fprintf(stderr, "Error: %s\n", opt.message().c_str());
					return 1;
				}
				opt.Optimize();
			}

			opt.SaveSNSF(out_path, true);
			break;
		}

		case SNSFOPT_PROC_F:
		{
			if (argi + 1 < argc && !out_name.empty())
			{
				fprintf(stderr, "Error: Output filename cannot be specified to multiple ROMs.\n");
				return 1;
			}

			// optimize
			for (; argi < argc; argi++)
			{
				// determine output filename
				std::string out_path = out_name;
				if (out_name.empty())
				{
					const char *ext = path_findext(argv[argi]);
					if (*ext == '\0')
					{
						out_path = argv[argi];
						out_path += ".snsf";
					}
					else
					{
						out_path = std::string(argv[argi], ext - argv[argi]);
						out_path += ".snsf";
					}
				}
				else
				{
					out_path = out_name;

					const char *ext = path_findext(out_name.c_str());
					if (*ext == '\0')
					{
						out_path += ".snsf";
					}
				}

				printf("Optimizing %s\n", argv[argi]);

				opt.ResetOptimizer();
				if (!opt.LoadROMFile(argv[argi]))
				{
					fprintf(stderr, "Error: %s\n", opt.message().c_str());
					return 1;
				}
				opt.Optimize();
				opt.SaveSNSF(out_path, true);
			}
			break;
		}

		case SNSFOPT_PROC_R:
		{
			if (argi + 1 < argc && !out_name.empty())
			{
				fprintf(stderr, "Error: Output filename cannot be specified to multiple ROMs.\n");
				return 1;
			}

			for (int i = argi; i < argc; i++)
			{
				std::string out_path;
				if (out_name.empty())
				{
					const char *ext = path_findext(argv[i]);
					if (*ext == '\0')
					{
						out_path = argv[i];
						out_path += ".smc";
					}
					else
					{
						out_path = std::string(argv[i], ext - argv[i]);
						out_path += ".smc";
					}
				}
				else
				{
					out_path = out_name;

					const char *ext = path_findext(out_name.c_str());
					if (*ext == '\0')
					{
						out_path += ".smc";
					}
				}

				if (!opt.LoadROMFile(argv[i]))
				{
					fprintf(stderr, "Error: %s\n", opt.message().c_str());
					return 1;
				}
				opt.SaveROM(out_path, false);
			}
			break;
		}

		case SNSFOPT_PROC_T:
		{
			if (!out_name.empty())
			{
				fprintf(stderr, "Error: Output filename cannot be specified for \"-t\".\n");
				return 1;
			}

			// optimize
			for (; argi < argc; argi++)
			{
				// determine output filename
				std::string out_path = argv[argi];

				opt.ResetOptimizer();
				if (!opt.LoadROMFile(argv[argi]))
				{
					fprintf(stderr, "Error: %s\n", opt.message().c_str());
					return 1;
				}
				opt.Optimize();

#ifdef _DEBUG
				for (int count = 1; count <= opt.GetTargetLoopCount(); count++)
				{
					printf("Loop Point %d = %s\n", count, opt.GetLoopPointString(count).c_str());
				}
#endif

				if (addSNSFTags)
				{
					PSFFile * snsf = PSFFile::load(argv[argi]);
					if (snsf == NULL)
					{
						fprintf(stderr, "Error: Invalid PSF file %s (file operation error)\n", argv[argi]);
						return 1;
					}

					if (opt.IsOneShot())
					{
						snsf->tags["length"] = SnsfOpt::ToTimeString(opt.GetOneShotEndPoint() + oneshotPostgapLength, false);
						snsf->tags.erase("fade");
					}
					else
					{
						snsf->tags["length"] = SnsfOpt::ToTimeString(opt.GetLoopPoint(), false);

						if (loopFadeLength >= 0.001)
						{
							snsf->tags["fade"] = SnsfOpt::ToTimeString(loopFadeLength, false);
						}
						else
						{
							snsf->tags.erase("fade");
						}
					}

					snsf->save(out_path);
					delete snsf;
				}
			}
			break;
		}

		default:
			fprintf(stderr, "Sorry, specified processing mode is not supported yet.\n");
			return 1;
	}

	return 0;
}
