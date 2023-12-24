#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <algorithm>

#include <random>

using namespace std;

void readHelper( Reader& reader, uint64_t len, std::string& out )
{ //a copy from byte_stream_helpers.cc
  out.clear();

  while ( reader.bytes_buffered() and out.size() < len ) {
    auto view = reader.peek();

    if ( view.empty() ) {
      throw std::runtime_error( "Reader::peek() returned empty string_view" );
    }

    view = view.substr( 0, len - out.size() ); // Don't return more bytes than desired.
    out += view;
    reader.pop( view.size() );
  } 
}


/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  :
  FIN_acked_(false),
  win_empty_(false),
  windowSize_(1),
  isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ),
  first_index_(0),
  last_acked_(0),
  initial_RTO_ms_( initial_RTO_ms ),
  consecutive_retransmissions_ (0),
  sequence_numbers_in_flight_(0),
  messageQueue(priority_queue<messageUnit, vector<messageUnit>, greater<messageUnit>>{}),
  outstandingQueue(priority_queue<messageUnit, vector<messageUnit>, greater<messageUnit>>{}),
  retrans_timer_(initial_RTO_ms_)
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return sequence_numbers_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if (!messageQueue.empty()) {
    pair<uint64_t, TCPSenderMessage> toSend = messageQueue.top();
    messageQueue.pop();
    outstandingQueue.push(toSend);
    if (!retrans_timer_.is_on()) {
      retrans_timer_.start();
      // retrans_timer_.resetRTO(initial_RTO_ms_);
    }
    return toSend.second;
  } else {
    return nullopt;
  }
}

//stop when outbound_stream is empty or windowSize is 0
//special case when windowSize is 0 at first
void TCPSender::push( Reader& outbound_stream )
{
  if (win_empty_ && sequence_numbers_in_flight_ == 0) { //assume windowSize is 1
    if (first_index_ == 0) {
      string data = "";
      TCPSenderMessage newMessage = {isn_, true, data, false};
      messageQueue.push({first_index_, newMessage});
      sequence_numbers_in_flight_ += newMessage.sequence_length();
      first_index_ += 1;
    } else if (outbound_stream.bytes_buffered() == 0 && outbound_stream.is_finished()){
      TCPSenderMessage newMessage = {Wrap32::wrap(first_index_, isn_), false, string{""}, true};
      messageQueue.push({first_index_, newMessage});
      sequence_numbers_in_flight_ += newMessage.sequence_length();
      first_index_ += 1;
      FIN_acked_ = true;
    } else if (outbound_stream.bytes_buffered()) {
      string data = "";
      readHelper(outbound_stream, 1, data);
      TCPSenderMessage newMessage = {Wrap32::wrap(first_index_, isn_), false, data, false};
      messageQueue.push({first_index_, newMessage});
      sequence_numbers_in_flight_ += newMessage.sequence_length();
      first_index_ += newMessage.sequence_length();
    }
  }
  bool signal_need_handled = (!FIN_acked_ && outbound_stream.is_finished()) || first_index_ == 0;
  while (windowSize_ > 0) {
    if (!signal_need_handled && outbound_stream.bytes_buffered() == 0) { //SYN handled above
      break;
    }
    int length = min(windowSize_, (uint16_t)TCPConfig::MAX_PAYLOAD_SIZE);
    string data;
    if (first_index_ == 0) {
      length -= 1; 
    }
    readHelper(outbound_stream, length, data);
    TCPSenderMessage newMessage = {Wrap32::wrap(first_index_, isn_), first_index_ == 0, data, false};
    windowSize_ -= newMessage.sequence_length();
    if (windowSize_ > 0 && outbound_stream.bytes_buffered() == 0 && outbound_stream.is_finished()){
      signal_need_handled = false;
      FIN_acked_ = true;
      newMessage.FIN = true;
      windowSize_ -= 1;
    }
    messageQueue.push({first_index_, newMessage});
    sequence_numbers_in_flight_ += newMessage.sequence_length();
    first_index_ += newMessage.sequence_length();
  }
  
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return {Wrap32::wrap(first_index_, isn_), false, string{""}, false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.window_size == 0) {
    win_empty_ = true;
  } else {
    win_empty_ = false;
  }
  if (msg.ackno.has_value()) {
    uint64_t cur_acked = msg.ackno.value().unwrap(isn_, last_acked_);
    if (cur_acked > first_index_) {
      return;
    }
    if (cur_acked > last_acked_) {
      last_acked_ = cur_acked;
    }
  }
  bool flag = false;
  while (!outstandingQueue.empty() && outstandingQueue.top().first + outstandingQueue.top().second.sequence_length() - 1 < last_acked_) {
    flag = true;
    sequence_numbers_in_flight_ -= outstandingQueue.top().second.sequence_length();
    outstandingQueue.pop();
  }
  if (flag) {
    retrans_timer_.resetRTO(initial_RTO_ms_);
    if (!outstandingQueue.empty()) {
      retrans_timer_.start();
    }
    consecutive_retransmissions_ = 0;
  }
  if (outstandingQueue.empty()) {
    retrans_timer_.stop();
  }
  if (msg.window_size > sequence_numbers_in_flight_) {
    windowSize_ = msg.window_size - sequence_numbers_in_flight_;
  } else {
    windowSize_ = 0;
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  retrans_timer_.tick(ms_since_last_tick);
  if (retrans_timer_.is_expired()) {
      time_expire();
  }
}

void TCPSender::time_expire() {
  pair<uint64_t, TCPSenderMessage> toSend = outstandingQueue.top();
  outstandingQueue.pop();
  messageQueue.push(toSend);
  if (!win_empty_ && sequence_numbers_in_flight_ != 0) {
    consecutive_retransmissions_++;
    retrans_timer_.doubleRTO();
  }
  retrans_timer_.start();
}

//below are definitions of RetransTimer

RetransTimer::RetransTimer(uint64_t initial_RTO_ms_) : is_on_( false ), is_expired_(false), time_cnt_( 0 ), RTO_ms_( initial_RTO_ms_ ) {}

void RetransTimer::tick(uint64_t ms_since_last_tick) {
  if (!is_on_) {
    return;
  }
  time_cnt_ += ms_since_last_tick;
  if (time_cnt_ >= RTO_ms_) {
    is_expired_ = true;
    time_cnt_ = 0;
  }
}

void RetransTimer::doubleRTO() {
  RTO_ms_ *= 2;
}

void RetransTimer::resetRTO(uint64_t initial_RTO_ms_) {
  RTO_ms_ = initial_RTO_ms_;
}


void RetransTimer::start() {
  is_on_ = true;
  time_cnt_ = 0;
  is_expired_ = false;
}

void RetransTimer::stop() {
  is_on_ = false;
}

bool RetransTimer::is_expired() {
  return is_expired_;
}

bool RetransTimer::is_on() {
  return is_on_;
}