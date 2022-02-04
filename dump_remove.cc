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
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>

#include "lzip.h"
#include "lzip_index.h"


// If strip is false, dump to outfd members/gaps/tdata in member_list.
// If strip is true, dump to outfd members/gaps/tdata not in member_list.
int dump_members( const std::vector< std::string > & filenames,
                  const std::string & default_output_filename,
                  const Member_list & member_list, const bool force,
                  bool ignore_errors, bool ignore_trailing,
                  const bool loose_trailing, const bool strip,
                  const bool to_stdout )
  {
  if( to_stdout || default_output_filename.empty() ) outfd = STDOUT_FILENO;
  else
    {
    output_filename = default_output_filename;
    set_signal_handler();
    if( !open_outstream( force, false, false, false ) ) return 1;
    }
  if( ( strip || !member_list.tdata || member_list.damaged || member_list.range() ) &&
      !check_tty_out() ) return 1;	// check tty except for --dump=tdata
  unsigned long long copied_size = 0, stripped_size = 0;
  unsigned long long copied_tsize = 0, stripped_tsize = 0;
  long members = 0, smembers = 0;
  int files = 0, tfiles = 0, retval = 0;
  if( member_list.damaged ) ignore_errors = true;
  if( member_list.tdata ) ignore_trailing = true;
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

    const Lzip_index lzip_index( infd, ignore_trailing, loose_trailing,
                                 ignore_errors, ignore_errors );
    if( lzip_index.retval() != 0 )
      {
      show_file_error( input_filename, lzip_index.error().c_str() );
      set_retval( retval, lzip_index.retval() );
      close( infd );
      continue;
      }
    if( !safe_seek( infd, 0 ) ) cleanup_and_fail( 1 );
    const long blocks = lzip_index.blocks( false );	// not counting tdata
    long long stream_pos = 0;		// first pos not yet read from file
    long gaps = 0;
    const long prev_members = members, prev_smembers = smembers;
    const unsigned long long prev_stripped_size = stripped_size;
    for( long j = 0; j < lzip_index.members(); ++j )	// copy members and gaps
      {
      const Block & mb = lzip_index.mblock( j );
      if( mb.pos() > stream_pos )				// gap
        {
        const bool in = member_list.damaged ||
                        member_list.includes( j + gaps, blocks );
        if( in == !strip )
          {
          if( !safe_seek( infd, stream_pos ) ||
              !copy_file( infd, outfd, mb.pos() - stream_pos ) )
            cleanup_and_fail( 1 );
          copied_size += mb.pos() - stream_pos; ++members;
          }
        else { stripped_size += mb.pos() - stream_pos; ++smembers; }
        ++gaps;
        }
      bool in = member_list.includes( j + gaps, blocks );	// member
      if( !in && member_list.damaged )
        {
        if( !safe_seek( infd, mb.pos() ) ) cleanup_and_fail( 1 );
        in = ( test_member_from_file( infd, mb.size() ) != 0 );	// damaged
        }
      if( in == !strip )
        {
        if( !safe_seek( infd, mb.pos() ) ||
            !copy_file( infd, outfd, mb.size() ) ) cleanup_and_fail( 1 );
        copied_size += mb.size(); ++members;
        }
      else { stripped_size += mb.size(); ++smembers; }
      stream_pos = mb.end();
      }
    if( strip && members == prev_members )	// all members were stripped
      { if( verbosity >= 1 )
          show_file_error( input_filename, "All members stripped, skipping." );
        stripped_size = prev_stripped_size; smembers = prev_smembers;
        close( infd ); continue; }
    if( ( !strip && members > prev_members ) ||
        ( strip && smembers > prev_smembers ) ) ++files;
    // copy trailing data
    const unsigned long long cdata_size = lzip_index.cdata_size();
    const long long trailing_size = lzip_index.file_size() - cdata_size;
    if( member_list.tdata == !strip && trailing_size > 0 &&
        ( !strip || i + 1 >= filenames.size() ) )	// strip all but last
      {
      if( !safe_seek( infd, cdata_size ) ||
          !copy_file( infd, outfd, trailing_size ) ) cleanup_and_fail( 1 );
      copied_tsize += trailing_size;
      }
    else if( trailing_size > 0 ) { stripped_tsize += trailing_size; ++tfiles; }
    close( infd );
    }
  if( close_outstream( 0 ) != 0 ) set_retval( retval, 1 );
  if( verbosity >= 1 )
    {
    if( !strip )
      {
      if( member_list.damaged || member_list.range() )
        std::fprintf( stderr, "%llu bytes dumped from %ld %s from %d %s.\n",
                      copied_size,
                      members, ( members == 1 ) ? "member" : "members",
                      files, ( files == 1 ) ? "file" : "files" );
      if( member_list.tdata )
        std::fprintf( stderr, "%llu trailing bytes dumped.\n", copied_tsize );
      }
    else
      {
      if( member_list.damaged || member_list.range() )
        std::fprintf( stderr, "%llu bytes stripped from %ld %s from %d %s.\n",
                      stripped_size,
                      smembers, ( smembers == 1 ) ? "member" : "members",
                      files, ( files == 1 ) ? "file" : "files" );
      if( member_list.tdata )
        std::fprintf( stderr, "%llu trailing bytes stripped from %d %s.\n",
                      stripped_tsize, tfiles, ( tfiles == 1 ) ? "file" : "files" );
      }
    }
  return retval;
  }


