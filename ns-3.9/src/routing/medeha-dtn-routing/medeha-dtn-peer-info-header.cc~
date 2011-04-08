#include "medeha-dtn-peer-info-header.h"
#include "ns3/log.h"

#include "ns3/address-utils.h"
#include "ns3/config.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MeDeHaDtnPeerInfoHeader");


// ---------------- OLSR Packet -------------------------------
NS_OBJECT_ENSURE_REGISTERED (MeDeHaDtnPeerInfoHeader);

MeDeHaDtnPeerInfoHeader::MeDeHaDtnPeerInfoHeader ()
{}

MeDeHaDtnPeerInfoHeader::~MeDeHaDtnPeerInfoHeader ()
{}

TypeId
MeDeHaDtnPeerInfoHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::olsr::MeDeHaDtnPeerInfoHeader")
    .SetParent<Header> ()
    .AddConstructor<MeDeHaDtnPeerInfoHeader> ()
    ;
  return tid;
}
TypeId
MeDeHaDtnPeerInfoHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t 
MeDeHaDtnPeerInfoHeader::GetSerializedSize (void) const
{
  return MEDEHA_DTN_PEER_INFO_HEADER_SIZE;
}

void 
MeDeHaDtnPeerInfoHeader::Print (std::ostream &os) const
{
  // TODO
}

void
MeDeHaDtnPeerInfoHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU8 (m_headerType);
  i.WriteU8 (m_headerLength);
  i.WriteU32 (m_destAddress.Get ());
}

uint32_t
MeDeHaDtnPeerInfoHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_headerType = i.ReadU8 ();
  m_headerLength  = i.ReadU8 ();
  m_destAddress = (Ipv4Address) i.ReadU32 ();
  return GetSerializedSize ();
}


// ---------------- OLSR Message -------------------------------

NS_OBJECT_ENSURE_REGISTERED (MeDeHaDtnMessageHeader);

MeDeHaDtnMessageHeader::MeDeHaDtnMessageHeader ()
  : m_messageType (MeDeHaDtnMessageHeader::MessageType (0))
{}

MeDeHaDtnMessageHeader::~MeDeHaDtnMessageHeader ()
{}

TypeId
MeDeHaDtnMessageHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::olsr::MeDeHaDtnMessageHeader")
    .SetParent<Header> ()
    .AddConstructor<MeDeHaDtnMessageHeader> ()
    ;
  return tid;
}
TypeId
MeDeHaDtnMessageHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
MeDeHaDtnMessageHeader::GetSerializedSize (void) const
{
  uint32_t size = MEDEHA_DTN_PEER_INFO_MSG_HEADER_SIZE;
  switch (m_messageType)
    {
    case CURRENT_PEERS:
      size += m_message.currentPI.GetSerializedSize ();
      break;
      
    case RECENT_PEERS:
      NS_LOG_DEBUG ("RecentPeerInfo Message Size: " << size << " + " << m_message.recentPI.GetSerializedSize ());
      size += m_message.recentPI.GetSerializedSize ();
      break;
      
    case HELLO_MSG:
      size += m_message.helloMsg.GetSerializedSize ();
      break;
      
    case MSG_VECTOR:
      size += m_message.msgVector.GetSerializedSize ();
      break;      
      
    default:
      NS_ASSERT (false);
    }
  return size;
}

void 
MeDeHaDtnMessageHeader::Print (std::ostream &os) const
{
  // TODO
}

void
MeDeHaDtnMessageHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU16 (m_messageType);
  i.WriteU8 (GetSerializedSize ());
  i.WriteU8 (m_numOfStas);

  switch (m_messageType)
    {
    case CURRENT_PEERS:
      m_message.currentPI.Serialize (i);
      break;
      
    case RECENT_PEERS:
      m_message.recentPI.Serialize (i);
      break;
      
    case HELLO_MSG:
      m_message.helloMsg.Serialize (i);
      break;
      
    case MSG_VECTOR:
      m_message.msgVector.Serialize (i);
      break;
      
    default:
      NS_ASSERT (false);
    }

}

