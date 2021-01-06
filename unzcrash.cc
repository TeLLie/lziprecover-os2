/* Unzcrash - Tests robustness of decompressors to corrupted data.
   Inspired by unzcrash.c from Julian Seward's bzip2.
   Copyright (C) 2008-2021 Antonio Diaz Diaz.

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
/*
   Exit status: 0 for a normal exit, 1 for environmental problems
   (file not found, invalid flags, I/O errors, etc), 2 to indicate a
   corrupt or invalid input file, 3 for an internal consistency error
   (eg, bug) which caused unzcrash to panic.
*/

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "arg_parser.h"

#if CHAR_BIT != 8
#error "Environments where CHAR_BIT != 8 are not supported."
#endif

#ifndef INT64_MAX
#define INT64_MAX  0x7FFFFFFFFFFFFFFFLL
#endif

void show_error( const char * const msg, const int errcode = 0,
                 const bool help = false );

namespace {

const char * const program_name = "unzcrash";
const char * invocation_name = program_name;		// default value

int verbosity = 0;


void show_help()
  {
  std::printf( "Unzcrash tests the robustness of decompressors to corrupted data.\n"
               "\nBy default, unzcrash reads the file specified and then repeatedly\n"
               "decompresses it, increasing 256 times each byte of the compressed data, so\n"
               "as to test all possible one-byte errors. Note that it may take years or even\n"
               "centuries to test all possible one-byte errors in a large file (tens of MB).\n"
               "\nIf the option '--block' is given, unzcrash reads the file specified and\n"
               "then repeatedly decompresses it, setting all bytes in each successive block\n"
               "to the value given, so as to test all possible full sector errors.\n"
               "\nIf the option '--truncate' is given, unzcrash reads the file specified\n"
               "and then repeatedly decompresses it, truncating the file to increasing\n"
               "lengths, so as to test all possible truncation points.\n"
               "\nNone of the three test modes described above should cause any invalid memory\n"
               "accesses. If any of them does, please, report it as a bug to the maintainers\n"
               "of the decompressor being tested.\n"
               "\nIf the decompressor returns with zero status, unzcrash compares the output\n"
               "of the decompressor for the original and corrupt files. If the outputs\n"
               "differ, it means that the decompressor returned a false negative; it failed\n"
               "to recognize the corruption and produced garbage output. The only exception\n"
               "is when a multimember file is truncated just after the last byte of a\n"
               "member, producing a shorter but valid compressed file. Except in this latter\n"
               "case, please, report any false negative as a bug.\n"
               "\nIn order to compare the outputs, unzcrash needs a 'zcmp' program able to\n"
               "understand the format being tested. For example the zcmp provided by zutils.\n"
               "Use '--zcmp=false' to disable comparisons.\n"
               "\nUsage: %s [options] 'lzip -t' file.lz\n", invocation_name );
  std::printf( "\nOptions:\n"
               "  -h, --help                    display this help and exit\n"
               "  -V, --version                 output version information and exit\n"
               "  -b, --bits=<range>            test N-bit errors instead of full byte\n"
               "  -B, --block[=<size>][,<val>]  test blocks of given size [512,0]\n"
               "  -d, --delta=<n>               test one byte/block/truncation every n bytes\n"
               "  -e, --set-byte=<pos>,<val>    set byte at position <pos> to value <val>\n"
               "  -n, --no-verify               skip initial verification of file.lz\n"
               "  -p, --position=<bytes>        first byte position to test [default 0]\n"
               "  -q, --quiet                   suppress all messages\n"
               "  -s, --size=<bytes>            number of byte positions to test [all]\n"
               "  -t, --truncate                test decompression of truncated file\n"
               "  -v, --verbose                 be verbose (a 2nd -v gives more)\n"
               "  -z, --zcmp=<command>          set zcmp command name and options [zcmp]\n"
               "Examples of <range>:  1  1,2,3  1-4  1,3-5,8  1-3,5-8\n"
               "A negative position is relative to the end of file.\n"
               "A negative size is relative to the rest of the file.\n"
               "\nExit status: 0 for a normal exit, 1 for environmental problems (file\n"
               "not found, invalid flags, I/O errors, etc), 2 to indicate a corrupt or\n"
               "invalid input file, 3 for an internal consistency error (eg, bug) which\n"
               "caused unzcrash to panic.\n"
               "\nReport bugs to lzip-bug@nongnu.org\n"
               "Lziprecover home page: http://www.nongnu.org/lzip/lziprecover.html\n" );
  }

} // end namespace


