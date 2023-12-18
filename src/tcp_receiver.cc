#include "tcp_receiver.hh"

using namespace std;

TCPReceiver::TCPReceiver() : ISN(), ACKNO(), started(false){}

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if (message.SYN) {
    started = true;
    ISN = message.seqno;
    ACKNO = message.seqno + 1;
  }
  if (started) {
    uint64_t checkpoint = inbound_stream.bytes_pushed();
    uint64_t firstIndex = message.seqno.unwrap(ISN.value(), checkpoint) - 1;
    if (message.SYN) {
      firstIndex += 1;
    }
    reassembler.insert(firstIndex, message.payload, message.FIN, inbound_stream);
    ACKNO = ACKNO.value() + (inbound_stream.bytes_pushed() - checkpoint);
    if (inbound_stream.is_closed()) {
      ACKNO = ACKNO.value() + 1;
    }
    send(inbound_stream);
  }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  uint64_t tmp = inbound_stream.available_capacity();
  uint16_t windowSize;
  windowSize = tmp > UINT16_MAX ? UINT16_MAX : (uint16_t)tmp;
  return {ACKNO, windowSize};  
}