uint32_t
MeDeHaDtnMessageHeader::Deserialize (Buffer::Iterator start)
{
  uint32_t size;
  Buffer::Iterator i = start;
  m_messageType  = (MessageType) i.ReadU16 ();
//  NS_ASSERT (m_messageType >= HELLO_MESSAGE && m_messageType <= HNA_MESSAGE);
  m_messageSize  = i.ReadU8 ();
  m_numOfStas  = i.ReadU8 ();
//  std::cout << "MeDeHaDtnMessageHeader::Deserialize, Message Type = " << m_messageType << std::endl;

  size = MEDEHA_DTN_PEER_INFO_MSG_HEADER_SIZE;
  switch (m_messageType)
    {
    case CURRENT_PEERS:
      size += m_message.currentPI.Deserialize (i, m_numOfStas);
      break;
    
    case RECENT_PEERS:
      size += m_message.recentPI.Deserialize (i, m_numOfStas);
      break;
    
    case HELLO_MSG:
      size += m_message.helloMsg.Deserialize (i, m_numOfStas);
      break;
      
    case MSG_VECTOR:
      size += m_message.msgVector.Deserialize (i, m_numOfStas);
      break;
      
    default:
      NS_ASSERT (false);
    }
  return size;
}


// ---------------- OLSR MID Message -------------------------------

uint32_t 
MeDeHaDtnMessageHeader::CurrentPeerInfo::GetSerializedSize (void) const
{
  uint32_t size = 0;
  for (std::vector<Interfaces>::const_iterator iter = this->m_interfaces.begin ();
       iter != this->m_interfaces.end (); iter++)
    {
      const Interfaces &interfaces = *iter;
      size += 1 + (interfaces.m_numOfIfces * IPV4_ADDRESS_SIZE);
    }

  return size;
}

void 
MeDeHaDtnMessageHeader::CurrentPeerInfo::Print (std::ostream &os) const
{
  // TODO
}

void
MeDeHaDtnMessageHeader::CurrentPeerInfo::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  for (std::vector<Interfaces>::const_iterator iter = this->m_interfaces.begin ();
       iter != this->m_interfaces.end (); iter++)
    {
      i.WriteU8 (iter->m_numOfIfces);
      std::vector<Ipv4Address> interfaceList = iter->interfaceAddresses;
      int count = 0;
      for (std::vector<Ipv4Address>::const_iterator iter2 = interfaceList.begin (); iter2 != interfaceList.end (); iter2++, count++)
        {
          i.WriteU32 (iter2->Get ());
        }
    }
}

uint32_t
MeDeHaDtnMessageHeader::CurrentPeerInfo::Deserialize (Buffer::Iterator start, uint8_t numOfStas)
{
  Buffer::Iterator i = start;

//  this->m_interfaces.interfaceAddresses.clear ();
  this->m_interfaces.clear ();  
//  NS_ASSERT (messageSize % IPV4_ADDRESS_SIZE == 0);
  
  for (int k=0; k<numOfStas; k++)
    {
      Interfaces interfaces;
      interfaces.m_numOfIfces = i.ReadU8 ();
      for (int j=0; j<interfaces.m_numOfIfces; j++)
        {
          interfaces.interfaceAddresses.push_back (Ipv4Address (i.ReadU32 ()));
        }
      this->m_interfaces.push_back (interfaces);
    }

  return GetSerializedSize ();
}



// ---------------- OLSR HELLO Message -------------------------------

uint32_t 
MeDeHaDtnMessageHeader::RecentPeerInfo::GetSerializedSize (void) const
{
  uint32_t size = 0;
  for (std::vector<StaList>::const_iterator iter = this->m_staList.begin ();
       iter != this->m_staList.end (); iter++)
    {
      const StaList &staList = *iter;
      size += 1 + (staList.m_numOfIfces * IPV4_ADDRESS_SIZE) + 1 + (staList.m_numOfMetrics * 5);
    }

  return size;
}

void 
MeDeHaDtnMessageHeader::RecentPeerInfo::Print (std::ostream &os) const
{
  // TODO
}

void
MeDeHaDtnMessageHeader::RecentPeerInfo::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  for (std::vector<StaList>::const_iterator iter = this->m_staList.begin ();
       iter != this->m_staList.end (); iter++)
    {
      i.WriteU8 (iter->m_numOfIfces);
      std::vector<Ipv4Address> interfaceList = iter->interfaceAddresses;
      uint16_t count = 0;
      for (std::vector<Ipv4Address>::const_iterator iter2 = interfaceList.begin ();
           count < iter->m_numOfIfces; iter2++, count++)
        {
          i.WriteU32 (iter2->Get ());
        }
      i.WriteU8 (iter->m_numOfMetrics);

      std::vector<Metric> metricList = iter->m_metricList;
      count = 0;
      for (std::vector<Metric>::const_iterator iter2 = metricList.begin ();
           count < iter->m_numOfMetrics; iter2++, count++)
        {
          i.WriteU8 (iter2->m_metricType);
          i.WriteU32 ((uint32_t)iter2->m_metricValue);
        }
    }
}

