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

#include "lzip.h"
#include "lzip_index.h"


int seek_read( const int fd, uint8_t * const buf, const int size,
               const long long pos )
  {
  if( lseek( fd, pos, SEEK_SET ) == pos )
    return readblock( fd, buf, size );
  return 0;
  }


bool Lzip_index::check_header_error( const Lzip_header & header,
                                     const bool ignore_bad_ds )
  {
  if( !header.verify_magic() )
    { error_ = bad_magic_msg; retval_ = 2; return true; }
  if( !header.verify_version() )
    { error_ = bad_version( header.version() ); retval_ = 2; return true; }
  if( !ignore_bad_ds && !isvalid_ds( header.dictionary_size() ) )
    { error_ = bad_dict_msg; retval_ = 2; return true; }
  return false;
  }

void Lzip_index::set_errno_error( const char * const msg )
  {
  error_ = msg; error_ += std::strerror( errno );
  retval_ = 1;
  }

void Lzip_index::set_num_error( const char * const msg, unsigned long long num )
  {
  char buf[80];
  snprintf( buf, sizeof buf, "%s%llu", msg, num );
  error_ = buf;
  retval_ = 2;
  }


bool Lzip_index::read_header( const int fd, Lzip_header & header,
                              const long long pos )
  {
  if( seek_read( fd, header.data, Lzip_header::size, pos ) != Lzip_header::size )
    { set_errno_error( "Error reading member header: " ); return false; }
  return true;
  }

bool Lzip_index::read_trailer( const int fd, Lzip_trailer & trailer,
                               const long long pos )
  {
  if( seek_read( fd, trailer.data, Lzip_trailer::size,
                 pos - Lzip_trailer::size ) != Lzip_trailer::size )
    { set_errno_error( "Error reading member trailer: " ); return false; }
  return true;
  }


/* Skip backwards the gap or trailing data ending at pos.
   'ignore_gaps' also ignores format errors and a truncated last member.
   If successful, push member preceding gap and set pos to member header. */
bool Lzip_index::skip_gap( const int fd, unsigned long long & pos,
                   const bool ignore_trailing, const bool loose_trailing,
                   const bool ignore_bad_ds, const bool ignore_gaps )
  {
  if( pos < min_member_size )
    {
    if( ignore_gaps && !member_vector.empty() ) { pos = 0; return true; }
    return false;
    }
  enum { block_size = 16384,
         buffer_size = block_size + Lzip_trailer::size - 1 + Lzip_header::size };
  uint8_t buffer[buffer_size];
  int bsize = pos % block_size;			// total bytes in buffer
  if( bsize <= buffer_size - block_size ) bsize += block_size;
  int search_size = bsize;			// bytes to search for trailer
  int rd_size = bsize;				// bytes to read from file
  unsigned long long ipos = pos - rd_size;	// aligned to block_size

  while( true )
    {
    if( seek_read( fd, buffer, rd_size, ipos ) != rd_size )
      { set_errno_error( "Error seeking member trailer: " ); return false; }
    const uint8_t max_msb = ( ipos + search_size ) >> 56;
    for( int i = search_size; i >= Lzip_trailer::size; --i )
      if( buffer[i-1] <= max_msb )	// most significant byte of member_size
        {
        const Lzip_trailer & trailer =
          *(const Lzip_trailer *)( buffer + i - Lzip_trailer::size );
        const unsigned long long member_size = trailer.member_size();
        if( member_size == 0 )			// skip trailing zeros
          { while( i > Lzip_trailer::size && buffer[i-9] == 0 ) --i; continue; }
        if( member_size > ipos + i || !trailer.verify_consistency() )
          continue;
        Lzip_header header;
        if( !read_header( fd, header, ipos + i - member_size ) ) return false;
        if( !header.verify( ignore_bad_ds ) ) continue;
        const Lzip_header & header2 = *(const Lzip_header *)( buffer + i );
        const bool full_h2 = bsize - i >= Lzip_header::size;
        if( header2.verify_prefix( bsize - i ) )	// next header
          {
          if( !ignore_gaps && member_vector.empty() )	// last member
            {
            if( !full_h2 ) error_ = "Last member in input file is truncated.";
            else if( !check_header_error( header2, ignore_bad_ds ) )
              error_ = "Last member in input file is truncated or corrupt.";
            retval_ = 2; return false;
            }
          const unsigned dictionary_size =
                         full_h2 ? header2.dictionary_size() : 0;
          const unsigned long long member_size = pos - ( ipos + i );
          pos = ipos + i;
          // approximate data and member sizes for '-i -D'
          member_vector.push_back( Member( 0, member_size, pos,
                                           member_size, dictionary_size ) );
          }
        if( !ignore_gaps && member_vector.empty() )
          {
          if( !loose_trailing && full_h2 && header2.verify_corrupt() )
            { error_ = corrupt_mm_msg; retval_ = 2; return false; }
          if( !ignore_trailing )
            { error_ = trailing_msg; retval_ = 2; return false; }
          }
        pos = ipos + i - member_size;
        const unsigned dictionary_size = header.dictionary_size();
        member_vector.push_back( Member( 0, trailer.data_size(), pos,
                                         member_size, dictionary_size ) );
        if( dictionary_size_ < dictionary_size )
          dictionary_size_ = dictionary_size;
        return true;
        }
    if( ipos == 0 )
      {
      if( ignore_gaps && !member_vector.empty() )
        {
        const Lzip_header * header = (const Lzip_header *)buffer;
        const unsigned dictionary_size = header->dictionary_size();
        // approximate data and member sizes for '-i -D'
        member_vector.push_back( Member( 0, pos, 0, pos, dictionary_size ) );
        pos = 0; return true;
        }
      set_num_error( "Bad trailer at pos ", pos - Lzip_trailer::size );
      return false;
      }
    bsize = buffer_size;
    search_size = bsize - Lzip_header::size;
    rd_size = block_size;
    ipos -= rd_size;
    std::memcpy( buffer + rd_size, buffer, buffer_size - rd_size );
    }
  }


