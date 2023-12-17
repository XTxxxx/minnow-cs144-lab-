#include "tcp_receiver.hh"

using namespace std;

TCPReceiver::TCPReceiver() : ISN(0), started(false), windowSize(0) {}

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if (message.SYN) {
    started = true;
    ISN = message.seqno;
  }
  if (started) {
    uint64_t checkpoint = inbound_stream.bytes_pushed();
    uint64_t firstIndex = message.seqno.unwrap(ISN, checkpoint) - 1;
    reassembler.insert(firstIndex, message.payload, message.FIN, inbound_stream);
    windowSize = inbound_stream.available_capacity() - checkpoint + 1;
    send(inbound_stream);
  }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  return {Wrap32(inbound_stream.bytes_pushed()), windowSize};
}