uint32_t
MeDeHaDtnMessageHeader::RecentPeerInfo::Deserialize (Buffer::Iterator start, uint8_t numOfStas)
{
  Buffer::Iterator i = start;

//  NS_ASSERT (messageSize >= 4);

//  this->m_staList.interfaceAddresses.clear ();
//  this->m_staList.m_metricList.clear ();
  this->m_staList.clear ();
  
  for (int k=0; k<numOfStas; k++)
    {
      StaList staList;
      staList.m_numOfIfces = i.ReadU8 ();
      for (int j=0; j<staList.m_numOfIfces; j++)
        {
          staList.interfaceAddresses.push_back (Ipv4Address (i.ReadU32 ()));
        }

      staList.m_numOfMetrics = i.ReadU8 ();

      for (int j=0; j<staList.m_numOfMetrics; j++)
        {
          Metric metric;
          metric.m_metricType = i.ReadU8 ();
          metric.m_metricValue = i.ReadU32 ();
          staList.m_metricList.push_back (metric);
        }
      this->m_staList.push_back (staList);
    }

  return GetSerializedSize ();
}


uint32_t 
MeDeHaDtnMessageHeader::HelloMessage::GetSerializedSize (void) const
{
  uint32_t size = 0;
  size += 1 + (m_interfaces.m_numOfIfces * IPV4_ADDRESS_SIZE);
  size += 7;

  return size;
}

void 
MeDeHaDtnMessageHeader::HelloMessage::Print (std::ostream &os) const
{
  // TODO
}

void
MeDeHaDtnMessageHeader::HelloMessage::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  i.WriteU8 (m_interfaces.m_numOfIfces);
  std::vector<Ipv4Address> interfaceList = m_interfaces.interfaceAddresses;
  int count = 0;
  for (std::vector<Ipv4Address>::const_iterator iter2 = interfaceList.begin (); iter2 != interfaceList.end (); iter2++, count++)
    {
      i.WriteU32 (iter2->Get ());
    }
    
  i.WriteU8 (m_associated);
  i.WriteU32 (m_associatedWith.Get ());
  i.WriteU16 (m_bufferState);
}

uint32_t
MeDeHaDtnMessageHeader::HelloMessage::Deserialize (Buffer::Iterator start, uint8_t numOfStas)
{
  Buffer::Iterator i = start;

//  NS_ASSERT (messageSize % IPV4_ADDRESS_SIZE == 0);
  
  m_interfaces.m_numOfIfces = i.ReadU8 ();
  for (int j=0; j<m_interfaces.m_numOfIfces; j++)
    {
      m_interfaces.interfaceAddresses.push_back (Ipv4Address (i.ReadU32 ()));
    }
  
  m_associated = i.ReadU8 ();
  m_associatedWith = (Ipv4Address) i.ReadU32 ();
  m_bufferState = (BufferClass::BufferState) i.ReadU16 ();

  return GetSerializedSize ();
}


uint32_t 
MeDeHaDtnMessageHeader::MessageVectorList::GetSerializedSize (void) const
{
  uint32_t size = 2;
  for (std::vector<MessageVector>::const_iterator iter = this->m_msgVectorList.begin ();
       iter != this->m_msgVectorList.end (); iter++)
    {
      size += 10;
    }

  return size;
}

void 
MeDeHaDtnMessageHeader::MessageVectorList::Print (std::ostream &os) const
{
  // TODO
}

void
MeDeHaDtnMessageHeader::MessageVectorList::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  i.WriteU16 (m_numOfMsgs);
  int count = 0;
  for (std::vector<MessageVector>::const_iterator iter2 = m_msgVectorList.begin (); iter2 != m_msgVectorList.end (); iter2++, count++)
    {
      i.WriteU32 (iter2->srcAddress.Get ());
      i.WriteU32 (iter2->dstAddress.Get ());
      i.WriteU16 (iter2->seqNumber);
    }  
}

uint32_t
MeDeHaDtnMessageHeader::MessageVectorList::Deserialize (Buffer::Iterator start, uint8_t numOfStas)
{
  Buffer::Iterator i = start;

//  NS_ASSERT (messageSize % IPV4_ADDRESS_SIZE == 0);
  
  m_numOfMsgs = i.ReadU16 ();
  for (int j=0; j<m_numOfMsgs; j++)
    {
      MessageVector msgVector;
      msgVector.srcAddress = Ipv4Address (i.ReadU32 ());
      msgVector.dstAddress = Ipv4Address (i.ReadU32 ());
      msgVector.seqNumber = i.ReadU16 ();
      m_msgVectorList.push_back (msgVector);
    }

  return GetSerializedSize ();
}

}
