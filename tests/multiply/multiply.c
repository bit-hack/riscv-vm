// See LICENSE for license details.

// *************************************************************************
// multiply filter bencmark
// -------------------------------------------------------------------------
//
// This benchmark tests the software multiply implemenation. The
// input data (and reference data) should be generated using the
// multiply_gendata.pl perl script and dumped to a file named
// dataset1.h

static int multiply( int x, int y )
{

 int i;
 int result = 0;

 for (i = 0; i < 32; i++) {
   if ((x & 0x1) == 1)
     result = result + y;
       
   x = x >> 1;
   y = y << 1;
 } 
 
 return result;

}

static int verify(int n, const volatile int* test, const int* verify)
{
  int i;
  // Unrolled for faster verification
  for (i = 0; i < n/2*2; i+=2)
  {
    int t0 = test[i], t1 = test[i+1];
    int v0 = verify[i], v1 = verify[i+1];
    if (t0 != v0) return i+1;
    if (t1 != v1) return i+2;
  }
  if (n % 2 != 0 && test[n-1] != verify[n-1])
    return n;
  return 0;
}

//--------------------------------------------------------------------------
// Input/Reference Data

#include "dataset1.h"

//--------------------------------------------------------------------------
// Main

int main( int argc, char* argv[] )
{
  int i;
  int results_data[DATA_SIZE];

#if PREALLOCATE
  for (i = 0; i < DATA_SIZE; i++)
  {
    results_data[i] = multiply( input_data1[i], input_data2[i] );
  }
#endif

//  setStats(1);
  for (i = 0; i < DATA_SIZE; i++)
  {
    results_data[i] = multiply( input_data1[i], input_data2[i] );
  }
//  setStats(0);

  // Check the results
  return verify( DATA_SIZE, results_data, verify_data );
}