#include "main_common.cc"


namespace {

void parse_block( const char * const ptr, long & size, uint8_t & value )
  {
  const char * tail = ptr;

  if( tail[0] != ',' )
    size = getnum( ptr, 0, 1, INT_MAX, &tail );
  if( tail[0] == ',' )
    value = getnum( tail + 1, 0, 0, 255 );
  else if( tail[0] )
    {
    show_error( "Bad separator in argument of '--block'", 0, true );
    std::exit( 1 );
    }
  }


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


// Recognized formats: <pos>,<value> <pos>,+<value> <pos>,f<value>
//
void parse_pos_value( const char * const ptr, Bad_byte & bad_byte )
  {
  const char * tail;
  bad_byte.pos = getnum( ptr, 0, 0, INT64_MAX, &tail );
  if( tail[0] != ',' )
    {
    show_error( "Bad separator between <pos> and <val>.", 0, true );
    std::exit( 1 );
    }
  if( tail[1] == '+' ) { ++tail; bad_byte.mode = Bad_byte::delta; }
  else if( tail[1] == 'f' ) { ++tail; bad_byte.mode = Bad_byte::flip; }
  else bad_byte.mode = Bad_byte::literal;
  bad_byte.value = getnum( tail + 1, 0, 0, 255 );
  }


/* Returns the address of a malloc'd buffer containing the file data and
   the file size in '*size'.
   In case of error, returns 0 and does not modify '*size'.
*/
uint8_t * read_file( const char * const name, long * const size )
  {
  FILE * const f = std::fopen( name, "rb" );
  if( !f )
    {
    if( verbosity >= 0 )
      std::fprintf( stderr, "%s: Can't open input file '%s': %s\n",
                    program_name, name, std::strerror( errno ) );
    return 0;
    }

  long buffer_size = 1 << 20;
  uint8_t * buffer = (uint8_t *)std::malloc( buffer_size );
  if( !buffer ) { show_error( mem_msg ); return 0; }
  long file_size = std::fread( buffer, 1, buffer_size, f );
  while( file_size >= buffer_size )
    {
    if( buffer_size >= LONG_MAX )
      {
      if( verbosity >= 0 )
        std::fprintf( stderr, "%s: Input file '%s' is too large.\n",
                      program_name, name );
      std::free( buffer ); return 0;
      }
    buffer_size = ( buffer_size <= LONG_MAX / 2 ) ? 2 * buffer_size : LONG_MAX;
    uint8_t * const tmp = (uint8_t *)std::realloc( buffer, buffer_size );
    if( !tmp ) { show_error( mem_msg ); std::free( buffer ); return 0; }
    buffer = tmp;
    file_size += std::fread( buffer + file_size, 1, buffer_size - file_size, f );
    }
  if( std::ferror( f ) || !std::feof( f ) )
    {
    if( verbosity >= 0 )
      std::fprintf( stderr, "%s: Error reading file '%s': %s\n",
                    program_name, name, std::strerror( errno ) );
    std::free( buffer ); return 0;
    }
  std::fclose( f );
  *size = file_size;
  return buffer;
  }


class Bitset8			// 8 value bitset (1 to 8)
  {
  bool data[8];
  static bool valid_digit( const unsigned char ch )
    { return ( ch >= '1' && ch <= '8' ); }

public:
  Bitset8() { for( int i = 0; i < 8; ++i ) data[i] = true; }

  bool includes( const int i ) const
    { return ( i >= 1 && i <= 8 && data[i-1] ); }

