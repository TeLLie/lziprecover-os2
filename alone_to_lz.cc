/* Lziprecover - Data recovery tool for the lzip format
   Copyright (C) 2009-2021 Antonio Diaz Diaz.

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
#include <new>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "lzip.h"
#include "mtester.h"


namespace {

/* Returns the address of a malloc'd buffer containing the file data and
   the file size in '*size'. The buffer is at least 20 bytes larger.
   In case of error, returns 0 and does not modify '*size'.
*/
uint8_t * read_file( const int infd, long * const size,
                     const char * const filename )
  {
  long buffer_size = 1 << 20;
  uint8_t * buffer = (uint8_t *)std::malloc( buffer_size );
  if( !buffer ) throw std::bad_alloc();

  long file_size = readblock( infd, buffer, buffer_size - 20 );
  while( file_size >= buffer_size - 20 && !errno )
    {
    if( buffer_size >= LONG_MAX )
      { show_file_error( filename, "File is too large" ); std::free( buffer );
        return 0; }
    buffer_size = ( buffer_size <= LONG_MAX / 2 ) ? 2 * buffer_size : LONG_MAX;
    uint8_t * const tmp = (uint8_t *)std::realloc( buffer, buffer_size );
    if( !tmp ) { std::free( buffer ); throw std::bad_alloc(); }
    buffer = tmp;
    file_size +=
      readblock( infd, buffer + file_size, buffer_size - 20 - file_size );
    }
  if( errno )
    {
    show_file_error( filename, "Error reading file", errno );
    std::free( buffer ); return 0;
    }
  *size = file_size;
  return buffer;
  }


bool validate_ds( unsigned * const dictionary_size )
  {
  if( *dictionary_size < min_dictionary_size )
    { *dictionary_size = min_dictionary_size; return false; }
  if( *dictionary_size > max_dictionary_size )
    { *dictionary_size = max_dictionary_size; return false; }
  return true;
  }

} // end namespace


int alone_to_lz( const int infd, const Pretty_print & pp )
  {
  enum { lzma_header_size = 13, offset = lzma_header_size - Lzip_header::size };
  long file_size = 0;
  uint8_t * const buffer = read_file( infd, &file_size, pp.name() );
  if( !buffer ) return 1;
  if( file_size < lzma_header_size )
    { show_file_error( pp.name(), "file is too short" );
      std::free( buffer ); return 2; }

  if( buffer[0] != 93 )			// (45 * 2) + (9 * 0) + 3
    {
    const Lzip_header & header = *(const Lzip_header *)buffer;
    if( header.verify_magic() && header.verify_version() &&
        isvalid_ds( header.dictionary_size() ) )
      show_file_error( pp.name(), "file is already in lzip format" );
    else
      show_file_error( pp.name(), "file has non-default LZMA properties" );
    std::free( buffer ); return 2;
    }
  for( int i = 5; i < 13; ++i ) if( buffer[i] != 0xFF )
    { show_file_error( pp.name(), "file is non-streamed" );
      std::free( buffer ); return 2; }

  if( verbosity >= 1 ) pp();
  unsigned dictionary_size = 0;
  for( int i = 4; i > 0; --i )
    { dictionary_size <<= 8; dictionary_size += buffer[i]; }
  const unsigned orig_dictionary_size = dictionary_size;
  validate_ds( &dictionary_size );
  Lzip_header & header = *(Lzip_header *)( buffer + offset );
  header.set_magic();
  header.dictionary_size( dictionary_size );
  for( int i = 0; i < Lzip_trailer::size; ++i ) buffer[file_size++] = 0;
  {
  LZ_mtester mtester( buffer + offset, file_size - offset, dictionary_size );
  const int result = mtester.test_member();
  if( result == 1 && orig_dictionary_size > max_dictionary_size )
    { pp( "dictionary size is too large" ); std::free( buffer ); return 2; }
  if( result != 3 || !mtester.finished() )
    { pp( "file is corrupt" ); std::free( buffer ); return 2; }
  if( mtester.max_distance() < dictionary_size &&
      dictionary_size > min_dictionary_size )
    {
    dictionary_size =
      std::max( mtester.max_distance(), (unsigned)min_dictionary_size );
    header.dictionary_size( dictionary_size );
    }
  Lzip_trailer & trailer =
    *(Lzip_trailer *)( buffer + file_size - Lzip_trailer::size );
  trailer.data_crc( mtester.crc() );
  trailer.data_size( mtester.data_position() );
  trailer.member_size( mtester.member_position() );
  }
  LZ_mtester mtester( buffer + offset, file_size - offset, dictionary_size );
  if( mtester.test_member() != 0 || !mtester.finished() )
    { pp( "conversion failed" ); std::free( buffer ); return 2; }
  if( writeblock( outfd, buffer + offset, file_size - offset ) != file_size - offset )
    {
    show_error( "Error writing output file", errno );
    std::free( buffer ); return 1;
    }
  std::free( buffer );
  if( verbosity >= 1 ) std::fputs( "done\n", stderr );
  return 0;
  }
