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
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "lzip.h"
#include "md5.h"
#include "mtester.h"
#include "lzip_index.h"


namespace {

bool check_member( const uint8_t * const mbuffer, const long msize,
                   const unsigned dictionary_size, const char * const name,
                   md5_type & digest )
  {
  MD5SUM md5sum;
  LZ_mtester mtester( mbuffer, msize, dictionary_size, -1, &md5sum );
  if( mtester.test_member() != 0 || !mtester.finished() )
    { show_file_error( name, "Error checking input file." ); return false; }
  md5sum.md5_finish( digest );
  return true;
  }


bool compare_member( const uint8_t * const mbuffer, const long msize,
                     const unsigned dictionary_size,
                     const long long byte_pos, const md5_type & digest )
  {
  MD5SUM md5sum;
  LZ_mtester mtester( mbuffer, msize, dictionary_size, -1, &md5sum );
  bool error = ( mtester.test_member() != 0 || !mtester.finished() );
  if( !error )
    {
    md5_type new_digest;
    md5sum.md5_finish( new_digest );
    if( digest != new_digest ) error = true;
    }
  if( error && verbosity >= 0 )
    std::printf( "byte %llu comparison failed\n", byte_pos );
  return !error;
  }


int test_member_rest( const LZ_mtester & master, uint8_t * const buffer2,
                      long * const failure_posp,
                      const unsigned long long byte_pos )
  {
  LZ_mtester mtester( master );		// tester with external buffer
  mtester.duplicate_buffer( buffer2 );
  int result = mtester.test_member( LONG_MAX, LLONG_MAX, stdout, byte_pos );
  if( result == 0 && !mtester.finished() ) result = -1;	// false negative
  if( result != 0 ) *failure_posp = mtester.member_position();
  return result;
  }


long next_pct_pos( const Lzip_index & lzip_index, const long i, const int pct,
                   const int sector_size = 0 )
  {
  if( pct <= 0 ) return 0;
  const long long cdata_size = lzip_index.cdata_size() - sector_size;
  const long long mpos = lzip_index.mblock( i ).pos();
  const long long msize = lzip_index.mblock( i ).size() - sector_size;
  long long pct_pos = (long long)( cdata_size / ( 100.0 / pct ) );

  if( pct_pos <= mpos ) pct_pos = 0;
  else if( pct_pos == cdata_size ) pct_pos = msize - 21;	// 100%
  else if( pct_pos >= mpos + msize ) pct_pos = msize;
  else pct_pos -= mpos;
  return pct_pos;
  }

} // end namespace


/* Test 1-bit errors in LZMA streams in file.
   Unless verbosity >= 1, print only the bytes with interesting results. */
