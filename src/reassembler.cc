#include "reassembler.hh"

using namespace std;

Reassembler::Reassembler() : checkpoint_(0), bytesPendging_(0), dataList(std::list<dataNode>{}) {}

void Reassembler::writeIfPossible(Writer& output) {
  auto i = dataList.begin();
  while(i->first_index == checkpoint_ && i != dataList.end()) {
    output.push(i->data);
    checkpoint_ += i->data.length();
    bytesPendging_ -= i->data.length();
    if (i -> is_last_substring) {
      output.close();
    }
    i = dataList.erase(i);
  }
}

void Reassembler::insertHelper(uint64_t first_index, string data, bool is_last_substring, Writer& output) {
  uint64_t last_index = first_index + data.length() - 1;
  auto i = dataList.begin();
  while (i != dataList.end()) {
    if (last_index < i->first_index) { //case 1
      dataList.insert(i, {first_index, last_index, data, is_last_substring || i -> is_last_substring});
      bytesPendging_ += data.length();
      break;
    }

    if (first_index > i->last_index) { //case 2
      i++;
      continue;
    }

    if (first_index <= i->first_index && last_index <= i->last_index) { //case 3
      int addLen = i->first_index - first_index;
      i->first_index = first_index;
      i->data = data.substr(0, addLen) + i->data; 
      i->is_last_substring = is_last_substring || i -> is_last_substring;
      bytesPendging_ += addLen;
      break;
    }

    if (first_index > i -> first_index && last_index < i->last_index) { //case 4
      break;
    }

    if (first_index > i -> first_index && last_index >= i -> last_index) { //case 5
      bytesPendging_ -= i->data.length();
      int len = first_index - i->first_index;
      first_index = i->first_index;
      data = i->data.substr(0, len) + data;
      is_last_substring = is_last_substring || i->is_last_substring;
      i = dataList.erase(i);
    }

    if (first_index <= i->first_index && last_index > i -> last_index) { //case 6
      bytesPendging_ -= i->data.length();
      is_last_substring = is_last_substring || i->is_last_substring;
      i = dataList.erase(i);
    }
  }

  if (i == dataList.end()) {
    dataList.push_back({first_index, last_index, data, is_last_substring});
    bytesPendging_ += data.length();
  }
  writeIfPossible(output);
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output)
{
  if (data.length() == 0) {
    if (is_last_substring) {
      output.close();
    }
    return;
  }
  uint64_t last_index = first_index + data.length() - 1;
  //pre handle1
  //writtren data excluded
  if (last_index < checkpoint_) {
    return;
  } else if (checkpoint_ > first_index) {
    data = data.substr(checkpoint_ - first_index); 
    first_index = checkpoint_;
  }
  //pre handle2
  //out of range data excluded
  uint64_t maxIndexAccepted = output.available_capacity() + checkpoint_ - 1;
  if (maxIndexAccepted < first_index) {
    return;
  } else if (maxIndexAccepted < last_index) {
    data = data.substr(0, maxIndexAccepted - first_index + 1);
    last_index = maxIndexAccepted;
  }
  //insert process
  //pass the ball to helper function
  insertHelper(first_index, data, is_last_substring, output);
}

uint64_t Reassembler::bytes_pending() const
{
  return bytesPendging_;
}
