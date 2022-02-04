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
/*
   Exit status: 0 for a normal exit, 1 for environmental problems
   (file not found, invalid flags, I/O errors, etc), 2 to indicate a
   corrupt or invalid input file, 3 for an internal consistency error
   (e.g., bug) which caused lziprecover to panic.
*/

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#if defined __MSVCRT__ || defined __OS2__ || defined __DJGPP__
#include <io.h>
#if defined __MSVCRT__
#define fchmod(x,y) 0
#define fchown(x,y,z) 0
#define SIGHUP SIGTERM
#define S_ISSOCK(x) 0
#ifndef S_IRGRP
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IROTH 0
#define S_IWOTH 0
#endif
#endif
#if defined __DJGPP__
#define S_ISSOCK(x) 0
#define S_ISVTX 0
#endif
#endif

#include "arg_parser.h"
#include "lzip.h"
#include "decoder.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if CHAR_BIT != 8
#error "Environments where CHAR_BIT != 8 are not supported."
#endif

#if ( defined  SIZE_MAX &&  SIZE_MAX < UINT_MAX ) || \
    ( defined SSIZE_MAX && SSIZE_MAX <  INT_MAX )
#error "Environments where 'size_t' is narrower than 'int' are not supported."
#endif

int verbosity = 0;

const char * const program_name = "lziprecover";
std::string output_filename;	// global vars for output file
int outfd = -1;			// see 'delete_output_on_interrupt' below

namespace {

const char * invocation_name = program_name;		// default value

const struct { const char * from; const char * to; } known_extensions[] = {
  { ".lz",  ""     },
  { ".tlz", ".tar" },
  { 0,      0      } };

enum Mode { m_none, m_alone_to_lz, m_debug_decompress, m_debug_delay,
            m_debug_repair, m_decompress, m_dump, m_list, m_md5sum, m_merge,
            m_nrep_stats, m_range_dec, m_remove, m_repair, m_reproduce,
            m_show_packets, m_split, m_strip, m_test, m_unzcrash_bit,
            m_unzcrash_block };

/* Variable used in signal handler context.
   It is not declared volatile because the handler never returns. */
bool delete_output_on_interrupt = false;


void show_help()
  {
  std::printf( "Lziprecover is a data recovery tool and decompressor for files in the lzip\n"
               "compressed data format (.lz). Lziprecover is able to repair slightly damaged\n"
               "files (up to one single-byte error per member), produce a correct file by\n"
               "merging the good parts of two or more damaged copies, reproduce a missing\n"
               "(zeroed) sector using a reference file, extract data from damaged files,\n"
               "decompress files, and test integrity of files.\n"
               "\nWith the help of lziprecover, losing an entire archive just because of a\n"
               "corrupt byte near the beginning is a thing of the past.\n"
               "\nLziprecover can remove the damaged members from multimember files, for\n"
               "example multimember tar.lz archives.\n"
               "\nLziprecover provides random access to the data in multimember files; it only\n"
               "decompresses the members containing the desired data.\n"
               "\nLziprecover facilitates the management of metadata stored as trailing data\n"
               "in lzip files.\n"
               "\nLziprecover is not a replacement for regular backups, but a last line of\n"
               "defense for the case where the backups are also damaged.\n"
               "\nUsage: %s [options] [files]\n", invocation_name );
  std::printf( "\nOptions:\n"
               "  -h, --help                    display this help and exit\n"
               "  -V, --version                 output version information and exit\n"
               "  -a, --trailing-error          exit with error status if trailing data\n"
               "  -A, --alone-to-lz             convert lzma-alone files to lzip format\n"
               "  -c, --stdout                  write to standard output, keep input files\n"
               "  -d, --decompress              decompress\n"
               "  -D, --range-decompress=<n-m>  decompress a range of bytes to stdout\n"
               "  -e, --reproduce               try to reproduce a zeroed sector in file\n"
               "      --lzip-level=N|a|m[N]     reproduce one level, all, or match length\n"
               "      --lzip-name=<name>        name of lzip executable for --reproduce\n"
               "      --reference-file=<file>   reference file for --reproduce\n"
               "  -f, --force                   overwrite existing output files\n"
               "  -i, --ignore-errors           ignore some errors in -d, -D, -l, -t, --dump\n"
               "  -k, --keep                    keep (don't delete) input files\n"
               "  -l, --list                    print (un)compressed file sizes\n"
               "  -m, --merge                   correct errors in file using several copies\n"
               "  -o, --output=<file>           place the output into <file>\n"
               "  -q, --quiet                   suppress all messages\n"
               "  -R, --repair                  try to repair a small error in file\n"
               "  -s, --split                   split multimember file in single-member files\n"
               "  -t, --test                    test compressed file integrity\n"
               "  -v, --verbose                 be verbose (a 2nd -v gives more)\n"
               "      --loose-trailing          allow trailing data seeming corrupt header\n"
               "      --dump=<list>:d:t         dump members listed/damaged, tdata to stdout\n"
               "      --remove=<list>:d:t       remove members, tdata from files in place\n"
               "      --strip=<list>:d:t        copy files to stdout stripping members given\n" );
  if( verbosity >= 1 )
    {
    std::printf( "\nDebug options for experts:\n"
                 "  -E, --debug-reproduce=<range>[,ss]  set range to 0 and try to reproduce file\n"
                 "  -M, --md5sum                      print the MD5 digests of the input files\n"
                 "  -S, --nrep-stats[=<val>]          print stats of N-byte repeated sequences\n"
                 "  -U, --unzcrash=1|B<size>          test 1-bit or block errors in input file\n"
                 "  -W, --debug-decompress=<pos>,<val>  set pos to val and decompress to stdout\n"
                 "  -X, --show-packets[=<pos>,<val>]  show in stdout the decoded LZMA packets\n"
                 "  -Y, --debug-delay=<range>         find max error detection delay in <range>\n"
                 "  -Z, --debug-repair=<pos>,<val>    test repair one-byte error at <pos>\n" );
    }
  std::printf( "\nIf no file names are given, or if a file is '-', lziprecover decompresses\n"
               "from standard input to standard output.\n"
               "Numbers may be followed by a multiplier: k = kB = 10^3 = 1000,\n"
               "Ki = KiB = 2^10 = 1024, M = 10^6, Mi = 2^20, G = 10^9, Gi = 2^30, etc...\n"
               "\nTo extract all the files from archive 'foo.tar.lz', use the commands\n"
               "'tar -xf foo.tar.lz' or 'lziprecover -cd foo.tar.lz | tar -xf -'.\n"
               "\nExit status: 0 for a normal exit, 1 for environmental problems (file\n"
               "not found, invalid flags, I/O errors, etc), 2 to indicate a corrupt or\n"
               "invalid input file, 3 for an internal consistency error (e.g., bug) which\n"
               "caused lziprecover to panic.\n"
               "\nReport bugs to lzip-bug@nongnu.org\n"
               "Lziprecover home page: http://www.nongnu.org/lzip/lziprecover.html\n" );
  }

} // end namespace

