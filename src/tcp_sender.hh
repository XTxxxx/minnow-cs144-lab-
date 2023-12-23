#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"


class RetransTimer
  {
    bool is_on_;
    bool is_expired_;
    uint64_t time_cnt_;
    uint64_t RTO_ms_;
    public:
      friend class TCPSender;
      RetransTimer(); 
      void tick(uint64_t ms_since_last_tick);
      void doubleRTO();
      void resetRTO(uint64_t);
      void start();
      void stop();
      bool is_on();
      bool is_expired();
  };

using messageUnit = std::pair<uint64_t, TCPSenderMessage>;

class TCPSender
{
  bool FIN_acked_;
  uint16_t windowSize_;
  Wrap32 isn_;
  uint64_t first_index_;
  uint64_t last_acked_;
  uint64_t initial_RTO_ms_;
  uint64_t consecutive_retransmissions_;
  uint64_t sequence_numbers_in_flight_;
  std::priority_queue<messageUnit, std::vector<messageUnit>, std::greater<messageUnit>> messageQueue;
  std::priority_queue<messageUnit, std::vector<messageUnit>, std::greater<messageUnit>> outstandingQueue;
  RetransTimer retrans_timer_;
  void time_expire();
public:
  friend class RetransTimer;
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};

