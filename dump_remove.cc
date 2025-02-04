/* Lziprecover - Data recovery tool for the lzip format
   Copyright (C) 2009-2025 Antonio Diaz Diaz.

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


const char * const pdate_msg = "warning: can't preserve file date";


/* If strip is false, dump to outfd members/gaps/tdata in member_list.
   If strip is true, dump to outfd members/gaps/tdata not in member_list. */
int dump_members( const std::vector< std::string > & filenames,
                  const std::string & default_output_filename,
                  const Cl_options & cl_opts, const Member_list & member_list,
                  const bool force, const bool strip, const bool to_stdout )
  {
  if( to_stdout || default_output_filename.empty() ) outfd = STDOUT_FILENO;
  else
    {
    output_filename = default_output_filename;
    set_signal_handler();
    if( !open_outstream( force, false, false, false ) ) return 1;
    }
  if( ( strip || !member_list.tdata || member_list.damaged ||
        member_list.empty || member_list.range() ) &&
      !check_tty_out() ) return 1;	// check tty except for --dump=tdata
  unsigned long long copied_size = 0, stripped_size = 0;
  unsigned long long copied_tsize = 0, stripped_tsize = 0;
  long members = 0, smembers = 0;
  int files = 0, tfiles = 0, retval = 0;
  bool stdin_used = false;
  for( unsigned i = 0; i < filenames.size(); ++i )
    {
    const bool from_stdin = filenames[i] == "-";
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
    if( !safe_seek( infd, 0, input_filename ) ) cleanup_and_fail( 1 );
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
          if( !safe_seek( infd, stream_pos, input_filename ) ||
              !copy_file( infd, outfd, filenames[i], output_filename,
                          mb.pos() - stream_pos ) ) cleanup_and_fail( 1 );
          copied_size += mb.pos() - stream_pos; ++members;
          }
        else { stripped_size += mb.pos() - stream_pos; ++smembers; }
        ++gaps;
        }
      bool in = member_list.includes( j + gaps, blocks );	// member
      if( !in && member_list.empty && lzip_index.dblock( j ).size() == 0 )
        in = true;
      if( !in && member_list.damaged )
        {
        if( !safe_seek( infd, mb.pos(), input_filename ) ) cleanup_and_fail( 1 );
        in = test_member_from_file( infd, mb.size() ) != 0;	// damaged
        }
      if( in == !strip )
        {
        if( !safe_seek( infd, mb.pos(), input_filename ) ||
            !copy_file( infd, outfd, filenames[i], output_filename,
                        mb.size() ) ) cleanup_and_fail( 1 );
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
      if( !safe_seek( infd, cdata_size, input_filename ) ||
          !copy_file( infd, outfd, filenames[i], output_filename,
                      trailing_size ) ) cleanup_and_fail( 1 );
      copied_tsize += trailing_size;
      }
    else if( trailing_size > 0 ) { stripped_tsize += trailing_size; ++tfiles; }
    close( infd );
    }
  if( !close_outstream( 0 ) ) set_retval( retval, 1 );
  if( verbosity >= 1 )
    {
    if( !strip )
      {
      if( member_list.damaged || member_list.empty || member_list.range() )
        std::fprintf( stderr, "%llu bytes dumped from %ld %s from %d %s.\n",
                      copied_size,
                      members, ( members == 1 ) ? "member" : "members",
                      files, ( files == 1 ) ? "file" : "files" );
      if( member_list.tdata )
        std::fprintf( stderr, "%llu trailing bytes dumped.\n", copied_tsize );
      }
    else
      {
      if( member_list.damaged || member_list.empty || member_list.range() )
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


/* Remove members, tdata from files in place by opening two descriptors for
   each file. */
int remove_members( const std::vector< std::string > & filenames,
                  const Cl_options & cl_opts, const Member_list & member_list )
  {
  unsigned long long removed_size = 0, removed_tsize = 0;
  long members = 0;
  int files = 0, tfiles = 0, retval = 0;
  for( unsigned i = 0; i < filenames.size(); ++i )
    {
    const char * const filename = filenames[i].c_str();
    struct stat in_stats, dummy_stats;
    const int infd = open_instream( filename, &in_stats, false, true );
    if( infd < 0 ) { set_retval( retval, 1 ); continue; }

    const Lzip_index lzip_index( infd, cl_opts, cl_opts.ignore_errors,
                                 cl_opts.ignore_errors );
    if( lzip_index.retval() != 0 )
      {
      show_file_error( filename, lzip_index.error().c_str() );
      set_retval( retval, lzip_index.retval() );
      close( infd );
      continue;
      }
    const int fd = open_truncable_stream( filename, &dummy_stats );
    if( fd < 0 ) { close( infd ); set_retval( retval, 1 ); continue; }

    if( !safe_seek( infd, 0, filename ) ) return 1;
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
              ( !safe_seek( infd, prev_end, filename ) ||
                !safe_seek( fd, stream_pos, filename ) ||
                !copy_file( infd, fd, filenames[i], filenames[i],
                            mb.pos() - prev_end ) ) )
            { error = true; set_retval( retval, 1 ); break; }
          stream_pos += mb.pos() - prev_end;
          }
        else ++members;
        ++gaps;
        }
      bool in = member_list.includes( j + gaps, blocks );	// member
      if( !in && member_list.empty && lzip_index.dblock( j ).size() == 0 )
        in = true;
      if( !in && member_list.damaged )
        {
        if( !safe_seek( infd, mb.pos(), filename ) )
          { error = true; set_retval( retval, 1 ); break; }
        in = test_member_from_file( infd, mb.size() ) != 0;	// damaged
        }
      if( !in )
        {
        if( stream_pos != mb.pos() &&
            ( !safe_seek( infd, mb.pos(), filename ) ||
              !safe_seek( fd, stream_pos, filename ) ||
              !copy_file( infd, fd, filenames[i], filenames[i], mb.size() ) ) )
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
            ( !safe_seek( infd, cdata_size, filename ) ||
              !safe_seek( fd, stream_pos, filename ) ||
              !copy_file( infd, fd, filenames[i], filenames[i], trailing_size ) ) )
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
    if( utime( filename, &t ) != 0 && verbosity >= 1 )
      show_file_error( filename, pdate_msg, errno );
    }
  if( verbosity >= 1 )
    {
    if( member_list.damaged || member_list.empty || member_list.range() )
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


/* Set to zero in place the first LZMA byte of each member in each file by
   opening one rw descriptor for each file. */
int nonzero_repair( const std::vector< std::string > & filenames,
                    const Cl_options & cl_opts )
  {
  long cleared_members = 0;
  int files = 0, retval = 0;
  for( unsigned i = 0; i < filenames.size(); ++i )
    {
    const char * const filename = filenames[i].c_str();
    struct stat in_stats;
    const int fd = open_truncable_stream( filename, &in_stats );
    if( fd < 0 ) { set_retval( retval, 1 ); continue; }

    const Lzip_index lzip_index( fd, cl_opts, true, cl_opts.ignore_errors );
    if( lzip_index.retval() != 0 )
      {
      show_file_error( filename, lzip_index.error().c_str() );
      set_retval( retval, lzip_index.retval() );
      close( fd );
      continue;
      }

    enum { bufsize = Lzip_header::size + 1 };
    uint8_t header_buf[bufsize];
    const uint8_t * const p = header_buf;	// keep gcc 6.1.0 quiet
    const Lzip_header & header = *(const Lzip_header *)p;
    uint8_t * const mark = header_buf + header.size;
    bool write_attempted = false;
    for( long j = 0; j < lzip_index.members(); ++j )	// clear the members
      {
      const Block & mb = lzip_index.mblock( j );
      if( seek_read( fd, header_buf, bufsize, mb.pos() ) != bufsize )
        { show_file_error( filename, "Error reading member header", errno );
          set_retval( retval, 1 ); break; }
      if( !header.check( true ) )
        { show_file_error( filename, "Member header became corrupt as we read it." );
          set_retval( retval, 2 ); break; }
      if( *mark == 0 ) continue;
      *mark = 0; write_attempted = true;
      if( seek_write( fd, mark, 1, mb.pos() + header.size ) != 1 )
        { show_file_error( filename, "Error writing to file", errno );
          set_retval( retval, 1 ); break; }
      ++cleared_members;
      }
    if( close( fd ) != 0 )
      {
      show_file_error( filename, "Error closing file", errno );
      set_retval( retval, 1 ); break;
      }
    if( write_attempted )
      {
      struct utimbuf t;
      t.actime = in_stats.st_atime;
      t.modtime = in_stats.st_mtime;
      if( utime( filename, &t ) != 0 && verbosity >= 1 )
        show_file_error( filename, pdate_msg, errno );
      ++files;
      }
    }
  if( verbosity >= 1 )
    std::fprintf( stderr, "%lu %s cleared in %d %s.\n", cleared_members,
                  ( cleared_members == 1 ) ? "member" : "members",
                  files, ( files == 1 ) ? "file" : "files" );
  return retval;
  }
