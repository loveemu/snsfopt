#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <windows.h>

#include <zlib.h>
#include "../xsfc/tagget.h"
#include "../xsfc/drvimpl.h"

#include "../pversion.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"

static struct
{
	unsigned char *rom;
	unsigned romsize;
	unsigned char *sram;
	unsigned sramsize;
	unsigned first;
	unsigned base;
	unsigned save;
} loaderwork = { 0, 0, 0, 0, 0, 0, 0 };

static void load_term(void)
{
	if (loaderwork.rom)
	{
		free(loaderwork.rom);
		loaderwork.rom = 0;
	}
	loaderwork.romsize = 0;
	if (loaderwork.sram)
	{
		free(loaderwork.sram);
		loaderwork.sram = 0;
	}
	loaderwork.sramsize = 0;
	loaderwork.first = 0;
	loaderwork.base = 0;
	loaderwork.save = 0;
}

static DWORD getdwordle(const BYTE *pData)
{
	return pData[0] | (((DWORD)pData[1]) << 8) | (((DWORD)pData[2]) << 16) | (((DWORD)pData[3]) << 24);
}

static int load_map(int level, unsigned char *udata, unsigned usize)
{	
	unsigned char *iptr;
	unsigned isize;
	unsigned char *xptr;
	unsigned xsize = getdwordle(udata + 4);
	unsigned xofs = getdwordle(udata + 0);
	if (!loaderwork.first)
	{
		loaderwork.first = 1;
		loaderwork.base = xofs;
	}
	else
	{
		xofs += loaderwork.base;
	}
	xofs &= 0x1ffffff;
	{
		iptr = loaderwork.rom;
		isize = loaderwork.romsize;
		loaderwork.rom = 0;
		loaderwork.romsize = 0;
	}
	if (!iptr)
	{
		unsigned rsize = xofs + xsize;
		if (0)
		{
			rsize -= 1;
			rsize |= rsize >> 1;
			rsize |= rsize >> 2;
			rsize |= rsize >> 4;
			rsize |= rsize >> 8;
			rsize |= rsize >> 16;
			rsize += 1;
		}
		iptr = (unsigned char *) malloc(rsize + 10);
		if (!iptr)
			return XSF_FALSE;
		memset(iptr, 0, rsize + 10);
		isize = rsize;
	}
	else if (isize < xofs + xsize)
	{
		unsigned rsize = xofs + xsize;
		if (0)
		{
			rsize -= 1;
			rsize |= rsize >> 1;
			rsize |= rsize >> 2;
			rsize |= rsize >> 4;
			rsize |= rsize >> 8;
			rsize |= rsize >> 16;
			rsize += 1;
		}
		xptr = (unsigned char *) realloc(iptr, xofs + rsize + 10);
		if (!xptr)
		{
			free(iptr);
			return XSF_FALSE;
		}
		iptr = xptr;
		isize = rsize;
	}
	memcpy(iptr + xofs, udata + 8, xsize);
	{
		loaderwork.rom = iptr;
		loaderwork.romsize = isize;
	}
	return XSF_TRUE;
}

static int load_mapz(int level, unsigned char *zdata, unsigned zsize, unsigned zcrc)
{
	int ret;
	int zerr;
	uLongf usize = 8;
	uLongf rsize = usize;
	unsigned char *udata;
	unsigned char *rdata;

	udata = (unsigned char *) malloc(usize);
	if (!udata)
		return XSF_FALSE;

	while (Z_OK != (zerr = uncompress(udata, &usize, zdata, zsize)))
	{
		if (usize >= 8)
		{
			uLongf hsize = getdwordle(udata + 4) + 8;
			if (usize == hsize) break;
			rsize = usize = hsize;
		}
		else
		{
			rsize += rsize;
			usize = rsize;
		}
		free(udata);
		if (Z_MEM_ERROR != zerr && Z_BUF_ERROR != zerr)
			return XSF_FALSE;
		udata = (unsigned char *) malloc(usize);
		if (!udata)
			return XSF_FALSE;
	}

	rdata = (unsigned char *) realloc(udata, usize);
	if (!rdata)
	{
		free(udata);
		return XSF_FALSE;
	}

	if (0)
	{
		unsigned ccrc = crc32(crc32(0L, Z_NULL, 0), rdata, usize);
		if (ccrc != zcrc)
			return XSF_FALSE;
	}

	ret = load_map(level, rdata, usize);
	free(rdata);
	return ret;
}

#define MIN(x,y) ((x)<(y)?(x):(y))

