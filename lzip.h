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

class State
  {
  int st;

public:
  enum { states = 12 };
  State() : st( 0 ) {}
  int operator()() const { return st; }
  bool is_char() const { return st < 7; }

  void set_char()
    {
    static const int next[states] = { 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5 };
    st = next[st];
    }
  bool is_char_set_char()
    {
    if( st < 7 ) { st -= ( st < 4 ) ? st : 3; return true; }
    else { st -= ( st < 10 ) ? 3 : 6; return false; }
    }
  void set_match()     { st = ( st < 7 ) ? 7 : 10; }
  void set_rep()       { st = ( st < 7 ) ? 8 : 11; }
  void set_short_rep() { st = ( st < 7 ) ? 9 : 11; }
  };


enum {
  min_dictionary_bits = 12,
  min_dictionary_size = 1 << min_dictionary_bits,	// >= modeled_distances
  max_dictionary_bits = 29,
  max_dictionary_size = 1 << max_dictionary_bits,
  min_member_size = 36,
  literal_context_bits = 3,
  literal_pos_state_bits = 0,				// not used
  pos_state_bits = 2,
  pos_states = 1 << pos_state_bits,
  pos_state_mask = pos_states - 1,

  len_states = 4,
  dis_slot_bits = 6,
  start_dis_model = 4,
  end_dis_model = 14,
  modeled_distances = 1 << (end_dis_model / 2),		// 128
  dis_align_bits = 4,
  dis_align_size = 1 << dis_align_bits,

  len_low_bits = 3,
  len_mid_bits = 3,
  len_high_bits = 8,
  len_low_symbols = 1 << len_low_bits,
  len_mid_symbols = 1 << len_mid_bits,
  len_high_symbols = 1 << len_high_bits,
  max_len_symbols = len_low_symbols + len_mid_symbols + len_high_symbols,

  min_match_len = 2,					// must be 2
  max_match_len = min_match_len + max_len_symbols - 1,	// 273
  min_match_len_limit = 5 };

inline int get_len_state( const int len )
  { return std::min( len - min_match_len, len_states - 1 ); }

inline int get_lit_state( const uint8_t prev_byte )
  { return ( prev_byte >> ( 8 - literal_context_bits ) ); }


enum { bit_model_move_bits = 5,
       bit_model_total_bits = 11,
       bit_model_total = 1 << bit_model_total_bits };

struct Bit_model
  {
  int probability;
  Bit_model() : probability( bit_model_total / 2 ) {}
  };

struct Len_model
  {
  Bit_model choice1;
  Bit_model choice2;
  Bit_model bm_low[pos_states][len_low_symbols];
  Bit_model bm_mid[pos_states][len_mid_symbols];
  Bit_model bm_high[len_high_symbols];
  };


// defined in main.cc
extern int verbosity;

class Pretty_print		// requires global var 'int verbosity'
  {
  std::string name_;
  std::string padded_name;
  const char * const stdin_name;
  unsigned longest_name;
  mutable bool first_post;

public:
  Pretty_print( const std::vector< std::string > & filenames )
    : stdin_name( "(stdin)" ), longest_name( 0 ), first_post( false )
    {
    if( verbosity <= 0 ) return;
    const unsigned stdin_name_len = std::strlen( stdin_name );
    for( unsigned i = 0; i < filenames.size(); ++i )
      {
      const std::string & s = filenames[i];
      const unsigned len = ( s == "-" ) ? stdin_name_len : s.size();
      if( longest_name < len ) longest_name = len;
      }
    if( longest_name == 0 ) longest_name = stdin_name_len;
    }

  Pretty_print( const std::string & filename )
    : stdin_name( "(stdin)" ), first_post( false )
    {
    const unsigned stdin_name_len = std::strlen( stdin_name );
    longest_name = ( filename == "-" ) ? stdin_name_len : filename.size();
    if( longest_name == 0 ) longest_name = stdin_name_len;
    set_name( filename );
    }

  void set_name( const std::string & filename )
    {
    if( filename.size() && filename != "-" ) name_ = filename;
    else name_ = stdin_name;
    padded_name = "  "; padded_name += name_; padded_name += ": ";
    if( longest_name > name_.size() )
      padded_name.append( longest_name - name_.size(), ' ' );
    first_post = true;
    }

  void reset() const { if( name_.size() ) first_post = true; }
  const char * name() const { return name_.c_str(); }
  void operator()( const char * const msg = 0, FILE * const f = stderr ) const;
  };