  // Recognized formats: 1 1,2,3 1-4 1,3-5,8 1-3,5-8
  bool parse( const char * p )
    {
    for( int i = 0; i < 8; ++i ) data[i] = false;
    while( true )
      {
      const unsigned char ch1 = *p++;
      if( !valid_digit( ch1 ) ) break;
      if( *p != '-' ) data[ch1-'1'] = true;
      else
        {
        ++p;
        if( !valid_digit( *p ) || ch1 > *p ) break;
        for( int c = ch1; c <= *p; ++c ) data[c-'1'] = true;
        ++p;
        }
      if( *p == 0 ) return true;
      if( *p == ',' ) ++p; else break;
      }
    show_error( "Invalid value or range." );
    return false;
    }

  // number of N-bit errors per byte (N=0 to 8): 1 8 28 56 70 56 28 8 1
  void print() const
    {
    std::fflush( stderr );
    int c = 0;
    for( int i = 0; i < 8; ++i ) if( data[i] ) ++c;
    if( c == 8 ) std::fputs( "Testing full byte.\n", stdout );
    else if( c == 0 ) std::fputs( "Nothing to test.\n", stdout );
    else
      {
      std::fputs( "Testing ", stdout );
      for( int i = 0; i < 8; ++i )
        if( data[i] )
          {
          std::printf( "%d", i + 1 );
          if( --c ) std::fputc( ',', stdout );
          }
      std::fputs( " bit errors.\n", stdout );
      }
    std::fflush( stdout );
    }
  };


int differing_bits( const uint8_t byte1, const uint8_t byte2 )
  {
  int count = 0;
  uint8_t dif = byte1 ^ byte2;
  while( dif )
    { count += ( dif & 1 ); dif >>= 1; }
  return count;
  }

} // end namespace