static int load_psf_one(int level, unsigned char *pfile, unsigned bytes)
{
	unsigned char *ptr = pfile;
	unsigned code_size;
	unsigned resv_size;
	unsigned code_crc;
	if (bytes < 16 || getdwordle(ptr) != 0x23465350)
		return XSF_FALSE;

	resv_size = getdwordle(ptr + 4);
	code_size = getdwordle(ptr + 8);
	code_crc = getdwordle(ptr + 12);

	if (resv_size)
	{
		unsigned char *rptr = pfile + 16;
		while (rptr + 8 <= pfile + 16 + resv_size)
		{
			unsigned r_type = getdwordle(rptr + 0);
			unsigned r_size = getdwordle(rptr + 4);
			if (r_type == 0)
			{
				if (!loaderwork.sram)
				{
					loaderwork.sramsize = 0x20000;
					loaderwork.sram = (unsigned char *)malloc(loaderwork.sramsize);
					memset(loaderwork.sram, 0xff, loaderwork.sramsize);
				}
				if (loaderwork.sram && rptr + 12 <= pfile + 16 + resv_size)
				{
					unsigned r_ofs = getdwordle(rptr + 8);
					if (r_size > 4 && loaderwork.sramsize > r_ofs)
					{
						unsigned r_len = MIN(r_size - 4, loaderwork.sramsize - r_ofs);
						memcpy(loaderwork.sram + r_ofs, rptr + 12, r_len);
					}
				}
			}
			rptr += 8 + r_size;
		}
	}

	if (code_size)
	{
		ptr = pfile + 16 + resv_size;
		if (16 + resv_size + code_size > bytes)
			return XSF_FALSE;
		if (!load_mapz(level, ptr, code_size, code_crc))
			return XSF_FALSE;
	}

	return XSF_TRUE;
}

typedef struct
{
	const char *tag;
	size_t taglen;
	int level;
	int found;
} loadlibwork_t;

static int load_psf_and_libs(int level, void *pfile, unsigned bytes);

static XSFTag::enum_callback_returnvalue load_psfcb(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
{
	loadlibwork_t *pwork = (loadlibwork_t *)pWork;
	XSFTag::enum_callback_returnvalue ret = XSFTag::enum_continue;
	if (size_t(pNameEnd - pNameTop) == pwork->taglen && !_strnicmp(pNameTop, pwork->tag , pwork->taglen))
	{
		ptrdiff_t l = pValueEnd - pValueTop;
		char *lib = (char*) malloc(l + 1);
		if (!lib)
		{
			ret = XSFTag::enum_break;
		}
		else
		{
			void *libbuf;
			unsigned libsize;
			memcpy(lib, pValueTop, l);
			lib[l] = '\0';
			if (!xsf_get_lib(lib, &libbuf, &libsize))
			{
				ret = XSFTag::enum_break;
			}
			else
			{
				if (!load_psf_and_libs(pwork->level + 1, libbuf, libsize))
					ret = XSFTag::enum_break;
				else
					pwork->found++;
				free(libbuf);
			}
			free(lib);
		}
	}
	return ret;
}

static int load_psf_and_libs(int level, void *pfile, unsigned bytes)
{
	int haslib = 0;
	loadlibwork_t work;

	work.level = level;
	work.tag = "_lib";
	work.taglen = strlen(work.tag);
	work.found = 0;

	if (level <= 10 && XSFTag::Enum(load_psfcb, &work, pfile, bytes) < 0)
		return XSF_FALSE;

	haslib = work.found;

	if (!load_psf_one(level, (unsigned char *)pfile, bytes))
		return XSF_FALSE;

/*	if (haslib) */
	{
		int n = 2;
		do
		{
			char tbuf[16];
#ifdef HAVE_SPRINTF_S
			sprintf_s(tbuf, sizeof(tbuf), "_lib%d", n++);
#else
			sprintf(tbuf, "_lib%d", n++);
#endif
			work.tag = tbuf;
			work.taglen = strlen(work.tag);
			work.found = 0;
			if (XSFTag::Enum(load_psfcb, &work, pfile, bytes) < 0)
				return XSF_FALSE;
		}
		while (work.found);
	}
	return XSF_TRUE;
}

static int load_psf(void *pfile, unsigned bytes)
{
	load_term();

	return load_psf_and_libs(1, pfile, bytes);
}

static double watof(const wchar_t *p){
	int i = 1;
	int s = 1;
	double r = 0;
	double b = 1;
	while (p && *p)
	{
		switch (*p)
		{
#if ((L'9' - L'0') == 9) && ((L'8' - L'0') == 8) && ((L'7' - L'0') == 7) && ((L'6' - L'0') == 6) && ((L'5' - L'0') == 5) && ((L'4' - L'0') == 4) && ((L'3' - L'0') == 3) && ((L'2' - L'0') == 2) && ((L'1' - L'0') == 1)
		case L'0':	case L'1':	case L'2':	case L'3':	case L'4':
		case L'5':	case L'6':	case L'7':	case L'8':	case L'9':
			r = r * 10.0 + (*p - L'0'); if (!i) b *= 0.1; break;
#else
		case L'0': r = r * 10 + 0; if (!i) b *= 0.1; break;
		case L'1': r = r * 10 + 1; if (!i) b *= 0.1; break;
		case L'2': r = r * 10 + 2; if (!i) b *= 0.1; break;
		case L'3': r = r * 10 + 3; if (!i) b *= 0.1; break;
		case L'4': r = r * 10 + 4; if (!i) b *= 0.1; break;
		case L'5': r = r * 10 + 5; if (!i) b *= 0.1; break;
		case L'6': r = r * 10 + 6; if (!i) b *= 0.1; break;
		case L'7': r = r * 10 + 7; if (!i) b *= 0.1; break;
		case L'8': r = r * 10 + 8; if (!i) b *= 0.1; break;
		case L'9': r = r * 10 + 9; if (!i) b *= 0.1; break;
#endif
		case L'.': i = 0; break;
		case L'-': s = -s; break;
		case L' ': break;
		}
		p++;
	}
	return s * r * b;
}

void xsf_set_extend_param(unsigned dwId, const wchar_t *lpPtr)
{
}

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
	Settings.SoundPlaybackRate = XSFDRIVER_SAMPLERATE;
	Settings.SixteenBitSound = TRUE;
	Settings.Stereo	= TRUE;
	Settings.ReverseStereo = FALSE;

    Memory.Init();

    S9xInitAPU();
	S9xInitSound(10, 0);
}