class CRC32
  {
  uint32_t data[256];		// Table of CRCs of all 8-bit messages.

public:
  CRC32()
    {
    for( unsigned n = 0; n < 256; ++n )
      {
      unsigned c = n;
      for( int k = 0; k < 8; ++k )
        { if( c & 1 ) c = 0xEDB88320U ^ ( c >> 1 ); else c >>= 1; }
      data[n] = c;
      }
    }

  uint32_t operator[]( const uint8_t byte ) const { return data[byte]; }

  void update_byte( uint32_t & crc, const uint8_t byte ) const
    { crc = data[(crc^byte)&0xFF] ^ ( crc >> 8 ); }

  void update_buf( uint32_t & crc, const uint8_t * const buffer,
                   const int size ) const
    {
    uint32_t c = crc;
    for( int i = 0; i < size; ++i )
      c = data[(c^buffer[i])&0xFF] ^ ( c >> 8 );
    crc = c;
    }
  };

extern const CRC32 crc32;


inline bool isvalid_ds( const unsigned dictionary_size )
  { return ( dictionary_size >= min_dictionary_size &&
             dictionary_size <= max_dictionary_size ); }


inline int real_bits( unsigned value )
  {
  int bits = 0;
  while( value > 0 ) { value >>= 1; ++bits; }
  return bits;
  }


const uint8_t lzip_magic[4] = { 0x4C, 0x5A, 0x49, 0x50 };	// "LZIP"

struct Lzip_header
  {
  uint8_t data[6];			// 0-3 magic bytes
					//   4 version
					//   5 coded_dict_size
  enum { size = 6 };

  void set_magic() { std::memcpy( data, lzip_magic, 4 ); data[4] = 1; }
  bool verify_magic() const
    { return ( std::memcmp( data, lzip_magic, 4 ) == 0 ); }

  bool verify_prefix( const int sz ) const	// detect (truncated) header
    {
    for( int i = 0; i < sz && i < 4; ++i )
      if( data[i] != lzip_magic[i] ) return false;
    return ( sz > 0 );
    }
  bool verify_corrupt() const			// detect corrupt header
    {
    int matches = 0;
    for( int i = 0; i < 4; ++i )
      if( data[i] == lzip_magic[i] ) ++matches;
    return ( matches > 1 && matches < 4 );
    }

  uint8_t version() const { return data[4]; }
  bool verify_version() const { return ( data[4] == 1 ); }

  unsigned dictionary_size() const
    {
    unsigned sz = ( 1 << ( data[5] & 0x1F ) );
    if( sz > min_dictionary_size )
      sz -= ( sz / 16 ) * ( ( data[5] >> 5 ) & 7 );
    return sz;
    }

  bool dictionary_size( const unsigned sz )
    {
    if( !isvalid_ds( sz ) ) return false;
    data[5] = real_bits( sz - 1 );
    if( sz > min_dictionary_size )
      {
      const unsigned base_size = 1 << data[5];
      const unsigned fraction = base_size / 16;
      for( unsigned i = 7; i >= 1; --i )
        if( base_size - ( i * fraction ) >= sz )
          { data[5] |= ( i << 5 ); break; }
      }
    return true;
    }
  };