Lzip_index::Lzip_index( const int infd, const bool ignore_trailing,
                        const bool loose_trailing, const bool ignore_bad_ds,
                        const bool ignore_gaps, const long long max_pos )
  : insize( lseek( infd, 0, SEEK_END ) ), retval_( 0 ), dictionary_size_( 0 )
  {
  if( insize < 0 )
    { set_errno_error( "Input file is not seekable: " ); return; }
  if( insize < min_member_size )
    { error_ = "Input file is too short."; retval_ = 2; return; }
  if( insize > INT64_MAX )
    { error_ = "Input file is too long (2^63 bytes or more).";
      retval_ = 2; return; }

  Lzip_header header;
  if( !read_header( infd, header, 0 ) ) return;
  if( check_header_error( header, ignore_bad_ds ) ) return;

  // pos always points to a header or to ( EOF || max_pos )
  unsigned long long pos = ( max_pos > 0 ) ? max_pos : insize;
  while( pos >= min_member_size )
    {
    Lzip_trailer trailer;
    if( !read_trailer( infd, trailer, pos ) ) break;
    const unsigned long long member_size = trailer.member_size();
    // if gaps are being ignored, verify consistency of last trailer only.
    if( member_size > pos || member_size < min_member_size ||
        ( ( !ignore_gaps || member_vector.empty() ) &&
        !trailer.verify_consistency() ) )		// bad trailer
      {
      if( ignore_gaps || member_vector.empty() )
        { if( skip_gap( infd, pos, ignore_trailing, loose_trailing,
              ignore_bad_ds, ignore_gaps ) ) continue; else return; }
      set_num_error( "Bad trailer at pos ", pos - Lzip_trailer::size );
      break;
      }
    if( !read_header( infd, header, pos - member_size ) ) break;
    if( !header.verify( ignore_bad_ds ) )		// bad header
      {
      if( ignore_gaps || member_vector.empty() )
        { if( skip_gap( infd, pos, ignore_trailing, loose_trailing,
              ignore_bad_ds, ignore_gaps ) ) continue; else return; }
      set_num_error( "Bad header at pos ", pos - member_size );
      break;
      }
    pos -= member_size;
    const unsigned dictionary_size = header.dictionary_size();
    member_vector.push_back( Member( 0, trailer.data_size(), pos,
                                     member_size, dictionary_size ) );
    if( dictionary_size_ < dictionary_size )
      dictionary_size_ = dictionary_size;
    }
  // block at pos == 0 must be a member unless shorter than min_member_size
  if( pos >= min_member_size || ( pos != 0 && !ignore_gaps ) ||
      member_vector.empty() )
    {
    member_vector.clear();
    if( retval_ == 0 ) { error_ = "Can't create file index."; retval_ = 2; }
    return;
    }
  std::reverse( member_vector.begin(), member_vector.end() );
  for( unsigned long i = 0; ; ++i )
    {
    const long long end = member_vector[i].dblock.end();
    if( end < 0 || end > INT64_MAX )
      {
      member_vector.clear();
      error_ = "Data in input file is too long (2^63 bytes or more).";
      retval_ = 2; return;
      }
    if( i + 1 >= member_vector.size() ) break;
    member_vector[i+1].dblock.pos( end );
    if( member_vector[i].mblock.end() > member_vector[i+1].mblock.pos() )
      internal_error( "two mblocks overlap after constructing a Lzip_index." );
    }
  }


