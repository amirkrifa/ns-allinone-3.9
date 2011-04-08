#ifndef __MEDEHA_DTN_PEER_INFO_HEADER_H__
#define __MEDEHA_DTN_PEER_INFO_HEADER_H__

#define MEDEHA_DTN_HEADER   0x1F

#define IPV4_ADDRESS_SIZE 4
#define MEDEHA_DTN_PEER_INFO_MSG_HEADER_SIZE 4
#define MEDEHA_DTN_PEER_INFO_HEADER_SIZE 6

#include <stdint.h>
#include <vector>
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/medeha-ap-buffer.h"


namespace ns3 {

class MeDeHaDtnPeerInfoHeader : public Header
{
public:
  MeDeHaDtnPeerInfoHeader ();
  virtual ~MeDeHaDtnPeerInfoHeader ();

  void SetHeaderLength (uint8_t length)
  {
    m_headerLength = length;
  }
  uint16_t GetHeaderLength () const
  {
    return m_headerLength;
  }

  void SetHeaderType (uint8_t headerType)
  {
    m_headerType = headerType;
  }
  uint8_t GetHeaderType () const
  {
    return m_headerType;
  }
  
  void SetDestAddress (Ipv4Address destAddress)
  {
    m_destAddress = destAddress;
  }
  Ipv4Address GetDestAddress () const
  {
    return m_destAddress;
  }

private:
  uint8_t m_headerType;
  uint8_t m_headerLength;
  Ipv4Address m_destAddress;

public:  
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
};

class MeDeHaDtnMessageHeader : public Header
{
public:

  enum MessageType {
    CURRENT_PEERS = 1,
    RECENT_PEERS  = 2,
    HELLO_MSG = 3,
    MSG_VECTOR = 4,
  };

  MeDeHaDtnMessageHeader ();
  virtual ~MeDeHaDtnMessageHeader ();

  void SetMessageType (MessageType messageType)
  {
    m_messageType = messageType;
  }
  MessageType GetMessageType () const
  {
    return m_messageType;
  }

  void SetNumOfStations (uint8_t numSta)
  {
    m_numOfStas = numSta;
  }
  uint8_t GetNumOfStations () const
  {
    return m_numOfStas;
  }
  
private:
  MessageType m_messageType;
  uint8_t m_messageSize;
  uint8_t m_numOfStas;

public:  
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

  struct Interfaces
  {
    uint8_t m_numOfIfces;
    std::vector<Ipv4Address> interfaceAddresses;
  };

  struct Metric
  {
    uint8_t m_metricType;
    float m_metricValue;
  };

  struct StaList
  {
    uint8_t m_numOfIfces;
    std::vector<Ipv4Address> interfaceAddresses;
    uint8_t m_numOfMetrics;
    std::vector<Metric> m_metricList;
  };
  
  struct MessageVector
  {
    Ipv4Address srcAddress;
    Ipv4Address dstAddress;
    uint16_t seqNumber;
  };

  struct CurrentPeerInfo
  {
    std::vector<Interfaces> m_interfaces;

    void Print (std::ostream &os) const;
    uint32_t GetSerializedSize (void) const;
    void Serialize (Buffer::Iterator start) const;
    uint32_t Deserialize (Buffer::Iterator start, uint8_t numOfStas);
  };

  struct RecentPeerInfo
  {
    std::vector<StaList> m_staList;

    void Print (std::ostream &os) const;
    uint32_t GetSerializedSize (void) const;
    void Serialize (Buffer::Iterator start) const;
    uint32_t Deserialize (Buffer::Iterator start, uint8_t numOfStas);
  };
  
  struct MessageVectorList
  {
    std::vector<MessageVector> m_msgVectorList;
    uint16_t m_numOfMsgs;
    
    void SetNumOfMsgs (uint16_t numMsgs)
    {
      m_numOfMsgs = numMsgs;
    }
    
    uint16_t GetNumOfMsgs (void)
    {
      return m_numOfMsgs;
    }

