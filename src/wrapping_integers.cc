#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32{(uint32_t)((n + (uint64_t) zero_point.raw_value_) % (1UL << 32))};
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t pow32 = (uint64_t) 1 << 32;
  uint64_t t = ((uint64_t) (this->raw_value_) + pow32 - zero_point.raw_value_) % pow32;
  //very important, or will exceed the limit
  if (checkpoint < t) {
    return t;
  }
  uint64_t k = (checkpoint - t) / pow32;
  if (checkpoint - (k * pow32 + t) < (k + 1) * pow32 + t - checkpoint) {
    return k * pow32 + t;
  } else {
    return (k + 1) * pow32 + t;
  }
}