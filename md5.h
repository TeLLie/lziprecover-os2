/* Functions to compute MD5 message digest of memory blocks according to the
   definition of MD5 in RFC 1321 from April 1992.
   Copyright (C) 2020-2022 Antonio Diaz Diaz.

   This library is free software. Redistribution and use in source and
   binary forms, with or without modification, are permitted provided
   that the following conditions are met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions, and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions, and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

class MD5SUM
  {
  uint64_t count;		// data length in bytes, modulo 2^64
  uint32_t state[4];		// state (ABCD)
  uint8_t ibuf[64];		// input buffer with space for a block

  void md5_process_block( const uint8_t block[64] );

public:
  MD5SUM() { reset(); }

  void reset()
    {
    count = 0;
    state[0] = 0x67452301;	// magic initialization constants
    state[1] = 0xEFCDAB89;
    state[2] = 0x98BADCFE;
    state[3] = 0x10325476;
    }

  void md5_update( const uint8_t * const buffer, const unsigned long len );
  void md5_finish( uint8_t digest[16] );
  };

void compute_md5( const uint8_t * const buffer, const unsigned long len,
                  uint8_t digest[16] );

bool check_md5( const uint8_t * const buffer, const unsigned long len,
                const uint8_t digest[16] );