struct Lzip_trailer
  {
  uint8_t data[20];	//  0-3  CRC32 of the uncompressed data
			//  4-11 size of the uncompressed data
			// 12-19 member size including header and trailer
  enum { size = 20 };

  unsigned data_crc() const
    {
    unsigned tmp = 0;
    for( int i = 3; i >= 0; --i ) { tmp <<= 8; tmp += data[i]; }
    return tmp;
    }

  void data_crc( unsigned crc )
    { for( int i = 0; i <= 3; ++i ) { data[i] = (uint8_t)crc; crc >>= 8; } }

  unsigned long long data_size() const
    {
    unsigned long long tmp = 0;
    for( int i = 11; i >= 4; --i ) { tmp <<= 8; tmp += data[i]; }
    return tmp;
    }

  void data_size( unsigned long long sz )
    { for( int i = 4; i <= 11; ++i ) { data[i] = (uint8_t)sz; sz >>= 8; } }

  unsigned long long member_size() const
    {
    unsigned long long tmp = 0;
    for( int i = 19; i >= 12; --i ) { tmp <<= 8; tmp += data[i]; }
    return tmp;
    }

  void member_size( unsigned long long sz )
    { for( int i = 12; i <= 19; ++i ) { data[i] = (uint8_t)sz; sz >>= 8; } }

  bool verify_consistency() const	// check internal consistency
    {
    const unsigned crc = data_crc();
    const unsigned long long dsize = data_size();
    if( ( crc == 0 ) != ( dsize == 0 ) ) return false;
    const unsigned long long msize = member_size();
    if( msize < min_member_size ) return false;
    const unsigned long long mlimit = ( 9 * dsize + 7 ) / 8 + min_member_size;
    if( mlimit > dsize && msize > mlimit ) return false;
    const unsigned long long dlimit = 7090 * ( msize - 26 ) - 1;
    if( dlimit > msize && dsize > dlimit ) return false;
    return true;
    }
  };


struct Bad_byte
  {
  enum Mode { literal, delta, flip };
  long long pos;
  Mode mode;
  uint8_t value;

  Bad_byte() : pos( -1 ), mode( literal ), value( 0 ) {}
  uint8_t operator()( const uint8_t old_value ) const
    {
    if( mode == delta ) return old_value + value;
    if( mode == flip ) return old_value ^ value;
    return value;
    }
  };


#ifndef INT64_MAX
#define INT64_MAX  0x7FFFFFFFFFFFFFFFLL
#endif

class Block
  {
  long long pos_, size_;		// pos + size <= INT64_MAX

public:
  Block( const long long p, const long long s ) : pos_( p ), size_( s ) {}

  long long pos() const { return pos_; }
  long long size() const { return size_; }
  long long end() const { return pos_ + size_; }

  void pos( const long long p ) { pos_ = p; }
  void size( const long long s ) { size_ = s; }

  bool operator==( const Block & b ) const
    { return pos_ == b.pos_ && size_ == b.size_; }
  bool operator!=( const Block & b ) const
    { return pos_ != b.pos_ || size_ != b.size_; }

  bool operator<( const Block & b ) const { return pos_ < b.pos_; }

  bool includes( const long long pos ) const
    { return ( pos_ <= pos && end() > pos ); }
  bool overlaps( const Block & b ) const
    { return ( pos_ < b.end() && b.pos_ < end() ); }

  void shift( Block & b ) { ++size_; ++b.pos_; --b.size_; }
  Block split( const long long pos );
  };


struct Member_list	// members/gaps/tdata to be dumped/removed/stripped
  {
  bool damaged;
  bool tdata;
  bool in, rin;
  std::vector< Block > range_vector, rrange_vector;

  Member_list() : damaged( false ), tdata( false ), in( true ), rin( true ) {}
  void parse( const char * p );

  bool range() const { return range_vector.size() || rrange_vector.size(); }

  // blocks is the sum of members + gaps, excluding trailing data
  bool includes( const long i, const long blocks ) const
    {
    for( unsigned j = 0; j < range_vector.size(); ++j )
      {
      if( range_vector[j].pos() > i ) break;
      if( range_vector[j].end() > i ) return in;
      }
    if( i >= 0 && i < blocks )
      for( unsigned j = 0; j < rrange_vector.size(); ++j )
        {
        if( rrange_vector[j].pos() > blocks - i - 1 ) break;
        if( rrange_vector[j].end() > blocks - i - 1 ) return rin;
        }
    return !in || !rin;
    }
  };


