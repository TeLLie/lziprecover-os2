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

class Range_mtester
  {
  const uint8_t * const buffer;	// input buffer
  const long buffer_size;
  long pos;			// current pos in buffer
  uint32_t code;
  uint32_t range;
  bool at_stream_end;

  void operator=( const Range_mtester & );	// declared as private

public:
  Range_mtester( const uint8_t * const buf, const long buf_size )
    :
    buffer( buf ),
    buffer_size( buf_size ),
    pos( Lzip_header::size ),
    code( 0 ),
    range( 0xFFFFFFFFU ),
    at_stream_end( false )
    {}

  bool finished() { return pos >= buffer_size; }
  unsigned long member_position() const { return pos; }

  uint8_t get_byte()
    {
    // 0xFF avoids decoder error if member is truncated at EOS marker
    if( finished() ) return 0xFF;
    return buffer[pos++];
    }

  const Lzip_trailer * get_trailer()
    {
    if( buffer_size - pos < Lzip_trailer::size ) return 0;
    const Lzip_trailer * const p = (const Lzip_trailer *)( buffer + pos );
    pos += Lzip_trailer::size;
    return p;
    }

  void load()
    {
    code = 0;
    for( int i = 0; i < 5; ++i ) code = (code << 8) | get_byte();
    range = 0xFFFFFFFFU;
    code &= range;		// make sure that first byte is discarded
    }

  void normalize()
    {
    if( range <= 0x00FFFFFFU )
      { range <<= 8; code = (code << 8) | get_byte(); }
    }

  unsigned decode( const int num_bits )
    {
    unsigned symbol = 0;
    for( int i = num_bits; i > 0; --i )
      {
      normalize();
      range >>= 1;
//      symbol <<= 1;
//      if( code >= range ) { code -= range; symbol |= 1; }
      const bool bit = ( code >= range );
      symbol = ( symbol << 1 ) + bit;
      code -= range & ( 0U - bit );
      }
    return symbol;
    }

  unsigned decode_bit( Bit_model & bm )
    {
    normalize();
    const uint32_t bound = ( range >> bit_model_total_bits ) * bm.probability;
    if( code < bound )
      {
      range = bound;
      bm.probability += (bit_model_total - bm.probability) >> bit_model_move_bits;
      return 0;
      }
    else
      {
      range -= bound;
      code -= bound;
      bm.probability -= bm.probability >> bit_model_move_bits;
      return 1;
      }
    }

  unsigned decode_tree3( Bit_model bm[] )
    {
    unsigned symbol = 1;
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    return symbol & 7;
    }

  unsigned decode_tree6( Bit_model bm[] )
    {
    unsigned symbol = 1;
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    return symbol & 0x3F;
    }

  unsigned decode_tree8( Bit_model bm[] )
    {
    unsigned symbol = 1;
    for( int i = 0; i < 8; ++i )
      symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
    return symbol & 0xFF;
    }

  unsigned decode_tree_reversed( Bit_model bm[], const int num_bits )
    {
    unsigned model = 1;
    unsigned symbol = 0;
    for( int i = 0; i < num_bits; ++i )
      {
      const unsigned bit = decode_bit( bm[model] );
      model = ( model << 1 ) + bit;
      symbol |= ( bit << i );
      }
    return symbol;
    }

  unsigned decode_tree_reversed4( Bit_model bm[] )
    {
    unsigned symbol = decode_bit( bm[1] );
    unsigned model = 2 + symbol;
    unsigned bit = decode_bit( bm[model] );
    model = ( model << 1 ) + bit; symbol |= ( bit << 1 );
    bit = decode_bit( bm[model] );
    model = ( model << 1 ) + bit; symbol |= ( bit << 2 );
    symbol |= ( decode_bit( bm[model] ) << 3 );
    return symbol;
    }

  unsigned decode_matched( Bit_model bm[], unsigned match_byte )
    {
    Bit_model * const bm1 = bm + 0x100;
    unsigned symbol = 1;
    while( symbol < 0x100 )
      {
      const unsigned match_bit = ( match_byte <<= 1 ) & 0x100;
      const unsigned bit = decode_bit( bm1[match_bit+symbol] );
      symbol = ( symbol << 1 ) | bit;
      if( match_bit != bit << 8 )
        {
        while( symbol < 0x100 )
          symbol = ( symbol << 1 ) | decode_bit( bm[symbol] );
        break;
        }
      }
    return symbol & 0xFF;
    }

  unsigned decode_len( Len_model & lm, const int pos_state )
    {
    if( decode_bit( lm.choice1 ) == 0 )
      return decode_tree3( lm.bm_low[pos_state] );
    if( decode_bit( lm.choice2 ) == 0 )
      return len_low_symbols + decode_tree3( lm.bm_mid[pos_state] );
    return len_low_symbols + len_mid_symbols + decode_tree8( lm.bm_high );
    }
  };


