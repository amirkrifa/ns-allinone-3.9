 
#include "mobitrade_msg_header.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/simulator.h"

#include "ns3/random-variable.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/ipv4-header.h"
#include "ns3/address-utils.h"

#include "ns3/config.h"
#include "ns3/udp-header.h"

using namespace std;
namespace ns3{

NS_LOG_COMPONENT_DEFINE ("ns3::MOBITRADEMsgHeader");
NS_OBJECT_ENSURE_REGISTERED (MOBITRADEMsgHeader);

MOBITRADEMsgHeader::MOBITRADEMsgHeader ()
{
}

MOBITRADEMsgHeader::~MOBITRADEMsgHeader ()
{
}

TypeId MOBITRADEMsgHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MOBITRADEMsgHeader").SetParent<Header> ().AddConstructor<MOBITRADEMsgHeader> ();
  return tid;
} 

uint32_t MOBITRADEMsgHeader::GetSerializedSize (void) const
{
  return sizeof(uint32_t) + sizeof(Ipv4Address) + sizeof(uint32_t) + sizeof(Ipv4Address);

}

void MOBITRADEMsgHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU32(msgType);
  i.WriteU32 (contentSource.Get());
  i.WriteU32 (msgTTL);
  i.WriteU32 (destAddress.Get());
}

uint32_t MOBITRADEMsgHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  
  msgType = i.ReadU32 ();
  contentSource = (Ipv4Address)i.ReadU32 ();
  msgTTL = i.ReadU32 ();
  destAddress = (Ipv4Address)i.ReadU32 ();
  return i.GetDistanceFrom (start);
}

TypeId MOBITRADEMsgHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void MOBITRADEMsgHeader::Print (std::ostream &os) const
{

}
}