struct Error
  {
  const char * const msg;
  explicit Error( const char * const s ) : msg( s ) {}
  };


inline unsigned long long positive_diff( const unsigned long long x,
                                         const unsigned long long y )
  { return ( ( x > y ) ? x - y : 0 ); }


const char * const bad_magic_msg = "Bad magic number (file not in lzip format).";
const char * const bad_dict_msg = "Invalid dictionary size in member header.";
const char * const corrupt_mm_msg = "Corrupt header in multimember file.";
const char * const trailing_msg = "Trailing data not allowed.";

// defined in alone_to_lz.cc
int alone_to_lz( const int infd, const Pretty_print & pp );

// defined in decoder.cc
long readblock( const int fd, uint8_t * const buf, const long size );
long writeblock( const int fd, const uint8_t * const buf, const long size );

// defined in dump_remove.cc
int dump_members( const std::vector< std::string > & filenames,
                  const std::string & default_output_filename,
                  const Member_list & member_list, const bool force,
                  bool ignore_errors, bool ignore_trailing,
                  const bool loose_trailing, const bool strip );
int remove_members( const std::vector< std::string > & filenames,
                    const Member_list & member_list, bool ignore_errors,
                    bool ignore_trailing, const bool loose_trailing );

// defined in list.cc
int list_files( const std::vector< std::string > & filenames,
                const bool ignore_errors,
                const bool ignore_trailing, const bool loose_trailing );

// defined in lzip_index.cc
int seek_read( const int fd, uint8_t * const buf, const int size,
               const long long pos );

// defined in main.cc
extern std::string output_filename;	// global vars for output file
extern int outfd;
struct stat;
const char * bad_version( const unsigned version );
const char * format_ds( const unsigned dictionary_size );
void show_header( const unsigned dictionary_size );
int open_instream( const char * const name, struct stat * const in_statsp,
                   const bool no_ofile, const bool reg_only = false );
int open_truncable_stream( const char * const name,
                           struct stat * const in_statsp );
bool open_outstream( const bool force, const bool from_stdin,
                     const bool rw = false, const bool skipping = true );
bool file_exists( const std::string & filename );
void cleanup_and_fail( const int retval );
void set_signal_handler();
int close_outstream( const struct stat * const in_statsp );
std::string insert_fixed( std::string name );
void show_error( const char * const msg, const int errcode = 0,
                 const bool help = false );
void show_file_error( const char * const filename, const char * const msg,
                      const int errcode = 0 );
void internal_error( const char * const msg );
void show_2file_error( const char * const msg1, const char * const name1,
                  const char * const name2, const char * const msg2 );
class Range_decoder;
void show_dprogress( const unsigned long long cfile_size = 0,
                     const unsigned long long partial_size = 0,
                     const Range_decoder * const d = 0,
                     const Pretty_print * const p = 0 );

// defined in merge.cc
bool copy_file( const int infd, const int outfd,
                const long long max_size = -1 );
int test_member_from_file( const int infd, const unsigned long long msize,
                           long long * const failure_posp = 0 );
int merge_files( const std::vector< std::string > & filenames,
                 const std::string & default_output_filename,
                 const bool force, const char terminator );

// defined in range_dec.cc
bool safe_seek( const int fd, const long long pos );
int range_decompress( const std::string & input_filename,
                      const std::string & default_output_filename,
                      Block range, const bool force, const bool ignore_errors,
                      const bool ignore_trailing, const bool loose_trailing,
                      const bool to_stdout );

// defined in repair.cc
int repair_file( const std::string & input_filename,
                 const std::string & default_output_filename,
                 const bool force, const char terminator );
int debug_delay( const std::string & input_filename, Block range,
                 const char terminator );
int debug_repair( const std::string & input_filename,
                  const Bad_byte & bad_byte, const char terminator );
int debug_decompress( const std::string & input_filename,
                      const Bad_byte & bad_byte, const bool show_packets );

// defined in split.cc
int split_file( const std::string & input_filename,
                const std::string & default_output_filename, const bool force );
