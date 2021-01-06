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

class Lzip_index
  {
  struct Member
    {
    Block dblock, mblock;		// data block, member block
    unsigned dictionary_size;

    Member( const long long dp, const long long ds,
            const long long mp, const long long ms, const unsigned dict_size )
      : dblock( dp, ds ), mblock( mp, ms ), dictionary_size( dict_size ) {}

    bool operator==( const Member & m ) const { return ( mblock == m.mblock ); }
    bool operator!=( const Member & m ) const { return ( mblock != m.mblock ); }
    };

  // member_vector only contains members with a valid header.
  // Garbage between members is represented by gaps between mblocks.
  std::vector< Member > member_vector;
  std::string error_;
  long long insize;
  int retval_;
  unsigned dictionary_size_;	// largest dictionary size in the file

  bool check_header_error( const Lzip_header & header,
                           const bool ignore_bad_ds );
  void set_errno_error( const char * const msg );
  void set_num_error( const char * const msg, unsigned long long num );
  bool read_header( const int fd, Lzip_header & header, const long long pos );
  bool read_trailer( const int fd, Lzip_trailer & trailer,
                     const long long pos );
  bool skip_gap( const int fd, unsigned long long & pos,
                 const bool ignore_trailing, const bool loose_trailing,
                 const bool ignore_bad_ds, const bool ignore_gaps );

public:
  Lzip_index()
    : error_( "No index" ), insize( 0 ), retval_( 2 ), dictionary_size_( 0 ) {}
  Lzip_index( const int infd, const bool ignore_trailing,
              const bool loose_trailing, const bool ignore_bad_ds = false,
              const bool ignore_gaps = false, const long long max_pos = 0 );
  Lzip_index( const std::vector< int > & infd_vector, const long long fsize );

  long members() const { return member_vector.size(); }
  long blocks( const bool count_tdata ) const;	// members + gaps [+ tdata]
  const std::string & error() const { return error_; }
  int retval() const { return retval_; }
  unsigned dictionary_size() const { return dictionary_size_; }

  bool operator==( const Lzip_index & li ) const
    {
    if( retval_ || li.retval_ || insize != li.insize ||
        member_vector.size() != li.member_vector.size() ) return false;
    for( unsigned long i = 0; i < member_vector.size(); ++i )
      if( member_vector[i] != li.member_vector[i] ) return false;
    return true;
    }
  bool operator!=( const Lzip_index & li ) const { return !( *this == li ); }

  long long udata_size() const
    { if( member_vector.empty() ) return 0;
      return member_vector.back().dblock.end(); }

  long long cdata_size() const
    { if( member_vector.empty() ) return 0;
      return member_vector.back().mblock.end(); }

  // total size including trailing data (if any)
  long long file_size() const
    { if( insize >= 0 ) return insize; else return 0; }

  const Block & dblock( const long i ) const
    { return member_vector[i].dblock; }
  const Block & mblock( const long i ) const
    { return member_vector[i].mblock; }
  unsigned dictionary_size( const long i ) const
    { return member_vector[i].dictionary_size; }
  };
