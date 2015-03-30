#define XSFDRIVER_VERSIONS "0.04"
#define XSFDRIVER_MODULENAME "snsf9x.bin"
#define XSFDRIVER_ENTRYNAME "XSFSetup"
#define XSFDRIVER_ISSYSTEMTAG(taglen, tag) (((taglen) > 0) && ((tag)[0] == '_'))
#define XSFDRIVER_GUID1 { 0xc7a86cd4, 0x550b, 0x48d7, { 0x97, 0x44, 0x4d, 0xce, 0xa2, 0x91, 0x55, 0xf4 } } /* {C7A86CD4-550B-48d7-9744-4DCEA29155F4} */
#define XSFDRIVER_GUID2 { 0x369d0f23, 0x1ecd, 0x4f3b, { 0xb7, 0x00, 0xe2, 0xe4, 0xe9, 0xd3, 0xfa, 0x30 } } /* {369D0F23-1ECD-4f3b-B700-E2E4E9D3FA30} */
#define XSFDRIVER_CHANNELMAP { { 8, "BRRPCM %d" } , { 0, 0 } }
#define XSFDRIVER_SIMPLENAME "SNSF Decoder"
#define XSFDRIVER_SAMPLERATE 48000

#define SNES9X_NAME "snes9x-gtk"
#define SNES9X_VERSIONS "r314"
#define SNES9X_COPYRIGHT \
"  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),\n" \
"                             Jerremy Koot (jkoot@snes9x.com)\n" \
"\n" \
"  (c) Copyright 2002 - 2004  Matthew Kendora\n" \
"\n" \
"  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)\n" \
"\n" \
"  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)\n" \
"\n" \
"  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)\n" \
"\n" \
"  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),\n" \
"                             Kris Bleakley (codeviolation@hotmail.com)\n" \
"\n" \
"  (c) Copyright 2002 - 2010  Brad Jorsch (anomie@users.sourceforge.net),\n" \
"                             Nach (n-a-c-h@users.sourceforge.net),\n" \
"                             zones (kasumitokoduck@yahoo.com)\n" \
"\n" \
"  (c) Copyright 2006 - 2007  nitsuja\n" \
"\n" \
"  (c) Copyright 2009 - 2010  BearOso,\n" \
"                             OV2\n" \
"\n" \
"\n" \
"  BS-X C emulator code\n" \
"  (c) Copyright 2005 - 2006  Dreamer Nom,\n" \
"                             zones\n" \
"\n" \
"  C4 x86 assembler and some C emulation code\n" \
"  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),\n" \
"                             Nach,\n" \
"                             zsKnight (zsknight@zsnes.com)\n" \
"\n" \
"  C4 C++ code\n" \
"  (c) Copyright 2003 - 2006  Brad Jorsch,\n" \
"                             Nach\n" \
"\n" \
"  DSP-1 emulator code\n" \
"  (c) Copyright 1998 - 2006  _Demo_,\n" \
"                             Andreas Naive (andreasnaive@gmail.com),\n" \
"                             Gary Henderson,\n" \
"                             Ivar (ivar@snes9x.com),\n" \
"                             John Weidman,\n" \
"                             Kris Bleakley,\n" \
"                             Matthew Kendora,\n" \
"                             Nach,\n" \
"                             neviksti (neviksti@hotmail.com)\n" \
"\n" \
"  DSP-2 emulator code\n" \
"  (c) Copyright 2003         John Weidman,\n" \
"                             Kris Bleakley,\n" \
"                             Lord Nightmare (lord_nightmare@users.sourceforge.net),\n" \
"                             Matthew Kendora,\n" \
"                             neviksti\n" \
"\n" \
"  DSP-3 emulator code\n" \
"  (c) Copyright 2003 - 2006  John Weidman,\n" \
"                             Kris Bleakley,\n" \
"                             Lancer,\n" \
"                             z80 gaiden\n" \
"\n" \
"  DSP-4 emulator code\n" \
"  (c) Copyright 2004 - 2006  Dreamer Nom,\n" \
"                             John Weidman,\n" \
"                             Kris Bleakley,\n" \
"                             Nach,\n" \
"                             z80 gaiden\n" \
"\n" \
"  OBC1 emulator code\n" \
"  (c) Copyright 2001 - 2004  zsKnight,\n" \
"                             pagefault (pagefault@zsnes.com),\n" \
"                             Kris Bleakley\n" \
"                             Ported from x86 assembler to C by sanmaiwashi\n" \
"\n" \
"  SPC7110 and RTC C++ emulator code used in 1.39-1.51\n" \
"  (c) Copyright 2002         Matthew Kendora with research by\n" \
"                             zsKnight,\n" \
"                             John Weidman,\n" \
"                             Dark Force\n" \
"\n" \
"  SPC7110 and RTC C++ emulator code used in 1.52+\n" \
"  (c) Copyright 2009         byuu,\n" \
"                             neviksti\n" \
"\n" \
"  S-DD1 C emulator code\n" \
"  (c) Copyright 2003         Brad Jorsch with research by\n" \
"                             Andreas Naive,\n" \
"                             John Weidman\n" \
"\n" \
"  S-RTC C emulator code\n" \
"  (c) Copyright 2001 - 2006  byuu,\n" \
"                             John Weidman\n" \
"\n" \
"  ST010 C++ emulator code\n" \
"  (c) Copyright 2003         Feather,\n" \
"                             John Weidman,\n" \
"                             Kris Bleakley,\n" \
"                             Matthew Kendora\n" \
"\n" \
"  Super FX x86 assembler emulator code\n" \
"  (c) Copyright 1998 - 2003  _Demo_,\n" \
"                             pagefault,\n" \
"                             zsKnight\n" \
"\n" \
"  Super FX C emulator code\n" \
"  (c) Copyright 1997 - 1999  Ivar,\n" \
"                             Gary Henderson,\n" \
"                             John Weidman\n" \
"\n" \
"  Sound emulator code used in 1.5-1.51\n" \
"  (c) Copyright 1998 - 2003  Brad Martin\n" \
"  (c) Copyright 1998 - 2006  Charles Bilyue'\n" \
"\n" \
"  Sound emulator code used in 1.52+\n" \
"  (c) Copyright 2004 - 2007  Shay Green (gblargg@gmail.com)\n" \
"\n" \
"  SH assembler code partly based on x86 assembler code\n" \
"  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)\n" \
"\n" \
"  2xSaI filter\n" \
"  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa\n" \
"\n" \
"  HQ2x, HQ3x, HQ4x filters\n" \
"  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)\n" \
"\n" \
"  NTSC filter\n" \
"  (c) Copyright 2006 - 2007  Shay Green\n" \
"\n" \
"  GTK+ GUI code\n" \
"  (c) Copyright 2004 - 2010  BearOso\n" \
"\n" \
"  Win32 GUI code\n" \
"  (c) Copyright 2003 - 2006  blip,\n" \
"                             funkyass,\n" \
"                             Matthew Kendora,\n" \
"                             Nach,\n" \
"                             nitsuja\n" \
"  (c) Copyright 2009 - 2010  OV2\n" \
"\n" \
"  Mac OS GUI code\n" \
"  (c) Copyright 1998 - 2001  John Stiles\n" \
"  (c) Copyright 2001 - 2010  zones\n" \
"\n" \
"\n" \
"  Specific ports contains the works of other authors. See headers in\n" \
"  individual files.\n"