// All files in 'infd_vector' must be at least 'fsize' bytes long.
Lzip_index::Lzip_index( const std::vector< int > & infd_vector,
                        const long long fsize )
  : insize( fsize ), retval_( 0 ), dictionary_size_( 0 )	// DS not used
  {
  if( insize < 0 )
    { set_errno_error( "Input file is not seekable: " ); return; }
  if( insize < min_member_size )
    { error_ = "Input file is too short."; retval_ = 2; return; }
  if( insize > INT64_MAX )
    { error_ = "Input file is too long (2^63 bytes or more).";
      retval_ = 2; return; }

  const int files = infd_vector.size();
  Lzip_header header;
  bool done = false;
  for( int i = 0; i < files && !done; ++i )
    {
    const int infd = infd_vector[i];
    if( !read_header( infd, header, 0 ) ) return;
    if( header.verify_magic() && header.verify_version() ) done = true;
    }
  if( !done )
    { error_ = bad_magic_msg; retval_ = 2; return; }

  long long pos = insize;		// always points to a header or to EOF
  while( pos >= min_member_size )
    {
    unsigned long long member_size;
    Lzip_trailer trailer;
    done = false;
    for( int it = 0; it < files && !done; ++it )
      {
      const int tfd = infd_vector[it];
      if( !read_trailer( tfd, trailer, pos ) ) goto error;
      member_size = trailer.member_size();
      if( member_size <= (unsigned long long)pos && trailer.verify_consistency() )
        for( int ih = 0; ih < files && !done; ++ih )
          {
          const int hfd = infd_vector[ih];
          if( !read_header( hfd, header, pos - member_size ) ) goto error;
          if( header.verify_magic() && header.verify_version() ) done = true;
          }
      }
    if( !done )
      {
      if( member_vector.empty() ) { --pos; continue; }	// maybe trailing data
      set_num_error( "Member size in trailer may be corrupt at pos ", pos - 8 );
      break;
      }
    if( member_vector.empty() && insize > pos )
      {
      const int size = std::min( (long long)Lzip_header::size, insize - pos );
      for( int i = 0; i < files; ++i )
        {
        const int infd = infd_vector[i];
        if( seek_read( infd, header.data, size, pos ) == size &&
            header.verify_prefix( size ) )
          {
          error_ = "Last member in input file is truncated or corrupt.";
          retval_ = 2; goto error;
          }
        }
      }
    pos -= member_size;
    member_vector.push_back( Member( 0, trailer.data_size(), pos,
                                     member_size, 0 ) );
    }
error:
  if( pos != 0 || member_vector.empty() )
    {
    member_vector.clear();
    if( retval_ == 0 ) { error_ = "Can't create file index."; retval_ = 2; }
    return;
    }
  std::reverse( member_vector.begin(), member_vector.end() );
  for( unsigned long i = 0; ; ++i )
    {
    const long long end = member_vector[i].dblock.end();
    if( end < 0 || end > INT64_MAX )
      {
      member_vector.clear();
      error_ = "Data in input file is too long (2^63 bytes or more).";
      retval_ = 2; return;
      }
    if( i + 1 >= member_vector.size() ) break;
    member_vector[i+1].dblock.pos( end );
    }
  }


// Return members + gaps [+ trailing data].
long Lzip_index::blocks( const bool count_tdata ) const
  {
  long n = member_vector.size() + ( count_tdata && cdata_size() < file_size() );
  if( member_vector.size() && member_vector[0].mblock.pos() > 0 ) ++n;
  for( unsigned long i = 1; i < member_vector.size(); ++i )
    if( member_vector[i-1].mblock.end() < member_vector[i].mblock.pos() ) ++n;
  return n;
  }
