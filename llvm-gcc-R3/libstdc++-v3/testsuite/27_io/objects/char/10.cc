// 2003-05-01  Petur Runolfsson  <peturr02@ru.is>

// Copyright (C) 2003 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
// USA.
 
#include <iostream>
#include <cstdio>
#include <testsuite_hooks.h>

void test10()
{
  using namespace std;

  bool test __attribute__((unused)) = true;
  const char* name = "filebuf_virtuals-1.txt";

  FILE* ret = freopen(name, "r", stdin);
  VERIFY( ret != NULL );

  streampos p1 = cin.tellg();
  VERIFY( p1 != streampos(-1) );
  VERIFY( streamoff(p1) == 0 );

  cin.seekg(0, ios::end);
  VERIFY( cin.good() );

  streampos p2 = cin.tellg();
  VERIFY( p2 != streampos(-1) );
  VERIFY( p2 != p1 );
  VERIFY( streamoff(p2) == ftell(stdin) );

  cin.seekg(p1);
  VERIFY( cin.good() );

  streamoff n = p2 - p1;
  VERIFY( n > 0 );
	
  for (int i = 0; i < n; ++i)
    {
      streampos p3 = cin.tellg();
      VERIFY( streamoff(p3) == i );
      VERIFY( ftell(stdin) == i );
      cin.get();
      VERIFY( cin.good() );
    }

  streampos p4 = cin.tellg();
  VERIFY( streamoff(p4) == n );
  VERIFY( ftell(stdin) == n );
  cin.get();
  VERIFY( cin.eof() );
}

int main()
{
  test10();
  return 0;
}
