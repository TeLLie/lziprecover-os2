/*  Lziprecover - Data recovery tool for the lzip format
    Copyright (C) 2009-2019 Antonio Diaz Diaz.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "lzip.h"
#include "mtester.h"


namespace {

const char * format_byte( const uint8_t byte )
  {
  enum { buffers = 8, bufsize = 16 };
  static char buffer[buffers][bufsize];	// circle of static buffers for printf
  static int current = 0;
  char * const buf = buffer[current++]; current %= buffers;
  if( ( byte >= 0x20 && byte <= 0x7E ) || byte >= 0xA0 )
    snprintf( buf, bufsize, "'%c' (0x%02X)", byte, byte );
  else
    snprintf( buf, bufsize, "    (0x%02X)", byte );
  return buf;
  }

} // end namespace


void LZ_mtester::print_block( const int len )
  {
  std::fputs( " \"", stdout );
  for( int i = len - 1; i >= 0; --i )
    {
    uint8_t byte = peek( i );
    if( byte < 0x20 || ( byte > 0x7E && byte < 0xA0 ) ) byte = '.';
    std::fputc( byte, stdout );
    }
  std::fputs( "\"\n", stdout );
  }


void LZ_mtester::duplicate_buffer()
  {
  uint8_t * const tmp = new uint8_t[dictionary_size];
  if( data_position() > 0 )
    std::memcpy( tmp, buffer, std::min( data_position(),
                                        (unsigned long long)dictionary_size ) );
  else tmp[dictionary_size-1] = 0;		// prev_byte of first byte
  buffer = tmp;
  }


void LZ_mtester::flush_data()
  {
  if( pos > stream_pos )
    {
    const int size = pos - stream_pos;
    crc32.update_buf( crc_, buffer + stream_pos, size );
    if( outfd >= 0 && writeblock( outfd, buffer + stream_pos, size ) != size )
      throw Error( "Write error" );
    if( pos >= dictionary_size )
      { partial_data_pos += pos; pos = 0; pos_wrapped = true; }
    stream_pos = pos;
    }
  }


bool LZ_mtester::verify_trailer()
  {
  const Lzip_trailer * const trailer = rdec.get_trailer();

  return ( trailer &&
           trailer->data_crc() == crc() &&
           trailer->data_size() == data_position() &&
           trailer->member_size() == member_position() );
  }


/* Return value: 0 = OK, 1 = decoder error, 2 = unexpected EOF,
                 3 = trailer error, 4 = unknown marker found,
                 -1 = pos_limit reached. */
int LZ_mtester::test_member( const unsigned long pos_limit )
  {
  if( pos_limit < Lzip_header::size + 5 ) return -1;
  if( member_position() == Lzip_header::size ) rdec.load();
  while( !rdec.finished() )
    {
    if( member_position() >= pos_limit ) { flush_data(); return -1; }
    const int pos_state = data_position() & pos_state_mask;
    if( rdec.decode_bit( bm_match[state()][pos_state] ) == 0 )	// 1st bit
      {
      // literal byte
      Bit_model * const bm = bm_literal[get_lit_state(peek_prev())];
      if( state.is_char_set_char() )
        put_byte( rdec.decode_tree8( bm ) );
      else
        put_byte( rdec.decode_matched( bm, peek( rep0 ) ) );
      continue;
      }
    // match or repeated match
    int len;
    if( rdec.decode_bit( bm_rep[state()] ) != 0 )		// 2nd bit
      {
      if( rdec.decode_bit( bm_rep0[state()] ) == 0 )		// 3rd bit
        {
        if( rdec.decode_bit( bm_len[state()][pos_state] ) == 0 ) // 4th bit
          { state.set_short_rep(); put_byte( peek( rep0 ) ); continue; }
        }
      else
        {
        unsigned distance;
        if( rdec.decode_bit( bm_rep1[state()] ) == 0 )		// 4th bit
          distance = rep1;
        else
          {
          if( rdec.decode_bit( bm_rep2[state()] ) == 0 )	// 5th bit
            distance = rep2;
          else
            { distance = rep3; rep3 = rep2; }
          rep2 = rep1;
          }
        rep1 = rep0;
        rep0 = distance;
        }
      state.set_rep();
      len = min_match_len + rdec.decode_len( rep_len_model, pos_state );
      }
    else					// match
      {
      len = min_match_len + rdec.decode_len( match_len_model, pos_state );
      unsigned distance = rdec.decode_tree6( bm_dis_slot[get_len_state(len)] );
      if( distance >= start_dis_model )
        {
        const unsigned dis_slot = distance;
        const int direct_bits = ( dis_slot >> 1 ) - 1;
        distance = ( 2 | ( dis_slot & 1 ) ) << direct_bits;
        if( dis_slot < end_dis_model )
          distance += rdec.decode_tree_reversed(
                      bm_dis + ( distance - dis_slot ), direct_bits );
        else
          {
          distance +=
            rdec.decode( direct_bits - dis_align_bits ) << dis_align_bits;
          distance += rdec.decode_tree_reversed4( bm_align );
          if( distance == 0xFFFFFFFFU )		// marker found
            {
            rdec.normalize();
            flush_data();
            if( len == min_match_len )		// End Of Stream marker
              {
              if( verify_trailer() ) return 0; else return 3;
              }
            return 4;
            }
          if( distance > max_rep0 ) max_rep0 = distance;
          }
        }
      rep3 = rep2; rep2 = rep1; rep1 = rep0; rep0 = distance;
      state.set_match();
      if( rep0 >= dictionary_size || ( rep0 >= pos && !pos_wrapped ) )
        { flush_data(); return 1; }
      }
    copy_block( rep0, len );
    }
  flush_data();
  return 2;
  }


