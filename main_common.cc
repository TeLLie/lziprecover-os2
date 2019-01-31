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

namespace {

const char * const program_year = "2019";

void show_version()
  {
  std::printf( "%s %s\n", program_name, PROGVERSION );
  std::printf( "Copyright (C) %s Antonio Diaz Diaz.\n", program_year );
  std::printf( "License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>\n"
               "This is free software: you are free to change and redistribute it.\n"
               "There is NO WARRANTY, to the extent permitted by law.\n" );
  }


// Recognized formats: <num>[YZEPTGM][i][Bs], <num>k[Bs], <num>Ki[Bs]
//
long long getnum( const char * const ptr, const int hardbs,
                  const long long llimit = -LLONG_MAX,
                  const long long ulimit = LLONG_MAX,
                  const char ** const tailp = 0 )
  {
  char * tail;
  errno = 0;
  long long result = strtoll( ptr, &tail, 0 );
  if( tail == ptr )
    {
    show_error( "Bad or missing numerical argument.", 0, true );
    std::exit( 1 );
    }

  if( !errno && tail[0] )
    {
    char * const p = tail++;
    int factor = 1000;				// default factor
    int exponent = -1;				// -1 = bad multiplier
    char usuf = 0;			// 'B' or 's' unit suffix is present
    switch( *p )
      {
      case 'Y': exponent = 8; break;
      case 'Z': exponent = 7; break;
      case 'E': exponent = 6; break;
      case 'P': exponent = 5; break;
      case 'T': exponent = 4; break;
      case 'G': exponent = 3; break;
      case 'M': exponent = 2; break;
      case 'K': if( tail[0] == 'i' ) { ++tail; factor = 1024; exponent = 1; } break;
      case 'k': if( tail[0] != 'i' ) exponent = 1; break;
      case 'B':
      case 's': usuf = *p; exponent = 0; break;
      default : if( tailp ) { tail = p; exponent = 0; }
      }
    if( exponent > 1 && tail[0] == 'i' ) { ++tail; factor = 1024; }
    if( exponent > 0 && usuf == 0 && ( tail[0] == 'B' || tail[0] == 's' ) )
      { usuf = tail[0]; ++tail; }
    if( exponent < 0 || ( usuf == 's' && hardbs <= 0 ) ||
        ( !tailp && tail[0] != 0 ) )
      {
      show_error( "Bad multiplier in numerical argument.", 0, true );
      std::exit( 1 );
      }
    for( int i = 0; i < exponent; ++i )
      {
      if( LLONG_MAX / factor >= llabs( result ) ) result *= factor;
      else { errno = ERANGE; break; }
      }
    if( usuf == 's' )
      {
      if( LLONG_MAX / hardbs >= llabs( result ) ) result *= hardbs;
      else errno = ERANGE;
      }
    }
  if( !errno && ( result < llimit || result > ulimit ) ) errno = ERANGE;
  if( errno )
    {
    show_error( "Numerical argument out of limits." );
    std::exit( 1 );
    }
  if( tailp ) *tailp = tail;
  return result;
  }

} // end namespace


void show_error( const char * const msg, const int errcode, const bool help )
  {
  if( verbosity < 0 ) return;
  if( msg && msg[0] )
    std::fprintf( stderr, "%s: %s%s%s\n", program_name, msg,
                  ( errcode > 0 ) ? ": " : "",
                  ( errcode > 0 ) ? std::strerror( errcode ) : "" );
  if( help )
    std::fprintf( stderr, "Try '%s --help' for more information.\n",
                  invocation_name );
  }


void internal_error( const char * const msg )
  {
  if( verbosity >= 0 )
    std::fprintf( stderr, "%s: internal error: %s\n", program_name, msg );
  std::exit( 3 );
  }
