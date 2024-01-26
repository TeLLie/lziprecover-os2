/* Lziprecover - Data recovery tool for the lzip format
   Copyright (C) 2009-2024 Antonio Diaz Diaz.

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
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "lzip.h"
#include "lzip_index.h"


/* Show how well the frequency of sequences of N repeated bytes in LZMA data
   matches the value expected for random data. ( 1 / 2^( 8 * N ) )
   Print cumulative data for all files followed by the name of the first
   file with the longest sequence.
*/
int print_nrep_stats( const std::vector< std::string > & filenames,
                      const Cl_options & cl_opts, const int repeated_byte )
  {
  std::vector< unsigned long > len_vector;
  unsigned long long lzma_size = 0;		// total size of LZMA data
  unsigned long best_pos = 0;
  int best_name = -1, retval = 0;
  const bool count_all = ( repeated_byte < 0 || repeated_byte >= 256 );
  bool stdin_used = false;
  for( unsigned i = 0; i < filenames.size(); ++i )
    {
    const bool from_stdin = ( filenames[i] == "-" );
    if( from_stdin ) { if( stdin_used ) continue; else stdin_used = true; }
    const char * const input_filename =
      from_stdin ? "(stdin)" : filenames[i].c_str();
    struct stat in_stats;				// not used
    const int infd = from_stdin ? STDIN_FILENO :
      open_instream( input_filename, &in_stats, false, true );
    if( infd < 0 ) { set_retval( retval, 1 ); continue; }

    const Lzip_index lzip_index( infd, cl_opts, cl_opts.ignore_errors,
                                 cl_opts.ignore_errors );
    if( lzip_index.retval() != 0 )
      {
      show_file_error( input_filename, lzip_index.error().c_str() );
      set_retval( retval, lzip_index.retval() );
      close( infd );
      continue;
      }
    const unsigned long long cdata_size = lzip_index.cdata_size();
    if( !fits_in_size_t( cdata_size ) )		// mmap uses size_t
      { show_file_error( input_filename, "Input file is too large for mmap." );
        set_retval( retval, 1 ); close( infd ); continue; }
    const uint8_t * const buffer =
      (const uint8_t *)mmap( 0, cdata_size, PROT_READ, MAP_PRIVATE, infd, 0 );
    close( infd );
    if( buffer == MAP_FAILED )
      { show_file_error( input_filename, "Can't mmap", errno );
        set_retval( retval, 1 ); continue; }
    for( long j = 0; j < lzip_index.members(); ++j )
      {
      const Block & mb = lzip_index.mblock( j );
      long pos = mb.pos() + 7;			// skip header (+1 byte) and
      const long end = mb.end() - 20;		// trailer of each member
      lzma_size += end - pos;
      while( pos < end )
        {
        const uint8_t byte = buffer[pos++];
        if( buffer[pos] == byte )
          {
          unsigned len = 2;
          ++pos;
          while( pos < end && buffer[pos] == byte ) { ++pos; ++len; }
          if( !count_all && repeated_byte != (int)byte ) continue;
          if( len >= len_vector.size() ) { len_vector.resize( len + 1 );
            best_name = i; best_pos = pos - len; }
          ++len_vector[len];
          }
        }
      }
    munmap( (void *)buffer, cdata_size );
    }

  if( verbosity < 0 ) return retval;
  if( count_all )
    std::fputs( "\nShowing repeated sequences of any byte value.\n", stdout );
  else
    std::printf( "\nShowing repeated sequences of the byte value 0x%02X\n",
                 repeated_byte );
  std::printf( "Total size of LZMA data: %llu bytes (%sBytes)\n",
               lzma_size, format_num( lzma_size, 999 ) );
  for( unsigned len = 2; len < len_vector.size(); ++len )
    if( len_vector[len] > 0 )
      std::printf( "len %u found %lu times, 1 every %llu bytes "
                   "(expected 1 every %sB)\n",
                   len, len_vector[len], lzma_size / len_vector[len],
                   format_num( 1ULL << ( 8 * ( len - count_all ) ), -1ULL, -1 ) );
  if( best_name >= 0 )
    std::printf( "Longest sequence found at position %lu of '%s'\n",
                 best_pos, filenames[best_name].c_str() );
  return retval;
  }