/* Return value: 0 = OK, 1 = decoder error, 2 = unexpected EOF,
                 3 = trailer error, 4 = unknown marker found. */
int LZ_mtester::debug_decode_member( const long long dpos, const long long mpos,
                                     const bool show_packets )
  {
  rdec.load();
  while( !rdec.finished() )
    {
    const unsigned long long dp = data_position() + dpos;
    const unsigned long long mp = member_position() + mpos - 4;
    const int pos_state = data_position() & pos_state_mask;
    if( rdec.decode_bit( bm_match[state()][pos_state] ) == 0 )	// 1st bit
      {
      // literal byte
      Bit_model * const bm = bm_literal[get_lit_state(peek_prev())];
      if( state.is_char_set_char() )
        {
        const uint8_t cur_byte = rdec.decode_tree8( bm );
        put_byte( cur_byte );
        if( show_packets )
          std::printf( "%6llu %6llu  literal %s\n",
                       mp, dp, format_byte( cur_byte ) );
        }
      else
        {
        const uint8_t match_byte = peek( rep0 );
        const uint8_t cur_byte = rdec.decode_matched( bm, match_byte );
        put_byte( cur_byte );
        if( show_packets )
          std::printf( "%6llu %6llu  literal %s, match byte %6llu %s\n",
                       mp, dp, format_byte( cur_byte ), dp - rep0 - 1,
                       format_byte( match_byte ) );
        }
      continue;
      }
    // match or repeated match
    int len;
    if( rdec.decode_bit( bm_rep[state()] ) != 0 )		// 2nd bit
      {
      int rep = 0;
      if( rdec.decode_bit( bm_rep0[state()] ) == 0 )		// 3rd bit
        {
        if( rdec.decode_bit( bm_len[state()][pos_state] ) == 0 ) // 4th bit
          {
          if( show_packets )
            std::printf( "%6llu %6llu shortrep %s %6u (%6llu)\n",
                         mp, dp, format_byte( peek( rep0 ) ),
                         rep0 + 1, dp - rep0 - 1 );
          state.set_short_rep(); put_byte( peek( rep0 ) ); continue;
          }
        }
      else
        {
        unsigned distance;
        if( rdec.decode_bit( bm_rep1[state()] ) == 0 )		// 4th bit
          { distance = rep1; rep = 1; }
        else
          {
          if( rdec.decode_bit( bm_rep2[state()] ) == 0 )	// 5th bit
            { distance = rep2; rep = 2; }
          else
            { distance = rep3; rep3 = rep2; rep = 3; }
          rep2 = rep1;
          }
        rep1 = rep0;
        rep0 = distance;
        }
      state.set_rep();
      len = min_match_len + rdec.decode_len( rep_len_model, pos_state );
      if( show_packets )
        std::printf( "%6llu %6llu  rep%c  %6u,%3d (%6llu)",
                     mp, dp, rep + '0', rep0 + 1, len, dp - rep0 - 1 );
      }
    else					// match
      {
      len = min_match_len + rdec.decode_len( match_len_model, pos_state );
      unsigned distance = rdec.decode_tree6( bm_dis_slot[get_len_state(len)] );
      if( distance >= start_dis_model )
        {
        const unsigned dis_slot = distance;
        const int direct_bits = ( dis_slot >> 1 ) - 1;
        distance = ( 2 | ( dis_slot & 1 ) ) << direct_bits;
        if( dis_slot < end_dis_model )
          distance += rdec.decode_tree_reversed(
                      bm_dis + ( distance - dis_slot ), direct_bits );
        else
          {
          distance +=
            rdec.decode( direct_bits - dis_align_bits ) << dis_align_bits;
          distance += rdec.decode_tree_reversed4( bm_align );
          if( distance == 0xFFFFFFFFU )		// marker found
            {
            rdec.normalize();
            flush_data();
            if( show_packets )
              std::printf( "%6llu %6llu  marker code '%d'\n", mp, dp, len );
            if( len == min_match_len )		// End Of Stream marker
              {
              if( show_packets )
                std::printf( "%6llu %6llu  member trailer\n",
                             mpos + member_position(), dpos + data_position() );
              if( verify_trailer() ) return 0;
              if( show_packets ) std::fputs( "trailer error\n", stdout );
              return 3;
              }
            if( len == min_match_len + 1 )	// Sync Flush marker
              {
              rdec.load(); continue;
              }
            return 4;
            }
          if( distance > max_rep0 ) max_rep0 = distance;
          }
        }
      rep3 = rep2; rep2 = rep1; rep1 = rep0; rep0 = distance;
      state.set_match();
      if( show_packets )
        std::printf( "%6llu %6llu  match %6u,%3d (%6lld)",
                     mp, dp, rep0 + 1, len, dp - rep0 - 1 );
      if( rep0 >= dictionary_size || ( rep0 >= pos && !pos_wrapped ) )
        { flush_data(); if( show_packets ) std::fputc( '\n', stdout );
          return 1; }
      }
    copy_block( rep0, len );
    if( show_packets ) print_block( len );
    }
  flush_data();
  return 2;
  }
