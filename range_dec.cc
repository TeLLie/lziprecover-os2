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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "lzip.h"
#include "decoder.h"
#include "lzip_index.h"


namespace {

int decompress_member( const int infd, const Pretty_print & pp,
                       const unsigned long long mpos,
                       const unsigned long long outskip,
                       const unsigned long long outend )
  {
  Range_decoder rdec( infd );
  Lzip_header header;
  rdec.read_data( header.data, Lzip_header::size );
  if( rdec.finished() )			// End Of File
    { pp( "File ends unexpectedly at member header." ); return 2; }
  if( !header.verify_magic() ) { pp( bad_magic_msg ); return 2; }
  if( !header.verify_version() )
    { pp( bad_version( header.version() ) ); return 2; }
  const unsigned dictionary_size = header.dictionary_size();
  if( !isvalid_ds( dictionary_size ) ) { pp( bad_dict_msg ); return 2; }

  if( verbosity >= 2 ) pp();

  LZ_decoder decoder( rdec, dictionary_size, outfd, outskip, outend );
  const int result = decoder.decode_member( pp );
  if( result != 0 )
    {
    if( verbosity >= 0 && result <= 2 )
      {
      pp();
      std::fprintf( stderr, "%s at pos %llu\n", ( result == 2 ) ?
                    "File ends unexpectedly" : "Decoder error",
                    mpos + rdec.member_position() );
      }
    return 2;
    }
  if( verbosity >= 2 ) std::fputs( "done\n", stderr );
  return 0;
  }


const char * format_num( unsigned long long num,
                         unsigned long long limit = -1ULL,
                         const int set_prefix = 0 )
  {
  const char * const si_prefix[8] =
    { "k", "M", "G", "T", "P", "E", "Z", "Y" };
  const char * const binary_prefix[8] =
    { "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };
  enum { buffers = 8, bufsize = 32 };
  static char buffer[buffers][bufsize];	// circle of static buffers for printf
  static int current = 0;
  static bool si = true;

  if( set_prefix ) si = ( set_prefix > 0 );
  const unsigned factor = ( si ? 1000 : 1024 );
  char * const buf = buffer[current++]; current %= buffers;
  const char * const * prefix = ( si ? si_prefix : binary_prefix );
  const char * p = "";
  bool exact = ( num % factor == 0 );

  for( int i = 0; i < 8 && ( num > limit || ( exact && num >= factor ) ); ++i )
    { num /= factor; if( num % factor != 0 ) exact = false; p = prefix[i]; }
  snprintf( buf, bufsize, "%llu %s", num, p );
  return buf;
  }

} // end namespace


bool safe_seek( const int fd, const long long pos )
  {
  if( lseek( fd, pos, SEEK_SET ) == pos ) return true;
  show_error( "Seek error", errno ); return false;
  }


int range_decompress( const std::string & input_filename,
                      const std::string & default_output_filename,
                      Block range, const bool force, const bool ignore_errors,
                      const bool ignore_trailing, const bool loose_trailing,
                      const bool to_stdout )
  {
  struct stat in_stats;
  const int infd = open_instream( input_filename.c_str(), &in_stats, true, true );
  if( infd < 0 ) return 1;

  const Lzip_index lzip_index( infd, ignore_trailing, loose_trailing,
                               ignore_errors, ignore_errors );
  if( lzip_index.retval() != 0 )
    { show_file_error( input_filename.c_str(), lzip_index.error().c_str() );
      return lzip_index.retval(); }

  if( range.end() > lzip_index.udata_size() )
    range.size( std::max( 0LL, lzip_index.udata_size() - range.pos() ) );
  if( range.size() <= 0 )
    { show_file_error( input_filename.c_str(), "Nothing to do." ); return 0; }

  if( to_stdout || default_output_filename.empty() )
    outfd = STDOUT_FILENO;
  else
    {
    output_filename = default_output_filename;
    set_signal_handler();
    if( !open_outstream( force, false, false, false ) )
      { close( infd ); return 1; }
    }

  if( verbosity >= 1 )
    std::fprintf( stderr, "Decompressing range %sB to %sB (%sof %sBytes)\n",
                  format_num( range.pos() ),
                  format_num( range.pos() + range.size() ),
                  format_num( range.size() ),
                  format_num( lzip_index.udata_size() ) );

  Pretty_print pp( input_filename );
  int retval = 0;
  for( long i = 0; i < lzip_index.members(); ++i )
    {
    const Block & db = lzip_index.dblock( i );
    if( range.overlaps( db ) )
      {
      if( verbosity >= 3 && lzip_index.members() > 1 )
        std::fprintf( stderr, "Decompressing member %3ld\n", i + 1 );
      const long long outskip = std::max( 0LL, range.pos() - db.pos() );
      const long long outend = std::min( db.size(), range.end() - db.pos() );
      const long long mpos = lzip_index.mblock( i ).pos();
      if( !safe_seek( infd, mpos ) ) { retval = 1; break; }
      const int tmp = decompress_member( infd, pp, mpos, outskip, outend );
      if( tmp && ( tmp != 2 || !ignore_errors ) ) cleanup_and_fail( tmp );
      if( tmp > retval ) retval = tmp;
      pp.reset();
      }
    }
  close( infd );
  retval = std::max( retval, close_outstream( &in_stats ) );
  if( verbosity >= 2 && retval == 0 )
    std::fputs( "Byte range decompressed successfully.\n", stderr );
  return retval;
  }
