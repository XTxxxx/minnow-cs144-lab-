#include <stdexcept>
#include <cstdint>
#include <algorithm>
#include <cstring>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ),
 available_capacity_( capacity ),
 bytes_buffered_(0),
 bytes_pushed_(0),
 bytes_popped_(0),
 peek_num_(128),
 closeBit(false),
 errorBit(false),
 buffer(""),
 peek_("") {}

void Writer::push( string data ) {
  uint64_t len = min(data.length(), available_capacity_);
  available_capacity_ -= len;
  bytes_buffered_ += len;
  bytes_pushed_ += len;
  buffer += data.substr(0, len);
  if (peek_.length() < peek_num_) {
    peek_ = buffer.substr(0, min(uint64_t(peek_num_), buffer.length()));
  }
}

void Writer::close()
{
  closeBit = true;
}

void Writer::set_error() {
  errorBit = true;
}

bool Writer::is_closed() const
{
  return closeBit;
}

uint64_t Writer::available_capacity() const
{
  return available_capacity_;
}
uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  return peek_;
}

bool Reader::is_finished() const
{
  return closeBit and bytes_buffered_ == 0;
}

bool Reader::has_error() const
{
  return errorBit;
}

void Reader::pop( uint64_t len )
{
  len = min(len, bytes_buffered_);
  buffer.erase(0, len);
  available_capacity_ += len;
  bytes_popped_ += len;
  bytes_buffered_ -= len;
  peek_ = buffer.substr(0, min(uint64_t(peek_num_), buffer.length()));
}

uint64_t Reader::bytes_buffered() const
{
  return bytes_buffered_;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