static unsigned long dwChannelMuteOld = 0;

class BUFFER
{
public:
	unsigned char *buf;
	unsigned fil;
	unsigned cur;
	unsigned len;
	BUFFER()
	{
		buf = 0;
		fil = 0;
		cur = 0;
	}
	~BUFFER()
	{
		Term();
	}
	bool Init()
	{
		len = 2*2*48000/5;
		buf = (unsigned char *)malloc(len);
		return buf != NULL;
	}
	void Fill(void)
	{
		if (dwChannelMuteOld != dwChannelMute)
		{
			S9xSetSoundControl(uint8(dwChannelMute ^ 0xff));
			dwChannelMuteOld = dwChannelMute;
		}
		S9xSyncSound();
		S9xMainLoop();
		Mix();
	}
	void Term(void)
	{
		if (buf)
		{
			free(buf);
			buf = 0;
		}
	}
	void Mix()
	{
		unsigned bytes = (S9xGetSampleCount() << 1) & ~3;
		unsigned bleft = (len - fil) & ~3;
		if (bytes == 0) return;
		if (bytes > bleft) bytes = bleft;
		ZeroMemory(buf + fil, bytes);
	    S9xMixSamples(buf + fil, bytes >> 1);
		fil += bytes;
	}
};
static BUFFER buffer;

int xsf_start(void *pfile, unsigned bytes)
{
	if (!load_psf(pfile, bytes))
		return XSF_FALSE;

	InitSnes9X();

	if (!buffer.Init())
		return XSF_FALSE;

	if (!Memory.LoadROMSNSF(loaderwork.rom, loaderwork.romsize, loaderwork.sram, loaderwork.sramsize))
		return XSF_FALSE;

	//S9xSetPlaybackRate(Settings.SoundPlaybackRate);
	S9xSetSoundMute(FALSE);

	// bad hack for gradius3snsf.rar
	//Settings.TurboMode = true;

	return XSF_TRUE;
}

int xsf_gen(void *pbuffer, unsigned samples)
{
	int ret = 0;
	unsigned bytes = samples << 2;
	unsigned char *des = (unsigned char *)pbuffer;
	while (bytes > 0)
	{
		unsigned remain = buffer.fil - buffer.cur;
		while (remain == 0)
		{
			buffer.cur = 0;
			buffer.fil = 0;
			buffer.Fill();

			remain = buffer.fil - buffer.cur;
		}
		unsigned len = remain;
		if (len > bytes)
		{
			len = bytes;
		}
		CopyMemory(des, buffer.buf + buffer.cur, len);
		bytes -= len;
		des += len;
		buffer.cur += len;
		ret += len;
	}
	return ret;
}

void xsf_term(void)
{
    Memory.Deinit ();
    S9xDeinitAPU ();
	load_term();
	buffer.Term();
}

void S9xMessage (int type, int message_no, const char *str)
{
}

bool8 S9xOpenSoundDevice (void)
{
    return (TRUE);
}