void Pretty_print::operator()( const char * const msg, FILE * const f ) const
  {
  if( verbosity < 0 ) return;
  if( first_post )
    {
    first_post = false;
    std::fputs( padded_name.c_str(), f );
    if( !msg ) std::fflush( f );
    }
  if( msg ) std::fprintf( f, "%s\n", msg );
  }


const char * bad_version( const unsigned version )
  {
  static char buf[80];
  snprintf( buf, sizeof buf, "Version %u member format not supported.",
            version );
  return buf;
  }


const char * format_ds( const unsigned dictionary_size )
  {
  enum { bufsize = 16, factor = 1024 };
  static char buf[bufsize];
  const char * const prefix[8] =
    { "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };
  const char * p = "";
  const char * np = "  ";
  unsigned num = dictionary_size;
  bool exact = ( num % factor == 0 );

  for( int i = 0; i < 8 && ( num > 9999 || ( exact && num >= factor ) ); ++i )
    { num /= factor; if( num % factor != 0 ) exact = false;
      p = prefix[i]; np = ""; }
  snprintf( buf, bufsize, "%s%4u %sB", np, num, p );
  return buf;
  }


void show_header( const unsigned dictionary_size )
  {
  std::fprintf( stderr, "dict %s, ", format_ds( dictionary_size ) );
  }


#include "main_common.cc"


// Colon-separated list of "damaged", "tdata", [r][^]<list> (1 1,3-5,8)
void Member_list::parse_ml( const char * arg, const char * const option_name )
  {
  while( true )
    {
    const char * tp = arg;		// points to terminator (':' or '\0')
    while( *tp && *tp != ':' ) ++tp;
    const unsigned len = tp - arg;
    if( std::islower( *(const unsigned char *)arg ) )
      {
      if( len <= 7 && std::strncmp( "damaged", arg, len ) == 0 )
        { damaged = true; goto next; }
      if( len <= 5 && std::strncmp( "tdata", arg, len ) == 0 )
        { tdata = true; goto next; }
      }
    {
    const bool reverse = ( *arg == 'r' );
    if( reverse ) ++arg;
    if( *arg == '^' ) { ++arg; if( reverse ) rin = false; else in = false; }
    std::vector< Block > * rvp = reverse ? &rrange_vector : &range_vector;
    while( std::isdigit( *(const unsigned char *)arg ) )
      {
      const char * tail;
      const int pos = getnum( arg, option_name, 0, 1, INT_MAX, &tail ) - 1;
      if( rvp->size() && pos < rvp->back().end() ) break;
      const int size = (*tail == '-') ?
        getnum( tail + 1, option_name, 0, pos + 1, INT_MAX, &tail ) - pos : 1;
      rvp->push_back( Block( pos, size ) );
      if( tail == tp ) goto next;
      if( *tail == ',' ) arg = tail + 1; else break;
      }
    }
    show_error( "Invalid list of members." );
    std::exit( 1 );
next:
    if( *(arg = tp) != 0 ) ++arg; else return;
    }
  }


namespace {

// Recognized formats: <digit> 'a' m[<match_length>]
//
int parse_lzip_level( const char * const arg, const char * const option_name )
  {
  if( *arg == 'a' || std::isdigit( *(const unsigned char *)arg ) ) return *arg;
  if( *arg != 'm' )
    {
    if( verbosity >= 0 )
      std::fprintf( stderr, "%s: Bad argument in option '%s'.\n",
                    program_name, option_name );
    std::exit( 1 );
    }
  if( arg[1] == 0 ) return -1;
  return -getnum( arg + 1, option_name, 0, min_match_len_limit, max_match_len );
  }


/* Recognized format: <range>[,<sector_size>]
   range formats: <begin> <begin>-<end> <begin>,<size> ,<size>
*/
void parse_range( const char * const arg, const char * const pn,
                  Block & range, int * const sector_sizep = 0 )
  {
  const char * tail = arg;
  long long value =
    ( arg[0] == ',' ) ? 0 : getnum( arg, pn, 0, 0, INT64_MAX - 1, &tail );
  if( tail[0] == 0 || tail[0] == ',' || tail[0] == '-' )
    {
    range.pos( value );
    if( tail[0] == 0 ) { range.size( INT64_MAX - value ); return; }
    const bool is_size = ( tail[0] == ',' );
    if( sector_sizep && tail[1] == ',' ) { value = INT64_MAX - value; ++tail; }
    else value = getnum( tail + 1, pn, 0, 1, INT64_MAX, &tail );	// size
    if( !is_size && value <= range.pos() )
      {
      if( verbosity >= 0 )
        std::fprintf( stderr, "%s: Begin must be < end in range argument "
                      "of option '%s'.\n", program_name, pn );
      std::exit( 1 );
      }
    if( !is_size ) value -= range.pos();
    if( INT64_MAX - value >= range.pos() )
      {
      range.size( value );
      if( sector_sizep && tail[0] == ',' )
        *sector_sizep = getnum( tail + 1, pn, 0, 8, INT_MAX );
      return;
      }
    }
  if( verbosity >= 0 )
    std::fprintf( stderr, "%s: Bad decompression range in option '%s'.\n",
                  program_name, pn );
  std::exit( 1 );
  }


void one_file( const int files )
  {
  if( files != 1 )
    {
    show_error( "You must specify exactly 1 file.", 0, true );
    std::exit( 1 );
    }
  }


void set_mode( Mode & program_mode, const Mode new_mode )
  {
  if( program_mode != m_none && program_mode != new_mode )
    {
    show_error( "Only one operation can be specified.", 0, true );
    std::exit( 1 );
    }
  program_mode = new_mode;
  }


void parse_u( const char * const arg, const char * const option_name,
              Mode & program_mode, int & sector_size )
  {
  if( arg[0] == '1' ) set_mode( program_mode, m_unzcrash_bit );
  else if( arg[0] == 'B' )
    { set_mode( program_mode, m_unzcrash_block );
      sector_size = getnum( arg + 1, option_name, 0, 1, INT_MAX ); }
  else
    {
    if( verbosity >= 0 )
      std::fprintf( stderr, "%s: Bad argument for option '%s'.\n",
                    program_name, option_name );
    std::exit( 1 );
    }
  }


int extension_index( const std::string & name )
  {
  for( int eindex = 0; known_extensions[eindex].from; ++eindex )
    {
    const std::string ext( known_extensions[eindex].from );
    if( name.size() > ext.size() &&
        name.compare( name.size() - ext.size(), ext.size(), ext ) == 0 )
      return eindex;
    }
  return -1;
  }


void set_a_outname( const std::string & name )
  {
  output_filename = name;
  if( name.size() > 5 && name.compare( name.size() - 5, 5, ".lzma" ) == 0 )
    output_filename.erase( name.size() - 2 );
  else if( name.size() > 4 && name.compare( name.size() - 4, 4, ".tlz" ) == 0 )
    output_filename.insert( name.size() - 2, "ar." );
  else if( name.size() <= 3 || name.compare( name.size() - 3, 3, ".lz" ) != 0 )
    output_filename += known_extensions[0].from;
  }


void set_d_outname( const std::string & name, const int eindex )
  {
  if( eindex >= 0 )
    {
    const std::string from( known_extensions[eindex].from );
    if( name.size() > from.size() )
      {
      output_filename.assign( name, 0, name.size() - from.size() );
      output_filename += known_extensions[eindex].to;
      return;
      }
    }
  output_filename = name; output_filename += ".out";
  if( verbosity >= 1 )
    std::fprintf( stderr, "%s: Can't guess original name for '%s' -- using '%s'\n",
                  program_name, name.c_str(), output_filename.c_str() );
  }

} // end namespace

int open_instream( const char * const name, struct stat * const in_statsp,
                   const bool one_to_one, const bool reg_only )
  {
  int infd = open( name, O_RDONLY | O_BINARY );
  if( infd < 0 )
    show_file_error( name, "Can't open input file", errno );
  else
    {
    const int i = fstat( infd, in_statsp );
    const mode_t mode = in_statsp->st_mode;
    const bool can_read = ( i == 0 && !reg_only &&
                            ( S_ISBLK( mode ) || S_ISCHR( mode ) ||
                              S_ISFIFO( mode ) || S_ISSOCK( mode ) ) );
    if( i != 0 || ( !S_ISREG( mode ) && ( !can_read || one_to_one ) ) )
      {
      if( verbosity >= 0 )
        std::fprintf( stderr, "%s: Input file '%s' is not a regular file%s.\n",
                      program_name, name, ( can_read && one_to_one ) ?
                      ",\n             and neither '-c' nor '-o' were specified" : "" );
      close( infd );
      infd = -1;
      }
    }
  return infd;
  }


int open_truncable_stream( const char * const name,
                           struct stat * const in_statsp )
  {
  int fd = open( name, O_RDWR | O_BINARY );
  if( fd < 0 )
    show_file_error( name, "Can't open input file", errno );
  else
    {
    const int i = fstat( fd, in_statsp );
    const mode_t mode = in_statsp->st_mode;
    if( i != 0 || !S_ISREG( mode ) )
      { show_file_error( name, "Not a regular file." ); close( fd ); fd = -1; }
    }
  return fd;
  }


bool open_outstream( const bool force, const bool protect,
                     const bool rw, const bool skipping )
  {
  const mode_t usr_rw = S_IRUSR | S_IWUSR;
  const mode_t all_rw = usr_rw | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  const mode_t outfd_mode = protect ? usr_rw : all_rw;
  int flags = O_CREAT | ( rw ? O_RDWR : O_WRONLY ) | O_BINARY;
  if( force ) flags |= O_TRUNC; else flags |= O_EXCL;

  outfd = open( output_filename.c_str(), flags, outfd_mode );
  if( outfd >= 0 ) delete_output_on_interrupt = true;
  else if( verbosity >= 0 )
    {
    if( errno == EEXIST )
      std::fprintf( stderr, "%s: Output file '%s' already exists%s.\n",
                    program_name, output_filename.c_str(), skipping ?
                    ", skipping" : ". Use '--force' to overwrite it" );
    else
      std::fprintf( stderr, "%s: Can't create output file '%s': %s\n",
                    program_name, output_filename.c_str(), std::strerror( errno ) );
    }
  return ( outfd >= 0 );
  }


bool file_exists( const std::string & filename )
  {
  struct stat st;
  if( stat( filename.c_str(), &st ) == 0 )
    {
    if( verbosity >= 0 )
      std::fprintf( stderr, "%s: Output file '%s' already exists."
                            " Use '--force' to overwrite it.\n",
                    program_name, filename.c_str() );
    return true;
    }
  return false;
  }


void set_signals( void (*action)(int) )
  {
  std::signal( SIGHUP, action );
  std::signal( SIGINT, action );
  std::signal( SIGTERM, action );
  }


void cleanup_and_fail( const int retval )
  {
  set_signals( SIG_IGN );			// ignore signals
  if( delete_output_on_interrupt )
    {
    delete_output_on_interrupt = false;
    if( verbosity >= 0 )
      std::fprintf( stderr, "%s: Deleting output file '%s', if it exists.\n",
                    program_name, output_filename.c_str() );
    if( outfd >= 0 ) { close( outfd ); outfd = -1; }
    if( std::remove( output_filename.c_str() ) != 0 && errno != ENOENT )
      show_error( "WARNING: deletion of output file (apparently) failed." );
    }
  std::exit( retval );
  }


bool check_tty_out()
  {
  if( isatty( outfd ) )
    { show_file_error( output_filename.size() ?
                       output_filename.c_str() : "(stdout)",
                       "I won't write compressed data to a terminal." );
      return false; }
  return true;
  }

namespace {

extern "C" void signal_handler( int )
  {
  show_error( "Control-C or similar caught, quitting." );
  cleanup_and_fail( 1 );
  }


bool check_tty_in( const char * const input_filename, const int infd,
                   const Mode program_mode, int & retval )
  {
  if( isatty( infd ) )			// all modes read compressed data
    { show_file_error( input_filename,
                       "I won't read compressed data from a terminal." );
      close( infd ); set_retval( retval, 2 );
      if( program_mode != m_test ) cleanup_and_fail( retval );
      return false; }
  return true;
  }

bool check_tty_out( const Mode program_mode )
  { return program_mode != m_alone_to_lz || ::check_tty_out(); }


// Set permissions, owner, and times.
void close_and_set_permissions( const struct stat * const in_statsp )
  {
  bool warning = false;
  if( in_statsp )
    {
    const mode_t mode = in_statsp->st_mode;
    // fchown will in many cases return with EPERM, which can be safely ignored.
    if( fchown( outfd, in_statsp->st_uid, in_statsp->st_gid ) == 0 )
      { if( fchmod( outfd, mode ) != 0 ) warning = true; }
    else
      if( errno != EPERM ||
          fchmod( outfd, mode & ~( S_ISUID | S_ISGID | S_ISVTX ) ) != 0 )
        warning = true;
    }
  if( close( outfd ) != 0 )
    {
    show_error( "Error closing output file", errno );
    cleanup_and_fail( 1 );
    }
  outfd = -1;
  delete_output_on_interrupt = false;
  if( in_statsp )
    {
    struct utimbuf t;
    t.actime = in_statsp->st_atime;
    t.modtime = in_statsp->st_mtime;
    if( utime( output_filename.c_str(), &t ) != 0 ) warning = true;
    }
  if( warning && verbosity >= 1 )
    show_error( "Can't change output file attributes." );
  }


unsigned char xdigit( const unsigned value )
  {
  if( value <= 9 ) return '0' + value;
  if( value <= 15 ) return 'A' + value - 10;
  return 0;
  }


bool show_trailing_data( const uint8_t * const data, const int size,
                         const Pretty_print & pp, const bool all,
                         const int ignore_trailing )	// -1 = show
  {
  if( verbosity >= 4 || ignore_trailing <= 0 )
    {
    std::string msg;
    if( !all ) msg = "first bytes of ";
    msg += "trailing data = ";
    for( int i = 0; i < size; ++i )
      {
      msg += xdigit( data[i] >> 4 );
      msg += xdigit( data[i] & 0x0F );
      msg += ' ';
      }
    msg += '\'';
    for( int i = 0; i < size; ++i )
      { if( std::isprint( data[i] ) ) msg += data[i]; else msg += '.'; }
    msg += '\'';
    pp( msg.c_str() );
    if( ignore_trailing == 0 ) show_file_error( pp.name(), trailing_msg );
    }
  return ( ignore_trailing > 0 );
  }


int decompress( const unsigned long long cfile_size, const int infd,
                const Pretty_print & pp, const bool ignore_errors,
                const bool ignore_trailing, const bool loose_trailing,
                const bool testing )
  {
  unsigned long long partial_file_pos = 0;
  Range_decoder rdec( infd );
  int retval = 0;

  for( bool first_member = true; ; first_member = false )
    {
    Lzip_header header;
    rdec.reset_member_position();
    const int size = rdec.read_header_carefully( header, ignore_errors );
    if( rdec.finished() ||			// End Of File
        ( size < Lzip_header::size && !rdec.find_header( header ) ) )
      {
      if( first_member )
        { show_file_error( pp.name(), "File ends unexpectedly at member header." );
          retval = 2; }
      else if( header.verify_prefix( size ) )
        { pp( "Truncated header in multimember file." );
          show_trailing_data( header.data, size, pp, true, -1 );
          retval = 2; }
      else if( size > 0 && !show_trailing_data( header.data, size, pp,
                                                true, ignore_trailing ) )
        retval = 2;
      break;
      }
    if( !header.verify_magic() )
      {
      if( first_member )
        { show_file_error( pp.name(), bad_magic_msg ); retval = 2; }
      else if( !loose_trailing && header.verify_corrupt() )
        { pp( corrupt_mm_msg );
          show_trailing_data( header.data, size, pp, false, -1 );
          retval = 2; }
      else if( !show_trailing_data( header.data, size, pp, false, ignore_trailing ) )
        retval = 2;
      if( ignore_errors ) { pp.reset(); continue; } else break;
      }
    if( !header.verify_version() )
      { pp( bad_version( header.version() ) ); retval = 2;
        if( ignore_errors ) { pp.reset(); continue; } else break; }
    const unsigned dictionary_size = header.dictionary_size();
    if( !isvalid_ds( dictionary_size ) )
      { pp( bad_dict_msg ); retval = 2;
        if( ignore_errors ) { pp.reset(); continue; } else break; }

    if( verbosity >= 2 || ( verbosity == 1 && first_member ) ) pp();

    LZ_decoder decoder( rdec, dictionary_size, outfd );
    show_dprogress( cfile_size, partial_file_pos, &rdec, &pp );	// init
    const int result = decoder.decode_member( pp );
    partial_file_pos += rdec.member_position();
    if( result != 0 )
      {
      if( verbosity >= 0 && result <= 2 )
        {
        pp();
        std::fprintf( stderr, "%s at pos %llu\n", ( result == 2 ) ?
                      "File ends unexpectedly" : "Decoder error",
                      partial_file_pos );
        }
      retval = 2; if( ignore_errors ) { pp.reset(); continue; } else break;
      }
    if( verbosity >= 2 )
      { std::fputs( testing ? "ok\n" : "done\n", stderr ); pp.reset(); }
    }
  if( verbosity == 1 && retval == 0 )
    std::fputs( testing ? "ok\n" : "done\n", stderr );
  if( retval == 2 && ignore_errors ) retval = 0;
  return retval;
  }

} // end namespace

void set_signal_handler() { set_signals( signal_handler ); }

int close_outstream( const struct stat * const in_statsp )
  {
  if( delete_output_on_interrupt )
    close_and_set_permissions( in_statsp );
  if( outfd >= 0 && close( outfd ) != 0 )
    { show_error( "Error closing stdout", errno ); return 1; }
  outfd = -1;
  return 0;
  }


std::string insert_fixed( std::string name )
  {
  if( name.size() > 7 && name.compare( name.size() - 7, 7, ".tar.lz" ) == 0 )
    name.insert( name.size() - 7, "_fixed" );
  else if( name.size() > 3 && name.compare( name.size() - 3, 3, ".lz" ) == 0 )
    name.insert( name.size() - 3, "_fixed" );
  else if( name.size() > 4 && name.compare( name.size() - 4, 4, ".tlz" ) == 0 )
    name.insert( name.size() - 4, "_fixed" );
  else name += "_fixed.lz";
  return name;
  }


void show_2file_error( const char * const msg1, const char * const name1,
                       const char * const name2, const char * const msg2 )
  {
  if( verbosity >= 0 )
    std::fprintf( stderr, "%s: %s '%s' and '%s' %s\n",
                  program_name, msg1, name1, name2, msg2 );
  }


void show_dprogress( const unsigned long long cfile_size,
                     const unsigned long long partial_size,
                     const Range_decoder * const d,
                     const Pretty_print * const p )
  {
  static unsigned long long csize = 0;		// file_size / 100
  static unsigned long long psize = 0;
  static const Range_decoder * rdec = 0;
  static const Pretty_print * pp = 0;
  static int counter = 0;
  static bool enabled = true;

  if( !enabled ) return;
  if( p )					// initialize static vars
    {
    if( verbosity < 2 || !isatty( STDERR_FILENO ) ) { enabled = false; return; }
    csize = cfile_size; psize = partial_size; rdec = d; pp = p; counter = 0;
    }
  if( rdec && pp && --counter <= 0 )
    {
    const unsigned long long pos = psize + rdec->member_position();
    counter = 7;		// update display every 114688 bytes
    if( csize > 0 )
      std::fprintf( stderr, "%4llu%%  %.1f MB\r", pos / csize, pos / 1000000.0 );
    else
      std::fprintf( stderr, "  %.1f MB\r", pos / 1000000.0 );
    pp->reset(); (*pp)();			// restore cursor position
    }
  }


int main( const int argc, const char * const argv[] )
  {
  Block range( 0, 0 );
  int sector_size = INT_MAX;		// default larger than practical range
  Bad_byte bad_byte;
  Member_list member_list;
  std::string default_output_filename;
  const char * lzip_name = "lzip";		// default is lzip
  const char * reference_filename = 0;
  Mode program_mode = m_none;
  int lzip_level = 0;		//  0 = test all levels and match lengths
				// '0'..'9' = level, 'a' = all levels
				// -5..-273 = match length, -1 = all lengths
  int repeated_byte = -1;	// 0 to 255, or -1 for all values
  bool force = false;
  bool ignore_errors = false;
  bool ignore_trailing = true;
  bool keep_input_files = false;
  bool loose_trailing = false;
  bool to_stdout = false;
  if( argc > 0 ) invocation_name = argv[0];

  enum { opt_du = 256, opt_lt, opt_lzl, opt_lzn, opt_ref, opt_re, opt_st };
  const Arg_parser::Option options[] =
    {
    { 'a', "trailing-error",     Arg_parser::no  },
    { 'A', "alone-to-lz",        Arg_parser::no  },
    { 'c', "stdout",             Arg_parser::no  },
    { 'd', "decompress",         Arg_parser::no  },
    { 'D', "range-decompress",   Arg_parser::yes },
    { 'e', "reproduce",          Arg_parser::no  },
    { 'E', "debug-reproduce",    Arg_parser::yes },
    { 'f', "force",              Arg_parser::no  },
    { 'h', "help",               Arg_parser::no  },
    { 'i', "ignore-errors",      Arg_parser::no  },
    { 'k', "keep",               Arg_parser::no  },
    { 'l', "list",               Arg_parser::no  },
    { 'm', "merge",              Arg_parser::no  },
    { 'M', "md5sum",             Arg_parser::no  },
    { 'n', "threads",            Arg_parser::yes },
    { 'o', "output",             Arg_parser::yes },
    { 'q', "quiet",              Arg_parser::no  },
    { 'R', "repair",             Arg_parser::no  },
    { 's', "split",              Arg_parser::no  },
    { 'S', "nrep-stats",         Arg_parser::maybe },
    { 't', "test",               Arg_parser::no  },
    { 'U', "unzcrash",           Arg_parser::yes },
    { 'v', "verbose",            Arg_parser::no  },
    { 'V', "version",            Arg_parser::no  },
    { 'W', "debug-decompress",   Arg_parser::yes },
    { 'X', "show-packets",       Arg_parser::maybe },
    { 'Y', "debug-delay",        Arg_parser::yes },
    { 'Z', "debug-repair",       Arg_parser::yes },
    { opt_du,  "dump",           Arg_parser::yes },
    { opt_lt,  "loose-trailing", Arg_parser::no  },
    { opt_lzl, "lzip-level",     Arg_parser::yes },
    { opt_lzn, "lzip-name",      Arg_parser::yes },
    { opt_ref, "reference-file", Arg_parser::yes },
    { opt_re,  "remove",         Arg_parser::yes },
    { opt_st,  "strip",          Arg_parser::yes },
    {  0 , 0,                    Arg_parser::no  } };

  const Arg_parser parser( argc, argv, options );
  if( parser.error().size() )				// bad option
    { show_error( parser.error().c_str(), 0, true ); return 1; }

  int argind = 0;
  for( ; argind < parser.arguments(); ++argind )
    {
    const int code = parser.code( argind );
    if( !code ) break;					// no more options
    const char * const pn = parser.parsed_name( argind ).c_str();
    const std::string & sarg = parser.argument( argind );
    const char * const arg = sarg.c_str();
    switch( code )
      {
      case 'a': ignore_trailing = false; break;
      case 'A': set_mode( program_mode, m_alone_to_lz ); break;
      case 'c': to_stdout = true; break;
      case 'd': set_mode( program_mode, m_decompress ); break;
      case 'D': set_mode( program_mode, m_range_dec );
                parse_range( arg, pn, range ); break;
      case 'e': set_mode( program_mode, m_reproduce ); break;
      case 'E': set_mode( program_mode, m_reproduce );
                parse_range( arg, pn, range, &sector_size ); break;
      case 'f': force = true; break;
      case 'h': show_help(); return 0;
      case 'i': ignore_errors = true; break;
      case 'k': keep_input_files = true; break;
      case 'l': set_mode( program_mode, m_list ); break;
      case 'm': set_mode( program_mode, m_merge ); break;
      case 'M': set_mode( program_mode, m_md5sum ); break;
      case 'n': break;
      case 'o': if( sarg == "-" ) to_stdout = true;
                else { default_output_filename = sarg; } break;
      case 'q': verbosity = -1; break;
      case 'R': set_mode( program_mode, m_repair ); break;
      case 's': set_mode( program_mode, m_split ); break;
      case 'S': if( arg[0] ) repeated_byte = getnum( arg, pn, 0, 0, 255 );
                set_mode( program_mode, m_nrep_stats ); break;
      case 't': set_mode( program_mode, m_test ); break;
      case 'U': parse_u( arg, pn, program_mode, sector_size ); break;
      case 'v': if( verbosity < 4 ) ++verbosity; break;
      case 'V': show_version(); return 0;
      case 'W': set_mode( program_mode, m_debug_decompress );
                bad_byte.parse_bb( arg, pn ); break;
      case 'X': set_mode( program_mode, m_show_packets );
                if( arg[0] ) { bad_byte.parse_bb( arg, pn ); } break;
      case 'Y': set_mode( program_mode, m_debug_delay );
                parse_range( arg, pn, range ); break;
      case 'Z': set_mode( program_mode, m_debug_repair );
                bad_byte.parse_bb( arg, pn ); break;
      case opt_du: set_mode( program_mode, m_dump );
                   member_list.parse_ml( arg, pn ); break;
      case opt_lt: loose_trailing = true; break;
      case opt_lzl: lzip_level = parse_lzip_level( arg, pn ); break;
      case opt_lzn: lzip_name = arg; break;
      case opt_ref: reference_filename = arg; break;
      case opt_re: set_mode( program_mode, m_remove );
                   member_list.parse_ml( arg, pn ); break;
      case opt_st: set_mode( program_mode, m_strip );
                   member_list.parse_ml( arg, pn ); break;
      default : internal_error( "uncaught option." );
      }
    } // end process options

#if defined __MSVCRT__ || defined __OS2__ || defined __DJGPP__
  setmode( STDIN_FILENO, O_BINARY );
  setmode( STDOUT_FILENO, O_BINARY );
#endif

  if( program_mode == m_none )
    {
    show_error( "You must specify the operation to be performed.", 0, true );
    return 1;
    }

  std::vector< std::string > filenames;
  bool filenames_given = false;
  for( ; argind < parser.arguments(); ++argind )
    {
    filenames.push_back( parser.argument( argind ) );
    if( filenames.back() != "-" ) filenames_given = true;
    }

  const char terminator = isatty( STDOUT_FILENO ) ? '\r' : '\n';
  try {
  switch( program_mode )
    {
    case m_none: internal_error( "invalid operation." ); break;
    case m_alone_to_lz: break;
    case m_debug_decompress:
      one_file( filenames.size() );
      return debug_decompress( filenames[0], bad_byte, false );
    case m_debug_delay:
      one_file( filenames.size() );
      return debug_delay( filenames[0], range, terminator );
    case m_debug_repair:
      one_file( filenames.size() );
      return debug_repair( filenames[0], bad_byte, terminator );
    case m_decompress: break;
    case m_dump:
    case m_strip:
      if( filenames.size() < 1 )
        { show_error( "You must specify at least 1 file.", 0, true ); return 1; }
      return dump_members( filenames, default_output_filename, member_list,
                           force, ignore_errors, ignore_trailing,
                           loose_trailing, program_mode == m_strip, to_stdout );
    case m_list: break;
    case m_md5sum: break;
    case m_merge:
      if( filenames.size() < 2 )
        { show_error( "You must specify at least 2 files.", 0, true ); return 1; }
      return merge_files( filenames, default_output_filename, terminator, force );
    case m_nrep_stats: return print_nrep_stats( filenames, repeated_byte,
                              ignore_errors, ignore_trailing, loose_trailing );
    case m_range_dec:
      one_file( filenames.size() );
      return range_decompress( filenames[0], default_output_filename, range,
                               force, ignore_errors, ignore_trailing,
                               loose_trailing, to_stdout );
    case m_remove:
      if( filenames.size() < 1 )
        { show_error( "You must specify at least 1 file.", 0, true ); return 1; }
      return remove_members( filenames, member_list, ignore_errors,
                             ignore_trailing, loose_trailing );
    case m_repair:
      one_file( filenames.size() );
      return repair_file( filenames[0], default_output_filename, terminator, force );
    case m_reproduce:
      one_file( filenames.size() );
      if( !reference_filename || !reference_filename[0] )
        { show_error( "You must specify a reference file.", 0, true ); return 1; }
      if( range.size() > 0 )
        return debug_reproduce_file( filenames[0], lzip_name,
          reference_filename, range, sector_size, lzip_level );
      else
        return reproduce_file( filenames[0], default_output_filename,
          lzip_name, reference_filename, lzip_level, terminator, force );
    case m_show_packets:
      one_file( filenames.size() );
      return debug_decompress( filenames[0], bad_byte, true );
    case m_split:
      one_file( filenames.size() );
      return split_file( filenames[0], default_output_filename, force );
    case m_test: break;
    case m_unzcrash_bit:
      one_file( filenames.size() );
      return lunzcrash_bit( filenames[0].c_str() );
    case m_unzcrash_block:
      one_file( filenames.size() );
      return lunzcrash_block( filenames[0].c_str(), sector_size );
    }
    }
  catch( std::bad_alloc & ) { show_error( mem_msg ); cleanup_and_fail( 1 ); }
  catch( Error & e ) { show_error( e.msg, errno ); cleanup_and_fail( 1 ); }

  if( filenames.empty() ) filenames.push_back("-");

  if( program_mode == m_list )
    return list_files( filenames, ignore_errors, ignore_trailing, loose_trailing );
  if( program_mode == m_md5sum )
    return md5sum_files( filenames );

  if( program_mode != m_alone_to_lz && program_mode != m_decompress &&
      program_mode != m_test )
    internal_error( "invalid decompressor operation." );

  if( program_mode == m_test ) to_stdout = false;	// apply overrides
  if( program_mode == m_test || to_stdout ) default_output_filename.clear();

  if( to_stdout && program_mode != m_test )	// check tty only once
    { outfd = STDOUT_FILENO; if( !check_tty_out( program_mode ) ) return 1; }
  else outfd = -1;

  const bool to_file = !to_stdout && program_mode != m_test &&
                       default_output_filename.size();
  if( !to_stdout && program_mode != m_test && ( filenames_given || to_file ) )
    set_signals( signal_handler );

  Pretty_print pp( filenames );

  int failed_tests = 0;
  int retval = 0;
  const bool one_to_one = !to_stdout && program_mode != m_test && !to_file;
  bool stdin_used = false;
  for( unsigned i = 0; i < filenames.size(); ++i )
    {
    std::string input_filename;
    int infd;
    struct stat in_stats;

    pp.set_name( filenames[i] );
    if( filenames[i] == "-" )
      {
      if( stdin_used ) continue; else stdin_used = true;
      infd = STDIN_FILENO;
      if( !check_tty_in( pp.name(), infd, program_mode, retval ) ) continue;
      if( one_to_one ) { outfd = STDOUT_FILENO; output_filename.clear(); }
      }
    else
      {
      input_filename = filenames[i];
      infd = open_instream( input_filename.c_str(), &in_stats, one_to_one );
      if( infd < 0 ) { set_retval( retval, 1 ); continue; }
      if( !check_tty_in( pp.name(), infd, program_mode, retval ) ) continue;
      if( one_to_one )			// open outfd after verifying infd
        {
        if( program_mode == m_alone_to_lz ) set_a_outname( input_filename );
        else set_d_outname( input_filename, extension_index( input_filename ) );
        if( !open_outstream( force, true ) )
          { close( infd ); set_retval( retval, 1 ); continue; }
        }
      }

    if( one_to_one && !check_tty_out( program_mode ) )
      { set_retval( retval, 1 ); return retval; }	// don't delete a tty

    if( to_file && outfd < 0 )		// open outfd after verifying infd
      {
      output_filename = default_output_filename;
      if( !open_outstream( force, false ) || !check_tty_out( program_mode ) )
        return 1;	// check tty only once and don't try to delete a tty
      }

    const struct stat * const in_statsp =
      ( input_filename.size() && one_to_one ) ? &in_stats : 0;
    const unsigned long long cfile_size =
      ( input_filename.size() && S_ISREG( in_stats.st_mode ) ) ?
        ( in_stats.st_size + 99 ) / 100 : 0;
    int tmp;
    try {
      if( program_mode == m_alone_to_lz )
        tmp = alone_to_lz( infd, pp );
      else
        tmp = decompress( cfile_size, infd, pp, ignore_errors, ignore_trailing,
                          loose_trailing, program_mode == m_test );
      }
    catch( std::bad_alloc & ) { pp( mem_msg ); tmp = 1; }
    catch( Error & e ) { pp(); show_error( e.msg, errno ); tmp = 1; }
    if( close( infd ) != 0 )
      { show_file_error( pp.name(), "Error closing input file", errno );
        set_retval( tmp, 1 ); }
    set_retval( retval, tmp );
    if( tmp )
      { if( program_mode != m_test ) cleanup_and_fail( retval );
        else ++failed_tests; }

    if( delete_output_on_interrupt && one_to_one )
      close_and_set_permissions( in_statsp );
    if( input_filename.size() && !keep_input_files && one_to_one &&
        ( program_mode != m_decompress || !ignore_errors ) )
      std::remove( input_filename.c_str() );
    }
  if( delete_output_on_interrupt ) close_and_set_permissions( 0 );	// -o
  else if( outfd >= 0 && close( outfd ) != 0 )				// -c
    {
    show_error( "Error closing stdout", errno );
    set_retval( retval, 1 );
    }
  if( failed_tests > 0 && verbosity >= 1 && filenames.size() > 1 )
    std::fprintf( stderr, "%s: warning: %d %s failed the test.\n",
                  program_name, failed_tests,
                  ( failed_tests == 1 ) ? "file" : "files" );
  return retval;
  }