int lunzcrash_bit( const char * const input_filename,
                   const Cl_options & cl_opts )
  {
  struct stat in_stats;				// not used
  const int infd = open_instream( input_filename, &in_stats, false, true );
  if( infd < 0 ) return 1;

  const Lzip_index lzip_index( infd, cl_opts );
  if( lzip_index.retval() != 0 )
    { show_file_error( input_filename, lzip_index.error().c_str() );
      return lzip_index.retval(); }
  if( verbosity >= 2 ) printf( "Testing file '%s'\n", input_filename );

  const long long cdata_size = lzip_index.cdata_size();
  long positions = 0, decompressions = 0, successes = 0, failed_comparisons = 0;
  int pct = ( cdata_size >= 1000 && isatty( STDERR_FILENO ) ) ? 0 : 100;
  for( long i = 0; i < lzip_index.members(); ++i )
    {
    const long long mpos = lzip_index.mblock( i ).pos();
    const long long msize = lzip_index.mblock( i ).size();
    uint8_t * const mbuffer = read_member( infd, mpos, msize, input_filename );
    if( !mbuffer ) return 1;
    const unsigned dictionary_size = lzip_index.dictionary_size( i );
    md5_type md5_orig;
    if( !check_member( mbuffer, msize, dictionary_size, input_filename,
                       md5_orig ) ) return 2;
    long pct_pos = next_pct_pos( lzip_index, i, pct );
    long pos = Lzip_header::size + 1, printed = 0;	// last pos printed
    const long end = msize - 20;
    if( verbosity == 0 )	// give a clue of the range being tested
      std::printf( "Testing bytes %llu to %llu\n", mpos + pos, mpos + end - 1 );
    LZ_mtester master( mbuffer, msize, dictionary_size );
    uint8_t * const buffer2 = new uint8_t[dictionary_size];
    for( ; pos < end; ++pos )
      {
      const long pos_limit = pos - 16;
      if( pos_limit > 0 && master.test_member( pos_limit ) != -1 )
        { show_error( "Can't advance master." ); return 1; }
      if( verbosity >= 0 && pos >= pct_pos )
        { std::fprintf( stderr, "\r%3u%% done\r", pct ); ++pct;
          pct_pos = next_pct_pos( lzip_index, i, pct ); }
      if( verbosity >= 1 )
        { std::printf( "byte %llu\n", mpos + pos ); printed = pos; }
      ++positions;
      const uint8_t byte = mbuffer[pos];
      for( uint8_t mask = 1; mask != 0; mask <<= 1 )
        {
        ++decompressions;
        mbuffer[pos] ^= mask;
        long failure_pos = 0;
        const int result = test_member_rest( master, buffer2, &failure_pos,
                           ( printed < pos ) ? mpos + pos : 0 );
        if( result <= 0 )
          {
          ++successes;
          if( verbosity >= 0 )
            {
            if( printed < pos )
              { std::printf( "byte %llu\n", mpos + pos ); printed = pos; }
            std::printf( "0x%02X (0x%02X^0x%02X) passed the test%s",
                         mbuffer[pos], byte, mask, ( result < 0 ) ? "" : "\n" );
            if( result < 0 )
              std::printf( ", but only consumed %lu bytes of %llu\n",
                           failure_pos, msize );
            }
          if( !compare_member( mbuffer, msize, dictionary_size, mpos + pos,
                               md5_orig ) ) ++failed_comparisons;
          }
        else if( result == 1 )
          {
          if( verbosity >= 2 ||
              ( verbosity >= 1 && failure_pos - pos >= 10000 ) ||
              ( verbosity >= 0 && failure_pos - pos >= 50000 ) )
            {
            if( printed < pos )
              { std::printf( "byte %llu\n", mpos + pos ); printed = pos; }
            std::printf( "Decoder error at pos %llu\n", mpos + failure_pos );
            }
          }
        else if( result == 3 || result == 4 )	// test_member printed the error
          { if( verbosity >= 0 && printed < pos ) printed = pos; }
        else if( verbosity >= 0 )
          {
          if( printed < pos )
            { std::printf( "byte %llu\n", mpos + pos ); printed = pos; }
          if( result == 2 )
            std::printf( "File ends unexpectedly at pos %llu\n",
                         mpos + failure_pos );
          else
            std::printf( "Unknown error code '%d'\n", result );
          }
        mbuffer[pos] ^= mask;
        }
      }
    delete[] buffer2;
    if( !compare_member( mbuffer, msize, dictionary_size, mpos + pos, md5_orig ) )
      internal_error( "Some byte was not properly restored." );
    delete[] mbuffer;
    }

  if( verbosity >= 0 )
    {
    std::printf( "\n%9ld bytes tested\n%9ld total decompressions"
                 "\n%9ld decompressions returned with zero status",
                 positions, decompressions, successes );
    if( successes > 0 )
      {
      if( failed_comparisons > 0 )
        std::printf( ", of which\n%9ld comparisons failed\n",
                     failed_comparisons );
      else std::fputs( "\n          all comparisons passed\n", stdout );
      }
    else std::fputc( '\n', stdout );
    }
  return 0;
  }


/* Test zeroed blocks of given size in LZMA streams in file.
   Unless verbosity >= 1, print only the bytes with interesting results. */
