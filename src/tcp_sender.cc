#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;


void readHelper( Reader& reader, uint64_t len, std::string& out )
{ //copy from byte_stream_helpers.cc
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
  :isFirst_(false),
  windowSize_(0),
  isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ),
  initial_RTO_ms_( initial_RTO_ms ),
  current_RTO_ms_( initial_RTO_ms ),
  messageQueue(queue<TCPSenderMessage>{}),
  retrans_timer_()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return {};
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if (!messageQueue.empty()) {
    TCPSenderMessage toSend = messageQueue.front();
    messageQueue.pop();
    return toSend;
  } else {
    return nullopt;
  }
}

//stop when outbound_stream is empty or windowSize is 0
//special case when windowSize is 0 at first
void TCPSender::push( Reader& outbound_stream )
{
  if (windowSize_ == 0 && outbound_stream.bytes_buffered() > 0) { //assume windowSize is 1
    string data;
    readHelper(outbound_stream, 1, data);
    TCPSenderMessage newMessage = {isn_, isFirst_, data, outbound_stream.bytes_buffered() == 0};
    messageQueue.push(newMessage);
    isn_ = isn_ + newMessage.sequence_length();
    isFirst_ = false;
    return;
  }
  while (windowSize_ > 0 && outbound_stream.bytes_buffered() > 0) {
    string data;
    readHelper(outbound_stream, TCPConfig::MAX_PAYLOAD_SIZE, data);
    TCPSenderMessage newMessage = {isn_, isFirst_, data, outbound_stream.bytes_buffered() == 0};
    messageQueue.push(newMessage);
    isn_ = isn_ + newMessage.sequence_length();
    isFirst_ = false;
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  string data = "";
  return {isn_, false, data, false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
}

RetransTimer::RetransTimer() : is_on( false ), time_cnt( 0 ), RTO_ms_( 0 ) {}

void RetransTimer::tick(uint64_t ms_since_last_tick) {
  time_cnt += ms_since_last_tick;
  if (time_cnt >= RTO_ms_) {
    expire();
  }
}

void RetransTimer::setRTO(uint64_t value) {
  RTO_ms_ = value;
}

void RetransTimer::expire() {
  
}