int main( const int argc, const char * const argv[] )
  {
  enum Mode { m_block, m_byte, m_truncate };
  const char * mode_str[3] = { "block", "byte", "size" };
  Bitset8 bits;			// if Bitset8::parse not called test full byte
  Bad_byte bad_byte;
  const char * zcmp_program = "zcmp";
  long pos = 0;
  long max_size = LONG_MAX;
  long delta = 0;		// to be set later
  long block_size = 512;
  Mode program_mode = m_byte;
  uint8_t block_value = 0;
  bool verify = true;
  if( argc > 0 ) invocation_name = argv[0];

  const Arg_parser::Option options[] =
    {
    { 'h', "help",      Arg_parser::no  },
    { 'b', "bits",      Arg_parser::yes },
    { 'B', "block",     Arg_parser::maybe },
    { 'd', "delta",     Arg_parser::yes },
    { 'e', "set-byte",  Arg_parser::yes },
    { 'n', "no-verify", Arg_parser::no  },
    { 'p', "position",  Arg_parser::yes },
    { 'q', "quiet",     Arg_parser::no  },
    { 's', "size",      Arg_parser::yes },
    { 't', "truncate",  Arg_parser::no  },
    { 'v', "verbose",   Arg_parser::no  },
    { 'V', "version",   Arg_parser::no  },
    { 'z', "zcmp",      Arg_parser::yes },
    {  0 , 0,           Arg_parser::no  } };

  const Arg_parser parser( argc, argv, options );
  if( parser.error().size() )				// bad option
    { show_error( parser.error().c_str(), 0, true ); return 1; }

  int argind = 0;
  for( ; argind < parser.arguments(); ++argind )
    {
    const int code = parser.code( argind );
    if( !code ) break;					// no more options
    const char * const arg = parser.argument( argind ).c_str();
    switch( code )
      {
      case 'h': show_help(); return 0;
      case 'b': if( !bits.parse( arg ) ) return 1; program_mode = m_byte; break;
      case 'B': if( arg[0] ) parse_block( arg, block_size, block_value );
                program_mode = m_block; break;
      case 'd': delta = getnum( arg, block_size, 1, INT_MAX ); break;
      case 'e': parse_pos_value( arg, bad_byte ); break;
      case 'n': verify = false; break;
      case 'p': pos = getnum( arg, block_size, -LONG_MAX, LONG_MAX ); break;
      case 'q': verbosity = -1; break;
      case 's': max_size = getnum( arg, block_size, -LONG_MAX, LONG_MAX ); break;
      case 't': program_mode = m_truncate; break;
      case 'v': if( verbosity < 4 ) ++verbosity; break;
      case 'V': show_version(); return 0;
      case 'z': zcmp_program = arg; break;
      default : internal_error( "uncaught option." );
      }
    } // end process options

  if( argind + 2 != parser.arguments() )
    {
    if( verbosity >= 0 )
      std::fprintf( stderr, "Usage: %s 'lzip -t' file.lz\n", invocation_name );
    return 1;
    }

  if( delta <= 0 ) delta = ( program_mode == m_block ) ? block_size : 1;

  const char * const filename = parser.argument( argind + 1 ).c_str();
  long file_size = 0;
  uint8_t * const buffer = read_file( filename, &file_size );
  if( !buffer ) return 1;
  const char * const command = parser.argument( argind ).c_str();
  char zcmp_command[1024] = { 0 };
  if( std::strcmp( zcmp_program, "false" ) != 0 )
    snprintf( zcmp_command, sizeof zcmp_command, "%s '%s' -",
              zcmp_program, filename );

  // verify original file
  if( verbosity >= 1 ) fprintf( stderr, "Testing file '%s'\n", filename );
  if( verify )
    {
    FILE * f = popen( command, "w" );
    if( !f )
      { show_error( "Can't open pipe to decompressor", errno ); return 1; }
    if( (long)std::fwrite( buffer, 1, file_size, f ) != file_size )
      { show_error( "Can't write to decompressor", errno ); return 1; }
    if( pclose( f ) != 0 )
      {
      if( verbosity >= 0 )
        std::fprintf( stderr, "%s: Can't run '%s'.\n", program_name, command );
      return 1;
      }
    if( zcmp_command[0] )
      {
      f = popen( zcmp_command, "w" );
      if( !f )
        { show_error( "Can't open pipe to zcmp command", errno ); return 1; }
      if( (long)std::fwrite( buffer, 1, file_size, f ) != file_size )
        { show_error( "Can't write to zcmp command", errno ); return 1; }
      if( pclose( f ) != 0 )
        {
        show_error( "zcmp command failed. Disabling comparisons" );
        zcmp_command[0] = 0;
        }
      }
    }

  std::signal( SIGPIPE, SIG_IGN );

  if( pos < 0 ) pos = std::max( 0L, file_size + pos );
  if( pos >= file_size || max_size == 0 ||
      ( max_size < 0 && -max_size >= file_size - pos ) )
    { show_error( "Nothing to do; domain is empty." ); return 0; }
  if( max_size < 0 ) max_size += file_size - pos;
  const long end = ( ( max_size < file_size - pos ) ? pos + max_size : file_size );
  if( bad_byte.pos >= file_size )
    { show_error( "Position of '--set-byte' is beyond end of file." );
      return 1; }
  if( bad_byte.pos >= 0 )
    buffer[bad_byte.pos] = bad_byte( buffer[bad_byte.pos] );
  long positions = 0, decompressions = 0, successes = 0, failed_comparisons = 0;
  if( program_mode == m_truncate )
    for( long i = pos; i < end; i += std::min( delta, end - i ) )
      {
      if( verbosity >= 0 )
        std::fprintf( stderr, "length %ld\n", i );
      ++positions; ++decompressions;
      FILE * f = popen( command, "w" );
      if( !f ) { show_error( "Can't open pipe", errno ); return 1; }
      std::fwrite( buffer, 1, i, f );
      if( pclose( f ) == 0 )
        {
        ++successes;
        if( verbosity >= 0 )
          std::fputs( "passed the test\n", stderr );
        if( zcmp_command[0] )
          {
          f = popen( zcmp_command, "w" );
          if( !f ) { show_error( "Can't open pipe", errno ); return 1; }
          std::fwrite( buffer, 1, i, f );
          if( pclose( f ) != 0 )
            {
            ++failed_comparisons;
            if( verbosity >= 0 )
              std::fprintf( stderr, "length %ld comparison failed\n", i );
            }
          }
        }
      }
  else if( program_mode == m_block )
    {
    uint8_t * block = (uint8_t *)std::malloc( block_size );
    if( !block ) { show_error( mem_msg ); return 1; }
    for( long i = pos; i < end; i += std::min( delta, end - i ) )
      {
      const long size = std::min( block_size, file_size - i );
      if( verbosity >= 0 )
        std::fprintf( stderr, "block %ld,%ld\n", i, size );
      ++positions; ++decompressions;
      FILE * f = popen( command, "w" );
      if( !f ) { show_error( "Can't open pipe", errno ); return 1; }
      std::memcpy( block , buffer + i, size );
      std::memset( buffer + i, block_value, size );
      std::fwrite( buffer, 1, file_size, f );
      if( pclose( f ) == 0 )
        {
        ++successes;
        if( verbosity >= 0 )
          std::fputs( "passed the test\n", stderr );
        if( zcmp_command[0] )
          {
          f = popen( zcmp_command, "w" );
          if( !f ) { show_error( "Can't open pipe", errno ); return 1; }
          std::fwrite( buffer, 1, file_size, f );
          if( pclose( f ) != 0 )
            {
            ++failed_comparisons;
            if( verbosity >= 0 )
              std::fprintf( stderr, "block %ld,%ld comparison failed\n", i, size );
            }
          }
        }
      std::memcpy( buffer + i, block, size );
      }
    std::free( block );
    }
  else
    {
    if( verbosity >= 1 ) bits.print();
    for( long i = pos; i < end; i += std::min( delta, end - i ) )
      {
      if( verbosity >= 0 )
        std::fprintf( stderr, "byte %ld\n", i );
      ++positions;
      const uint8_t byte = buffer[i];
      for( int j = 1; j < 256; ++j )
        {
        ++buffer[i];
        if( bits.includes( differing_bits( byte, buffer[i] ) ) )
          {
          ++decompressions;
          if( verbosity >= 2 )
            std::fprintf( stderr, "0x%02X (0x%02X+0x%02X) ",
                          buffer[i], byte, j );
          FILE * f = popen( command, "w" );
          if( !f ) { show_error( "Can't open pipe", errno ); return 1; }
          std::fwrite( buffer, 1, file_size, f );
          if( pclose( f ) == 0 )
            {
            ++successes;
            if( verbosity >= 0 )
              { if( verbosity < 2 )	// else already printed above
                  std::fprintf( stderr, "0x%02X (0x%02X+0x%02X) ",
                                buffer[i], byte, j );
                std::fputs( "passed the test\n", stderr ); }
            if( zcmp_command[0] )
              {
              f = popen( zcmp_command, "w" );
              if( !f ) { show_error( "Can't open pipe", errno ); return 1; }
              std::fwrite( buffer, 1, file_size, f );
              if( pclose( f ) != 0 )
                {
                ++failed_comparisons;
                if( verbosity >= 0 )
                  std::fprintf( stderr, "byte %ld comparison failed\n", i );
                }
              }
            }
          }
        }
      buffer[i] = byte;
      }
    }

  if( verbosity >= 0 )
    {
    std::fprintf( stderr, "\n%8ld %ss tested\n%8ld total decompressions"
                          "\n%8ld decompressions returned with zero status",
                  positions, mode_str[program_mode], decompressions, successes );
    if( successes > 0 )
      {
      if( zcmp_command[0] == 0 )
        std::fputs( "\n         comparisons disabled\n", stderr );
      else if( failed_comparisons > 0 )
        std::fprintf( stderr, ", of which\n%8ld comparisons failed\n",
                      failed_comparisons );
      else std::fputs( "\n         all comparisons passed\n", stderr );
      }
    else std::fputc( '\n', stderr );
    }

  std::free( buffer );
  return 0;
  }
