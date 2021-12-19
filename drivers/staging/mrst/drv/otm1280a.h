#ifndef _OTM1280A_H_
#define _OTM1280A_H_

static const u32 otm_cmd[] = {
	0x018012ff,
	0x00008000,
	0x008012ff,
	0x0000a000,
	0x003838b3,

	0x00008000,
	0x000000cb, 0x00000000, 0x00000000,
	0x00009000,
	0xffc000cb, 0x00000000, 0x00000000, 0x00000000,
	0x0000a000,

	0x000000cb, 0x00000000, 0x00000000, 0x00000000,
	0x0000b000,
	0x000000cb, 0x00000000, 0x00000000, 0x00000000,
	0x0000c000,
	0x0f0004cb, 0x04000000, 0x04040404, 0x0000f404,

	0x0000d000,
	0x00f4f4cb, 0x040408f4, 0x00000004, 0x00000000,
	0x0000e000,
	0x000055cb, 0x00000000, 0x08000000, 0x00000000,
	0x0000f000,

	0x017000cb, 0x00000000,
	0x00008000,
	0x474241cc, 0x4b4a4948, 0x4355524c, 0x4d516553, 0x8d914f4e, 0x40408f8e, 0x00004040,
	0x0000a000,
	0x474241cc, 0x4a4b4c48, 0x43555249, 0x4d516553, 0x8d914f4e, 0x40408f8e, 0xffff4040, 0x000001ff,

	0x0000c000,
	0x474241cc, 0x4b4a4948, 0x4355524c, 0x4d515453, 0x8d914f4e, 0x40408f8e, 0x00004040,
	0x0000e000,
	0x474241cc, 0x4a4b4c48, 0x43555249, 0x4d515453, 0x8d914f4e, 0x40408f8e, 0xffff4040, 0x000001ff,
	0x0009000,

	0x000022c1, 0x00000000,
	0x00008000,
	0x008700c0, 0x87000a06, 0x00000a06, 0x00000000,
	0x00009000,
	0x000a00c0, 0x002a0014,

	0x0000a000,
	0x010300c0, 0x1a010101, 0x00020003,
	
	0x00008000,
	0x000203c2, 0x00020000, 0x00000022,
	
	0x00009000,
	0xff0003c2, 0x000000ff, 0x00002200,
	0x0000b000,
	0x000000c2, 0x00000000, 0x00000000, 0x00000000,
	0x0000a000,
	0xff00ffc2, 0x000a0000, 0x0000000a,

	0x0000c000,
	0x000000c2, 0x00000000, 0x00000000,
	
	0x0000e000,
	0x100084c2, 0x0000000d,
	
	0x0000b300,
	0x00000fc0,
	0x0000a200,
	0x0000ffc1,
	
	0x0000b400,
	0x000054c0,
	0x00008000,
	0x000720c5, 0x6c00b0b0, 0x00000000,

	0x00009000,
	0x028530c5, 0x00159688, 0x4444440c, //0x0000000c
	0x00000000,
	0x520052d8, 0x00000000,
	
	0x00000000,
	0x80738fd9,
	
	0x0000c000,
	0x000095c0,
	0x0000d000,
	0x000005c0,
	0x0000b600,
	0x000000f5,

	0x0000b000,
	0x000011b3,
	0x0000b000,
	0x002000f5,
	
	0x0000b800,
	0x00120cf5,
	0x00009400,
	0x06140af5, 0x00000017,
	0x0000a200,
	0x07140af5, 0x00000014,

	0x00009000,
	0x071607f5, 0x00000014,
	0x0000a000,
	0x0a1202f5, 0x06120712, 0x08120b12, 0x00000012,

	0x00008000,
    0x000000D6,

    0x00000000,
    0x4A4746E1, 0x11050C4E, 0x08060809, 0x210D071C, 0x000A0B15,

    0x00000000,
    0x4A4746E2, 0x11050C4E, 0x08060809, 0x210D071C, 0x000A0B15,

    0x00000000,
    0x5A5857E3, 0x0B030B5C, 0x09080708, 0x1F0D0621, 0x000A0B18, 

    0x00000000,
    0x5A5857E4, 0x0B030B5C, 0x09080708, 0x1F0D0621, 0x000A0B18, 

    0x00000000,
    0x221814E5, 0x20060D2C, 0x05040B0C, 0x250A0412, 0x000A0B1E,

    0x00000000,
    0x221814E6, 0x20060D2C, 0x05040B0C, 0x250A0412, 0x000A0B1E,
#if 0
	0x00000000,   
    0x4A4746E1, 0x11050C4E, 0x08060809, 0x210D071C, 0x000A0B15,
    
    0x00000000,   
    0x4A4746E2, 0x11050C4E, 0x08060809, 0x210D071C, 0x000A0B15,

    0x00000000,   
    0x5A5857E3, 0x0B030B5C, 0x09080708, 0x1F0D0621, 0x000A0B18,

    0x00000000,   
    0x5A5857E4, 0x0B030B5C, 0x09080708, 0x1F0D0621, 0x000A0B18,

    0x00000000,   
    0x221814E5, 0x20060D2C, 0x05040B0C, 0x250A0412, 0x000A0B1E,

    0x00000000,   
    0x221814E6, 0x20060D2C, 0x05040B0C, 0x250A0412, 0x000A0B1E,
#endif
/*	
	0x00000011,

	0x00000029
*/	
	};

static const u32 datalen[] = {
	3, 1, 2, 1, 2, 1, 10, 1, 13, 1,
	13, 1, 14, 1, 13, 1, 13, 1, 14, 1,
	5, 1, 25, 1, 29, 1, 25, 1, 29, 1,
	5, 1, 12, 1, 6, 1, 10, 1, 8,1,
	9, 1, 12, 1, 8, 1, 8, 1, 4,1,
	1,1,1, 1, 2, 1, 8, 1, 11, 1, 
	4,1,3,1, 1, 1, 1, 1, 2, 1, 
	1, 1, 2,1, 2, 1, 4, 1, 4, 1, 
	4, 1, 12, 1, 1, 1,18,1,18,1,18,1,18,1,18,1,18,
};
static const u32 len[] = {
	1, 1, 1, 1, 1, 1, 3, 1, 4, 1,
	4, 1, 4, 1, 4, 1, 4, 1, 4, 1,
	2, 1, 7, 1, 8, 1, 7, 1, 8, 1,
	2, 1, 4, 1, 2, 1, 3, 1, 3, 1,
	3, 1, 4, 1, 3, 1, 3, 1, 2, 1,
	1, 1, 1, 1, 1,1,3, 1, 3, 1, 
	2, 1,1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1,1, 1, 1,1,2, 1, 2, 1, 
	2, 1, 4, 1, 1, 1,5,1,5,1,5,1,5,1,5,1,5,
};

#endif //_OTM1280A_H_
