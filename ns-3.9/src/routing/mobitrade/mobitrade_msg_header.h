#ifndef MOBITRADE_MSG_HEADER
#define  MOBITRADE_MSG_HEADER

#define DESCRIPTION_LENGTH 300
#define DATA_LENGTH 300

#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/udp-header.h"

namespace ns3 {

const uint32_t HELLO_MSG = 0;
const uint32_t INTEREST_MSG  = 1;
const uint32_t CONTENT_MSG = 2;
const uint32_t END_SESSION_MSG = 3;


class MOBITRADEMsgHeader : public Header
{
public:

	MOBITRADEMsgHeader ();
  ~MOBITRADEMsgHeader ();

  static TypeId GetTypeId (void);
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;
  virtual TypeId GetInstanceTypeId (void) const;

  inline void SetMsgType(uint16_t t)
  {
    msgType = t; 
  };

  inline void SetMsgContentSource(Ipv4Address s)
  {
    contentSource = s; 
  };

  inline void SetMsgTTL(uint32_t ttl)
  {
    msgTTL = ttl; 
  };
  inline uint16_t GetMessageType () const
  {
    return msgType;
  }
  inline Ipv4Address GetDestAddress() const
  {
    return destAddress;
  }

  inline void SetDestAddress (Ipv4Address adr)
  {
    destAddress = adr;
  }

private:

  uint32_t msgType;

  // Original source of the content message
  Ipv4Address contentSource;
  
  // The msg TTL  
  uint32_t msgTTL;
  Ipv4Address destAddress;
};


}
#endif