int lunzcrash_block( const char * const input_filename,
                     const Cl_options & cl_opts, const int sector_size )
  {
  struct stat in_stats;				// not used
  const int infd = open_instream( input_filename, &in_stats, false, true );
  if( infd < 0 ) return 1;

  const Lzip_index lzip_index( infd, cl_opts );
  if( lzip_index.retval() != 0 )
    { show_file_error( input_filename, lzip_index.error().c_str() );
      return lzip_index.retval(); }
  if( verbosity >= 2 ) printf( "Testing file '%s'\n", input_filename );

  const long long cdata_size = lzip_index.cdata_size();
  long decompressions = 0, successes = 0, failed_comparisons = 0;
  int pct = ( cdata_size >= 1000 && isatty( STDERR_FILENO ) ) ? 0 : 100;
  uint8_t * const block = new uint8_t[sector_size];
  for( long i = 0; i < lzip_index.members(); ++i )
    {
    const long long mpos = lzip_index.mblock( i ).pos();
    const long long msize = lzip_index.mblock( i ).size();
    // skip members with LZMA stream smaller than sector_size
    if( msize - Lzip_header::size - 1 - 20 <= sector_size ) continue;
    uint8_t * const mbuffer = read_member( infd, mpos, msize, input_filename );
    if( !mbuffer ) return 1;
    const unsigned dictionary_size = lzip_index.dictionary_size( i );
    md5_type md5_orig;
    if( !check_member( mbuffer, msize, dictionary_size, input_filename,
                       md5_orig ) ) return 2;
    long pct_pos = next_pct_pos( lzip_index, i, pct, sector_size );
    long pos = Lzip_header::size + 1;
    const long end = msize - sector_size - 20;
    if( verbosity >= 0 )	// give a clue of the range being tested
      std::printf( "Testing blocks of size %u from pos %llu to %llu\n",
                   sector_size, mpos + pos, mpos + end - 1 );
    LZ_mtester master( mbuffer, msize, dictionary_size );
    uint8_t * const buffer2 = new uint8_t[dictionary_size];
    for( ; pos < end; ++pos )
      {
      const long pos_limit = pos - 16;
      if( pos_limit > 0 && master.test_member( pos_limit ) != -1 )
        { show_error( "Can't advance master." ); return 1; }
      if( verbosity >= 0 && pos >= pct_pos )
        { std::fprintf( stderr, "\r%3u%% done\r", pct ); ++pct;
          pct_pos = next_pct_pos( lzip_index, i, pct, sector_size ); }
      std::memcpy( block, mbuffer + pos, sector_size );		// save block
      std::memset( mbuffer + pos, 0, sector_size );
      ++decompressions;
      long failure_pos = 0;
      const int result =
        test_member_rest( master, buffer2, &failure_pos, mpos + pos );
      if( result <= 0 )
        {
        ++successes;
        if( verbosity >= 0 )
          {
          std::printf( "block %llu,%u passed the test%s",
                       mpos + pos, sector_size, ( result < 0 ) ? "" : "\n" );
          if( result < 0 )
            std::printf( ", but only consumed %lu bytes of %llu\n",
                         failure_pos, msize );
          }
        if( !compare_member( mbuffer, msize, dictionary_size, mpos + pos,
                             md5_orig ) ) ++failed_comparisons;
        }
      else if( result == 1 )
        {
        if( verbosity >= 3 ||
            ( verbosity >= 2 && failure_pos - pos >= sector_size ) ||
            ( verbosity >= 1 && failure_pos - pos >= 10000 ) ||
            ( verbosity >= 0 && failure_pos - pos >= 50000 ) )
          std::printf( "block %llu,%u\nDecoder error at pos %llu\n",
                       mpos + pos, sector_size, mpos + failure_pos );
        }
      else if( result == 3 || result == 4 )	// test_member printed the error
        {}
      else if( verbosity >= 0 )
        {
        std::printf( "block %llu,%u\n", mpos + pos, sector_size );
        if( result == 2 )
          std::printf( "File ends unexpectedly at pos %llu\n",
                       mpos + failure_pos );
        else
          std::printf( "Unknown error code '%d'\n", result );
        }
      std::memcpy( mbuffer + pos, block, sector_size );		// restore block
      }
    delete[] buffer2;
    if( !compare_member( mbuffer, msize, dictionary_size, mpos + pos, md5_orig ) )
      internal_error( "Block was not properly restored." );
    delete[] mbuffer;
    }
  delete[] block;

  if( verbosity >= 0 )
    {
    std::printf( "\n%9ld blocks tested\n%9ld total decompressions"
                 "\n%9ld decompressions returned with zero status",
                 decompressions, decompressions, successes );
    if( successes > 0 )
      {
      if( failed_comparisons > 0 )
        std::printf( ", of which\n%9ld comparisons failed\n",
                     failed_comparisons );
      else std::fputs( "\n          all comparisons passed\n", stdout );
      }
    else std::fputc( '\n', stdout );
    }
  return 0;
  }


int md5sum_files( const std::vector< std::string > & filenames )
  {
  int retval = 0;
  bool stdin_used = false;

  for( unsigned i = 0; i < filenames.size(); ++i )
    {
    const bool from_stdin = ( filenames[i] == "-" );
    if( from_stdin ) { if( stdin_used ) continue; else stdin_used = true; }
    const char * const input_filename = filenames[i].c_str();
    struct stat in_stats;				// not used
    const int infd = from_stdin ? STDIN_FILENO :
      open_instream( input_filename, &in_stats, false );
    if( infd < 0 ) { set_retval( retval, 1 ); continue; }

    enum { buffer_size = 16384 };
    uint8_t buffer[buffer_size];
    md5_type md5_digest;
    MD5SUM md5sum;
    while( true )
      {
      const int len = readblock( infd, buffer, buffer_size );
      if( len != buffer_size && errno ) throw Error( "Read error" );
      if( len > 0 ) md5sum.md5_update( buffer, len );
      if( len < buffer_size ) break;
      }
    md5sum.md5_finish( md5_digest );
    if( close( infd ) != 0 )
      { show_file_error( input_filename, "Error closing input file", errno );
        return 1; }

    for( int i = 0; i < 16; ++i ) std::printf( "%02x", md5_digest[i] );
    std::printf( "  %s\n", input_filename );
    std::fflush( stdout );
    }
  return retval;
  }