    void Print (std::ostream &os) const;
    uint32_t GetSerializedSize (void) const;
    void Serialize (Buffer::Iterator start) const;
    uint32_t Deserialize (Buffer::Iterator start, uint8_t numOfStas);
  };
  
  struct HelloMessage
  {
    Interfaces m_interfaces;
    uint8_t m_associated;
    Ipv4Address m_associatedWith;
    BufferClass::BufferState m_bufferState;
    
    void SetAssociated (uint8_t associated)
    {
      m_associated = associated;
    }
    
    uint8_t GetAssociated (void)
    {
      return m_associated;
    }
    
    void SetAssociatedWith (Ipv4Address associatedWith)
    {
      m_associatedWith = associatedWith;
    }
    
    Ipv4Address GetAssociatedWith (void)
    {
      return m_associatedWith;
    }
    
    void SetBufferState (BufferClass::BufferState state)
    {
      m_bufferState = state;
    }
    BufferClass::BufferState GetBufferState (void)
    {
      return m_bufferState;
    }
    
    void Print (std::ostream &os) const;
    uint32_t GetSerializedSize (void) const;
    void Serialize (Buffer::Iterator start) const;
    uint32_t Deserialize (Buffer::Iterator start, uint8_t numOfStas);
  };

private:
  struct
  {
    CurrentPeerInfo currentPI;
    RecentPeerInfo recentPI;
    HelloMessage helloMsg;
    MessageVectorList msgVector;
  } m_message; // union not allowed

public:

  CurrentPeerInfo& GetCurrentPeerInfo ()
  {
    if (m_messageType == 0)
      {
        m_messageType = CURRENT_PEERS;
      }
    else
      {
        NS_ASSERT (m_messageType == CURRENT_PEERS);
      }
    return m_message.currentPI;
  }

  RecentPeerInfo& GetRecentPeerInfo ()
  {
    if (m_messageType == 0)
      {
        m_messageType = RECENT_PEERS;
      }
    else
      {
        NS_ASSERT (m_messageType == RECENT_PEERS);
      }
    return m_message.recentPI;
  }
  
  HelloMessage& GetHelloMessage ()
  {
    if (m_messageType == 0)
      {
        m_messageType = HELLO_MSG;
      }
    else
      {
        NS_ASSERT (m_messageType == HELLO_MSG);
      }
    return m_message.helloMsg;
  }
  
  MessageVectorList& GetMessageVectorList ()
  {
    if (m_messageType == 0)
      {
        m_messageType = MSG_VECTOR;
      }
    else
      {
        NS_ASSERT (m_messageType == MSG_VECTOR);
      }
    return m_message.msgVector;
  }

  const CurrentPeerInfo& GetCurrentPeerInfo () const
  {
    NS_ASSERT (m_messageType == CURRENT_PEERS);
    return m_message.currentPI;
  }

  const RecentPeerInfo& GetRecentPeerInfo () const
  {
    NS_ASSERT (m_messageType == RECENT_PEERS);
    return m_message.recentPI;
  }

  const HelloMessage& GetHelloMessage () const
  {
    NS_ASSERT (m_messageType == HELLO_MSG);
    return m_message.helloMsg;
  }
  
  const MessageVectorList& GetMessageVectorList () const
  {
    NS_ASSERT (m_messageType == MSG_VECTOR);
    return m_message.msgVector;
  }
};

static inline std::ostream& operator<< (std::ostream& os, const MeDeHaDtnPeerInfoHeader & packet)
{
  packet.Print (os);
  return os;
}

static inline std::ostream& operator<< (std::ostream& os, const MeDeHaDtnMessageHeader & message)
{
  message.Print (os);
  return os;
}

typedef std::vector<MeDeHaDtnMessageHeader> MessageList;

static inline std::ostream& operator<< (std::ostream& os, const MessageList & messages)
{
  os << "[";
  for (std::vector<MeDeHaDtnMessageHeader>::const_iterator messageIter = messages.begin ();
       messageIter != messages.end (); messageIter++)
    {
      messageIter->Print (os);
      if (messageIter+1 != messages.end ())
        os << ", ";
    }
  os << "]";
  return os;
}

}

#endif