#define WINAMPPLUGIN_COPYRIGHT SNES9X_COPYRIGHT
#define WINAMPPLUGIN_NAME "SNSF Decoder " XSFDRIVER_VERSIONS "/ " SNES9X_NAME " " SNES9X_VERSIONS " (x86)"
#define WINAMPPLUGIN_EXTS "SNSF;MINISNSF\0Super Nes Sound Format files(*.SNSF;*.MINISNSF)\0\0\0"
#define WINAMPPLUGIN_TAG_XSFBY "snsfby"

#define KBMEDIAPLUGIN_VERSION 4
#define KBMEDIAPLUGIN_COPYRIGHT SNES9X_COPYRIGHT
#define KBMEDIAPLUGIN_NAME "SNSF plugin " XSFDRIVER_VERSIONS " / " SNES9X_NAME " " SNES9X_VERSIONS
#define KBMEDIAPLUGIN_EXTS(n)	\
	static const char n##_snsfext[] = ".snsf";	\
	static const char n##_minisnsfext[] = ".minisnsf";	\
	static const char * const (n) [] = {	\
		n##_snsfext,	\
		n##_minisnsfext,	\
		0,	\
	};

#define FOOBAR2000COMPONENT_NAME "SNSF decoder / " SNES9X_NAME " " SNES9X_VERSIONS
#define FOOBAR2000COMPONENT_VERSION XSFDRIVER_VERSIONS
#define FOOBAR2000COMPONENT_ABOUT SNES9X_NAME " " SNES9X_VERSIONS "\n" SNES9X_COPYRIGHT "\n"
#define FOOBAR2000COMPONENT_TYPE "Super Nes Sound Format files"
#define FOOBAR2000COMPONENT_EXTS "*.SNSF;*.MINISNSF"
#define FOOBAR2000COMPONENT_EXT_CHECK (!stricmp_utf8(p_extension,"SNSF") || !stricmp_utf8(p_extension,"MINISNSF"))
#define FOOBAR2000COMPONENT_ENCODING "snsf"
