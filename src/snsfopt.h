
#ifndef SNSFOPT_H
#define SNSFOPT_H

#include <stdint.h>
#include <string>
#include <map>

#ifdef WIN32
#include <Windows.h>
#endif

#include "snsf9x/SNESSystem.h"

class SnsfOpt
{
public:
	SnsfOpt();
	virtual ~SnsfOpt();

	bool LoadROM(const uint8_t * rom, uint32_t romsize, const uint8_t * sram, uint32_t sramsize);
	bool LoadROMFile(const std::string& filename);
	void PatchROM(uint32_t offset, const void * data, uint32_t size, bool apply_base_offset);
	void ResetGame(void);

	void ResetOptimizer(void);
	void Optimize(void);

	bool GetROM(void * rom, uint32_t size, bool wipe_unused_data) const;
	bool SaveROM(const std::string& filename, bool wipe_unused_data) const;
	bool SaveSNSF(const std::string& filename, uint32_t base_offset, bool wipe_unused_data, std::map<std::string, std::string>& tags) const;

	inline uint32_t GetROMSize(void) const
	{
		return m_system->rom_size;
	}

	inline double GetTimeout(void) const
	{
		return optimize_timeout;
	}

	inline void SetTimeout(double timeout)
	{
		optimize_timeout = timeout;
	}

	inline bool IsTimeLoopBased(void) const
	{
		return time_loop_based;
	}

	inline void SetTimeLoopBased(bool sw)
	{
		time_loop_based = sw;
	}

	inline double GetLoopPoint(void)
	{
		return GetLoopPoint(target_loop_count);
	}

	inline double GetLoopPoint(uint8_t count)
	{
		return loop_point[count];
	}

	inline std::string GetLoopPointString(void)
	{
		return GetLoopPointString(target_loop_count);
	}

	inline std::string GetLoopPointString(uint8_t count)
	{
		return ToTimeString(GetLoopPoint(count));
	}

	inline bool IsOneShot(void) const
	{
		return oneshot;
	}

	inline double GetOneShotEndPoint(void) const
	{
		return oneshot_endpoint;
	}

	inline uint8_t GetTargetLoopCount(void) const
	{
		return target_loop_count;
	}

	inline void SetTargetLoopCount(uint8_t count)
	{
		target_loop_count = count;
	}

	inline double GetLoopVerifyLength(void) const
	{
		return loop_verify_length;
	}

	inline void SetLoopVerifyLength(double length)
	{
		loop_verify_length = length;
	}

	inline double GetOneShotVerifyLength(void) const
	{
		return oneshot_verify_length;
	}

	inline void SetOneShotVerifyLength(double length)
	{
		oneshot_verify_length = length;
	}

	inline uint32_t GetParanoidSize(void) const
	{
		return paranoid_bytes;
	}

	inline void SetParanoidSize(uint32_t size)
	{
		paranoid_bytes = size;
	}

	inline void SetSNSFBaseOffset(uint32_t base_offset)
	{
		snsf_base_offset = base_offset;
	}

	inline const std::string& message(void) const
	{
		return m_message;
	}

	static std::string ToTimeString(double t, bool padding = true);
	static double ToTimeValue(const std::string& str);

protected:
	SNESSystem * m_system;

	struct snsf_sound_out : public SNESSoundOut
	{
		uint32_t sample_rate;
		uint32_t samples_received;
		uint32_t silent_samples_received;
		uint16_t silence_threshold;
		uint32_t silence_start;

		snsf_sound_out() :
			sample_rate(32000),
			silence_threshold(8)
		{
			reset_timer();
		}

		virtual ~snsf_sound_out()
		{
		}

		// Receives signed 16-bit stereo audio and a byte count
		virtual void write(const void * samples, unsigned long bytes)
		{
			samples_received += (bytes / 2);

			for (unsigned int i = 0; i < (bytes / 2); i++)
			{
				int16_t samp = ((int16_t *)samples)[i];
				if ((samp + silence_threshold) >= 0 && (samp + silence_threshold) < (silence_threshold * 2))
				{
					if (silent_samples_received == 0)
					{
						silence_start = samples_received;
					}
					silent_samples_received++;
				}
				else
				{
					silence_start = samples_received;
					silent_samples_received = 0;
				}
			}
		}

		void reset_timer(void)
		{
			samples_received = 0;
			silence_start = 0;
			silent_samples_received = 0;
		}

		double get_timer(void) const
		{
			return (double)samples_received / 2 / sample_rate;
		}

		double get_silence_start(void) const
		{
			return (double)silence_start / 2 / sample_rate;
		}

		double get_silence_length(void) const
		{
			return (double)silent_samples_received / 2 / sample_rate;
		}
	};
	snsf_sound_out m_output;

	uint32_t snsf_base_offset;

	uint8_t * rom_refs;
	uint32_t rom_refs_histogram[256];
	uint32_t rom_bytes_used;

	uint8_t * apuram_refs;
	uint32_t apuram_refs_histogram[256];
	uint32_t apuram_bytes_used;

	double song_endpoint;
	double optimize_timeout;
	double optimize_endpoint;
	double optimize_progress_frequency;

	bool time_loop_based;
	uint8_t target_loop_count;
	double loop_verify_length;
	double oneshot_verify_length;

	double time_last_new_data;
	double loop_point[256];
	bool loop_point_updated[256];
	uint8_t loop_count;
	double oneshot_endpoint;
	bool oneshot;

	uint32_t paranoid_bytes;

	bool ReadSNSFFile(const std::string& filename, unsigned int nesting_level, uint8_t * rom_buf, uint32_t * ptr_rom_size, uint8_t * sram_buf, uint32_t * ptr_sram_size, uint32_t * ptr_base_offset);

	static uint32_t MergeRefs(uint8_t * dst_refs, const uint8_t * src_refs, uint32_t size);

	virtual void DetectLoop(void);
	virtual void DetectOneShot(void);
	virtual void AdjustOptimizationEndPoint(void);
	virtual void ResetOptimizerVariables(void);
	virtual void ShowOptimizeProgress(void) const;
	virtual void ShowOptimizeResult(void) const;

	std::string m_message;

private:
	virtual uint8_t ExpectPossibleLoopCount(const uint32_t * histogram, const uint32_t * new_histogram) const;

	std::string rom_path;
	std::string rom_filename;
	uint32_t rom_bytes_used_old;
	uint32_t apuram_bytes_used_old;
};

#endif
