#include "common.h"
#include "crc.h"

u32	crc32_table[4][256];

u32 Reflect(u32 ref, u8 Count)
{
     u32 value = 0;

      // Swap bit 0 for bit 7
      // bit 1 for bit 6, etc.
      for(int i = 1; i < (Count + 1); i++)
      {
            if(ref & 1)
                  value |= 1 << (Count - i);
            ref >>= 1;
      }
      return value; 
}

void Init_CRC32_Table()
{
    // This is the official polynomial used by CRC-32
    // in PKZip, WinZip and Ethernet.
    u32 ulPolynomial = 0x04c11db7;

    // 256 values representing ASCII character codes.
	for(int x = 0; x < 4; x++)
	{
		for(int i = 0; i <= 0xFF; i++)
		{
			crc32_table[x][i]=Reflect(i, 8) << 24;
			for (int j = 0; j < 8; j++)
					crc32_table[x][i] = (crc32_table[x][i] << 1) ^ (crc32_table[x][i] & (1 << 31) ? (ulPolynomial + (x * 8)) : 0);
			crc32_table[x][i] = Reflect(crc32_table[x][i], 32);
		} 
	}
}

u32 GenerateCRC(u8 *StartAddr, u32 len)
{
	u32  ulCRC = -1;

	// Perform the algorithm on each character
	// in the string, using the lookup table values.
	for(; len > 7; len-=8)
	{
		ulCRC ^= *(u32 *)StartAddr;
		ulCRC = crc32_table[3][((ulCRC) & 0xFF)] ^
			    crc32_table[2][((ulCRC >> 8) & 0xFF)] ^
			    crc32_table[1][((ulCRC >> 16) & 0xFF)] ^
				crc32_table[0][((ulCRC >> 24))];
		ulCRC ^= *(u32 *)(StartAddr + 4);
		ulCRC = crc32_table[3][((ulCRC) & 0xFF)] ^
			    crc32_table[2][((ulCRC >> 8) & 0xFF)] ^
			    crc32_table[1][((ulCRC >> 16) & 0xFF)] ^
				crc32_table[0][((ulCRC >> 24))];
		StartAddr+=8;
	}

	if(len > 3)
	{
		ulCRC ^= *(u32 *)StartAddr;
		ulCRC = crc32_table[3][((ulCRC) & 0xFF)] ^
			    crc32_table[2][((ulCRC >> 8) & 0xFF)] ^
			    crc32_table[1][((ulCRC >> 16) & 0xFF)] ^
				crc32_table[0][((ulCRC >> 24))];
		StartAddr+=4;
		len -= 4;
	}

	switch(len)
	{
		case 3:
			ulCRC = crc32_table[0][(ulCRC & 0xFF) ^ *StartAddr] ^ (ulCRC >> 8);
			StartAddr++;
		case 2:
			ulCRC = crc32_table[0][(ulCRC & 0xFF) ^ *StartAddr] ^ (ulCRC >> 8);
			StartAddr++;
		case 1:
			ulCRC = crc32_table[0][(ulCRC & 0xFF) ^ *StartAddr] ^ (ulCRC >> 8);
			StartAddr++;
	}

	return ulCRC;
}