int remove_members( const std::vector< std::string > & filenames,
                    const Member_list & member_list, bool ignore_errors,
                    bool ignore_trailing, const bool loose_trailing )
  {
  unsigned long long removed_size = 0, removed_tsize = 0;
  long members = 0;
  int files = 0, tfiles = 0, retval = 0;
  if( member_list.damaged ) ignore_errors = true;
  if( member_list.tdata ) ignore_trailing = true;
  for( unsigned i = 0; i < filenames.size(); ++i )
    {
    const char * const filename = filenames[i].c_str();
    struct stat in_stats, dummy_stats;
    const int infd = open_instream( filename, &in_stats, false, true );
    if( infd < 0 ) { set_retval( retval, 1 ); continue; }

    const Lzip_index lzip_index( infd, ignore_trailing, loose_trailing,
                                 ignore_errors, ignore_errors );
    if( lzip_index.retval() != 0 )
      {
      show_file_error( filename, lzip_index.error().c_str() );
      set_retval( retval, lzip_index.retval() );
      close( infd );
      continue;
      }
    const int fd = open_truncable_stream( filename, &dummy_stats );
    if( fd < 0 ) { close( infd ); set_retval( retval, 1 ); continue; }

    if( !safe_seek( infd, 0 ) ) return 1;
    const long blocks = lzip_index.blocks( false );	// not counting tdata
    long long stream_pos = 0;		// first pos not yet written to file
    long gaps = 0;
    bool error = false;
    const long prev_members = members;
    for( long j = 0; j < lzip_index.members(); ++j )	// copy members and gaps
      {
      const Block & mb = lzip_index.mblock( j );
      const long long prev_end = (j > 0) ? lzip_index.mblock(j - 1).end() : 0;
      if( mb.pos() > prev_end )					// gap
        {
        if( !member_list.damaged && !member_list.includes( j + gaps, blocks ) )
          {
          if( stream_pos != prev_end &&
              ( !safe_seek( infd, prev_end ) ||
                !safe_seek( fd, stream_pos ) ||
                !copy_file( infd, fd, mb.pos() - prev_end ) ) )
            { error = true; set_retval( retval, 1 ); break; }
          stream_pos += mb.pos() - prev_end;
          }
        else ++members;
        ++gaps;
        }
      bool in = member_list.includes( j + gaps, blocks );	// member
      if( !in && member_list.damaged )
        {
        if( !safe_seek( infd, mb.pos() ) )
          { error = true; set_retval( retval, 1 ); break; }
        in = ( test_member_from_file( infd, mb.size() ) != 0 );	// damaged
        }
      if( !in )
        {
        if( stream_pos != mb.pos() &&
            ( !safe_seek( infd, mb.pos() ) ||
              !safe_seek( fd, stream_pos ) ||
              !copy_file( infd, fd, mb.size() ) ) )
          { error = true; set_retval( retval, 1 ); break; }
        stream_pos += mb.size();
        }
      else ++members;
      }
    if( error ) { close( fd ); close( infd ); break; }
    if( stream_pos == 0 )			// all members were removed
      { show_file_error( filename, "All members would be removed, skipping." );
        close( fd ); close( infd ); set_retval( retval, 2 );
        members = prev_members; continue; }
    const long long cdata_size = lzip_index.cdata_size();
    if( cdata_size > stream_pos )
      { removed_size += cdata_size - stream_pos; ++files; }
    const long long file_size = lzip_index.file_size();
    const long long trailing_size = file_size - cdata_size;
    if( trailing_size > 0 )
      {
      if( !member_list.tdata )	// copy trailing data
        {
        if( stream_pos != cdata_size &&
            ( !safe_seek( infd, cdata_size ) ||
              !safe_seek( fd, stream_pos ) ||
              !copy_file( infd, fd, trailing_size ) ) )
          { close( fd ); close( infd ); set_retval( retval, 1 ); break; }
        stream_pos += trailing_size;
        }
      else { removed_tsize += trailing_size; ++tfiles; }
      }
    if( stream_pos >= file_size )		// no members were removed
      { close( fd ); close( infd ); continue; }
    int result;
    do result = ftruncate( fd, stream_pos );
      while( result != 0 && errno == EINTR );
    if( result != 0 )
      {
      show_file_error( filename, "Can't truncate file", errno );
      close( fd ); close( infd ); set_retval( retval, 1 ); break;
      }
    if( close( fd ) != 0 || close( infd ) != 0 )
      {
      show_file_error( filename, "Error closing file", errno );
      set_retval( retval, 1 ); break;
      }
    struct utimbuf t;
    t.actime = in_stats.st_atime;
    t.modtime = in_stats.st_mtime;
    utime( filename, &t );
    }
  if( verbosity >= 1 )
    {
    if( member_list.damaged || member_list.range() )
      std::fprintf( stderr, "%llu bytes removed from %ld %s from %d %s.\n",
                    removed_size,
                    members, ( members == 1 ) ? "member" : "members",
                    files, ( files == 1 ) ? "file" : "files" );
    if( member_list.tdata )
      std::fprintf( stderr, "%llu trailing bytes removed from %d %s.\n",
                    removed_tsize, tfiles, ( tfiles == 1 ) ? "file" : "files" );
    }
  return retval;
  }
