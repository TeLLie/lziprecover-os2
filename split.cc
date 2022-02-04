/* Lziprecover - Data recovery tool for the lzip format
   Copyright (C) 2009-2022 Antonio Diaz Diaz.

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
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "lzip.h"
#include "lzip_index.h"


namespace {

void first_filename( const std::string & input_filename,
                     const std::string & default_output_filename,
                     const int max_digits )
  {
  output_filename = default_output_filename.empty() ?
                    input_filename : default_output_filename;
  int b = output_filename.size();
  while( b > 0 && output_filename[b-1] != '/' ) --b;
  output_filename.insert( b, "rec1" );
  if( max_digits > 1 ) output_filename.insert( b + 3, max_digits - 1, '0' );
  }


bool next_filename( const int max_digits )
  {
  if( verbosity >= 1 )
    {
    std::printf( "Member '%s' done \n", output_filename.c_str() );
    std::fflush( stdout );
    }
  int b = output_filename.size();
  while( b > 0 && output_filename[b-1] != '/' ) --b;
  for( int i = b + max_digits + 2; i > b + 2; --i )	// "rec<max_digits>"
    {
    if( output_filename[i] < '9' ) { ++output_filename[i]; return true; }
    else output_filename[i] = '0';
    }
  return false;
  }

} // end namespace


int split_file( const std::string & input_filename,
                const std::string & default_output_filename, const bool force )
  {
  struct stat in_stats;
  const int infd =
    open_instream( input_filename.c_str(), &in_stats, false, true );
  if( infd < 0 ) return 1;

  Lzip_index lzip_index( infd, true, true, true, true );
  if( lzip_index.retval() != 0 )
    {
    show_file_error( input_filename.c_str(), lzip_index.error().c_str() );
    return lzip_index.retval();
    }
  // verify last member
  const Block b = lzip_index.mblock( lzip_index.members() - 1 );
  long long mpos = b.pos();
  long long msize = b.size();
  long long failure_pos = 0;
  if( !safe_seek( infd, mpos ) ) return 1;
  if( test_member_from_file( infd, msize, &failure_pos ) == 1 )
    {						// corrupt or fake trailer
    while( true )
      {
      mpos += failure_pos; msize -= failure_pos;
      if( msize < min_member_size ) break;		// trailing data
      if( !safe_seek( infd, mpos ) ) return 1;
      if( test_member_from_file( infd, msize, &failure_pos ) != 1 ) break;
      }
    lzip_index = Lzip_index( infd, true, true, true, true, mpos );
    if( lzip_index.retval() != 0 )
      {
      show_file_error( input_filename.c_str(), lzip_index.error().c_str() );
      return lzip_index.retval();
      }
    }

  if( !safe_seek( infd, 0 ) ) return 1;
  int max_digits = 1;
  for( long i = lzip_index.blocks( true ); i >= 10; i /= 10 ) ++max_digits;
  first_filename( input_filename, default_output_filename, max_digits );

  long long stream_pos = 0;		// first pos not yet written to file
  set_signal_handler();
  for( long i = 0; i < lzip_index.members(); ++i )
    {
    const Block & mb = lzip_index.mblock( i );
    if( mb.pos() > stream_pos )					// gap
      {
      if( !open_outstream( force, true, false, false ) ) return 1;
      if( !copy_file( infd, outfd, mb.pos() - stream_pos ) ||
          close_outstream( &in_stats ) != 0 )
        cleanup_and_fail( 1 );
      next_filename( max_digits );
      }
    if( !open_outstream( force, true, false, false ) ) return 1;  // member
    if( !copy_file( infd, outfd, mb.size() ) ||
        close_outstream( &in_stats ) != 0 )
      cleanup_and_fail( 1 );
    next_filename( max_digits );
    stream_pos = mb.end();
    }
  if( lzip_index.file_size() > stream_pos )		// trailing data
    {
    if( !open_outstream( force, true, false, false ) ) return 1;
    if( !copy_file( infd, outfd, lzip_index.file_size() - stream_pos ) ||
        close_outstream( &in_stats ) != 0 )
      cleanup_and_fail( 1 );
    next_filename( max_digits );
    }
  close( infd );
  return 0;
  }
