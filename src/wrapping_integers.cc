#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  //1 line
  return Wrap32{(uint32_t)((n + (uint64_t) zero_point.raw_value_) % (1UL << 32))};
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  //10 line
  (void) zero_point;
  uint64_t pow32 = (uint64_t) 1 << 32;
  uint64_t t = ((uint64_t) (this->raw_value_) + pow32 - zero_point.raw_value_) % pow32;
  //very important, or will exceed the limit
  if (t > checkpoint) {
    return t;
  }
  while (t < checkpoint) {
    t += pow32;
  }
  return t - checkpoint > checkpoint + pow32 - t ? t - pow32 : t;
}