class LZ_mtester
  {
  unsigned long long partial_data_pos;
  Range_mtester rdec;
  const unsigned dictionary_size;
  uint8_t * buffer;		// output buffer
  unsigned pos;			// current pos in buffer
  unsigned stream_pos;		// first byte not yet written to file
  uint32_t crc_;
  const int outfd;		// output file descriptor
  unsigned rep0;		// rep[0-3] latest four distances
  unsigned rep1;		// used for efficient coding of
  unsigned rep2;		// repeated distances
  unsigned rep3;
  State state;
  unsigned max_rep0;		// maximum distance found
  bool pos_wrapped;

  Bit_model bm_literal[1<<literal_context_bits][0x300];
  Bit_model bm_match[State::states][pos_states];
  Bit_model bm_rep[State::states];
  Bit_model bm_rep0[State::states];
  Bit_model bm_rep1[State::states];
  Bit_model bm_rep2[State::states];
  Bit_model bm_len[State::states][pos_states];
  Bit_model bm_dis_slot[len_states][1<<dis_slot_bits];
  Bit_model bm_dis[modeled_distances-end_dis_model+1];
  Bit_model bm_align[dis_align_size];

  Len_model match_len_model;
  Len_model rep_len_model;

  void print_block( const int len );
  void flush_data();
  bool verify_trailer();

  uint8_t peek_prev() const
    { return buffer[((pos > 0) ? pos : dictionary_size)-1]; }

  uint8_t peek( const unsigned distance ) const
    {
    const unsigned i = ( ( pos > distance ) ? 0 : dictionary_size ) +
                       pos - distance - 1;
    return buffer[i];
    }

  void put_byte( const uint8_t b )
    {
    buffer[pos] = b;
    if( ++pos >= dictionary_size ) flush_data();
    }

  void copy_block( const unsigned distance, unsigned len )
    {
    unsigned lpos = pos, i = lpos - distance - 1;
    bool fast, fast2;
    if( lpos > distance )
      {
      fast = ( len < dictionary_size - lpos );
      fast2 = ( fast && len <= lpos - i );
      }
    else
      {
      i += dictionary_size;
      fast = ( len < dictionary_size - i );	// (i == pos) may happen
      fast2 = ( fast && len <= i - lpos );
      }
    if( fast )					// no wrap
      {
      pos += len;
      if( fast2 )				// no wrap, no overlap
        std::memcpy( buffer + lpos, buffer + i, len );
      else
        for( ; len > 0; --len ) buffer[lpos++] = buffer[i++];
      }
    else for( ; len > 0; --len )
      {
      buffer[pos] = buffer[i];
      if( ++pos >= dictionary_size ) flush_data();
      if( ++i >= dictionary_size ) i = 0;
      }
    }

  void operator=( const LZ_mtester & );		// declared as private

public:
  LZ_mtester( const uint8_t * const ibuf, const long ibuf_size,
              const unsigned dict_size, const int ofd = -1 )
    :
    partial_data_pos( 0 ),
    rdec( ibuf, ibuf_size ),
    dictionary_size( dict_size ),
    buffer( new uint8_t[dictionary_size] ),
    pos( 0 ),
    stream_pos( 0 ),
    crc_( 0xFFFFFFFFU ),
    outfd( ofd ),
    rep0( 0 ),
    rep1( 0 ),
    rep2( 0 ),
    rep3( 0 ),
    max_rep0( 0 ),
    pos_wrapped( false )
    // prev_byte of first byte; also for peek( 0 ) on corrupt file
    { buffer[dictionary_size-1] = 0; }

  ~LZ_mtester() { delete[] buffer; }

  unsigned crc() const { return crc_ ^ 0xFFFFFFFFU; }
  unsigned long long data_position() const { return partial_data_pos + pos; }
  bool finished() { return rdec.finished(); }
  unsigned long member_position() const { return rdec.member_position(); }
  unsigned max_distance() const { return max_rep0 + 1; }

  void duplicate_buffer();
  int test_member( const unsigned long pos_limit = LONG_MAX );	// sets max_rep0
  int debug_decode_member( const long long dpos, const long long mpos,
                           const bool show_packets );	// sets max_rep0
  };
