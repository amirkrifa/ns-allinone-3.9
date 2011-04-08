
#ifndef __MEDEHA_DTN_ROUTING_H__
#define __MEDEHA_DTN_ROUTING_H__

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
#include "ns3/medeha-dtn-peer-info-header.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/medeha-ap-buffer.h"
#include "ns3/udp-header.h"


#include <vector>
#include <map>

#define HELLO_MESSAGE  0x10
#define METRIC_ENCOUNTER_TIME     0x20
#define METRIC_ENCOUNTER_NUMBERS  0x21

namespace ns3 {

class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;
class Ipv4RoutingTableEntry;
class Node;

class MeDeHaDTNPacketHeader : public Header 
{
public:

  MeDeHaDTNPacketHeader ();
  ~MeDeHaDTNPacketHeader ();

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  
  uint32_t GetSize (void) const;
  
  uint16_t GetMsgType (void) { return m_msgType; }
  void SetMsgType (uint16_t msgType) { m_msgType = msgType; }
  uint16_t GetNumOfAddresses (void) { return m_numOfAddresses; }
  void SetNumOfAddresses (uint16_t numOfAddresses) { m_numOfAddresses = numOfAddresses; }
  uint16_t GetAssociatedFlag (void) { return m_associated; }
  void SetAssociatedFlag (uint16_t associated) { m_associated = associated; }
  Ipv4Address GetAssociatedWith (void) { return m_associatedWith; }
  void SetAssociatedWith (Ipv4Address associatedWith) { m_associatedWith = associatedWith; }
  Ipv4Address GetIPAddress01 (void) { return m_IPAddress01; }
  void SetIPAddress01 (Ipv4Address IPAddress) { m_IPAddress01 = IPAddress; }
  Ipv4Address GetIPAddress02 (void) { return m_IPAddress02; }
  void SetIPAddress02 (Ipv4Address IPAddress) { m_IPAddress02 = IPAddress; }

private:
  uint16_t m_msgType;
  uint16_t m_numOfAddresses;
  uint16_t m_associated;
  Ipv4Address m_associatedWith;
  Ipv4Address m_IPAddress01;
  Ipv4Address m_IPAddress02;
};

class MeDeHaDtnRouting : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId (void);

  MeDeHaDtnRouting ();
  virtual ~MeDeHaDtnRouting ();

  void SetMainInterface (uint32_t interface);

private:
  /// HELLO messages' emission interval.
  Time m_helloInterval;
  Ptr<Ipv4> m_ipv4;
	
private:
  void Start ();
  void Clear ();

public:
  // From Ipv4RoutingProtocol
  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, uint32_t oif, Socket::SocketErrno &sockerr);
   virtual bool RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                             UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                             LocalDeliverCallback lcb, ErrorCallback ecb);  
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);

  void AddHostRouteTo (Ipv4Address dest, 
                       Ipv4Address nextHop, 
                       uint32_t interface);

  void AddHostRouteTo (Ipv4Address dest, 
                       uint32_t interface);

  void SetDefaultRoute (Ipv4Address nextHop, 
                        uint32_t interface);

  uint32_t GetNRoutes (void);

  Ipv4RoutingTableEntry GetDefaultRoute (void);

  Ipv4RoutingTableEntry GetRoute (uint32_t i);

  void RemoveRoute (uint32_t i);
  
  typedef std::list<Ipv4RoutingTableEntry *> HostRoutes;
  typedef std::list<Ipv4RoutingTableEntry *>::const_iterator HostRoutesCI;
  typedef std::list<Ipv4RoutingTableEntry *>::iterator HostRoutesI;  
  
  Ptr<Ipv4Route> LookupMeDeHaDtn (Ipv4Address dest);

  Ipv4Address SourceAddressSelection (uint32_t interface, Ipv4Address dest);

private:
  HostRoutes m_hostRoutes;
  Ipv4RoutingTableEntry *m_defaultRoute;
  
  struct ContactTable
  {
    Ipv4Address staAddress;
    Ipv4Address staAddressAlternate;
    uint16_t numOfEncounters;
    Time encounterTime;
  };
  
  typedef std::list<struct ContactTable> CTble;
  typedef std::list<struct ContactTable>::iterator CTbleI;
  CTble m_contactTable;
  
  struct RoutingTable
  {
    Ipv4Address staAddress;
    Ipv4Address staAddressAlternate;
    Ipv4Address nextHop;
    Time entryTime;
    bool immediateNeighbor;
  };
  
  struct VectorSummery
  {
    Ipv4Address srcAddress;
    Ipv4Address dstAddress;
    uint16_t seqNumber;
  };
  
  typedef std::list<struct VectorSummery> VSummery;
  typedef std::list<struct VectorSummery>::iterator VSummeryI;
  VSummery m_msgVectorSummeryList;
  
  typedef std::list<struct RoutingTable> RTble;
  typedef std::list<struct RoutingTable>::iterator RTbleI;
  RTble m_routingTable;
  
  // Timer handlers
  Timer m_helloTimer;
  Time m_niInterval;
  EventId m_niTimerEvent;

  Ipv4Address m_mainAddress;
  Ipv4Address m_addressAlternate;
  bool m_associated;
  Ipv4Address m_associatedWith;
  Ptr<Node> m_node;
  
  BufferClass buffer;
  EventId m_txTimerEvent;
  Time m_txTimer;
  EventId m_txTimerSourceEvent;
  Time m_txTimerSource;
  EventId m_txTimerMultiCopyEvent;
  Time m_txTimerMultiCopy;
  Ipv4Address m_destMultiCopy01;
  Ipv4Address m_destMultiCopy02;

  std::map< Ptr<Socket>, Ipv4Address > m_socketAddresses;

  TracedCallback <const MeDeHaDTNPacketHeader &> m_rxPacketTrace;
  TracedCallback <const MeDeHaDTNPacketHeader &> m_txPacketTrace;
  TracedCallback <uint32_t> m_routingTableChanged;
 
  void DoDispose ();

public:
  void SendPacket (Ptr<Packet> packet);
	
  void RecvPacket (Ptr<Socket> socket);

  Ipv4Address GetMainAddress (Ipv4Address iface_addr) const;
  bool GetAssociated (void)
  {
    return m_associated;
  }
  
  Ipv4Address GetAssociatedWith (void)
  {
    return m_associatedWith;
  }

  void HelloTimerExpire ();
  
  void SendHello ();
  void SendPeerInfo (Ipv4Address destAddress);

  void ProcessHello (MeDeHaDtnMessageHeader header);
  void ProcessCurrentPeerInfo (MeDeHaDtnMessageHeader header, Ipv4Address senderAddress);
  void ProcessRecentPeerInfo (MeDeHaDtnMessageHeader header, Ipv4Address senderAddress);
  void ProcessMessageVector (MeDeHaDtnMessageHeader msg);
		     
  void ConnectToCatchAssocEvent (void);
  void ReceiveAssocNotif (std::string context, Mac48Address apAddress);
  void ReceiveDisAssocNotif (std::string context, Mac48Address apAddress);
  
  bool RefreshEntryForDest (Ipv4Address destAddress, Ipv4Address nextHopAddr, Ipv4Address destAddressAlternate, bool iN);
  
  void HandleNITimer (void);

  void UpdateContactTable (Ipv4Address address01, Ipv4Address address02, bool status);
  void UpdateRoutingTable (Ipv4Address address01, Ipv4Address address02, Ipv4Address nextHop, bool iN);
  bool GetIPAddressForDtnMode (Ipv4Address inputAddress, Ipv4Address* outputAddress);
  void CheckForExpiredEntries (void);
  bool RemovingRoutingEntry (Ipv4Address destAddress);
  bool SearchForDestEntryAsImmediateNeighbor (Ipv4Address destAddress, Ipv4Address destAddressAlternate);
  bool SearchForDestEntryInRoutingTable (Ipv4Address destAddress);
  ContactTable* SearchForDestEntryInContactTable (Ipv4Address destAddress);
  void MeDeHaBufferPacketLocally (Ptr<const Packet> packet, Ipv4Header ipHeader, UdpHeader* udpHeader);
  bool RemoveDuplicateCopiesFromBuffer (BufferedFramesSt frame);
  void SendStoredFrames (Ptr<Ipv4L3Protocol> ipv4L3);
  int CountNumberOfStoredFrames (Ipv4Address destAddress);
  void HandleSendTimer (void);
  void HandleSourceStoredFrames (void);
  
  bool CompareSeqNumAndSendPacket (Ptr<Ipv4L3Protocol> ipv4L3, Ipv4Address destAddress01, Ipv4Address destAddress02);
  bool CheckCommonStoredPacket (Ipv4Address src, Ipv4Address dst, uint16_t seqNum);
  bool ForwardStoredFramesToAssociatedAPs (Ptr<Ipv4L3Protocol> ipv4L3);
  void RemoveRouteWithGWAsDest (Ipv4Address gatewayAddress);
  bool RemoveFromRoutingTable (Ipv4Address destAddress);
  
  void HandleMultiCopyTimer (void);
  
  bool CompareSocialAffiliationWithAP (Ipv4Address dstAddress, Ipv4Address apAddress);
  bool CompareSocialAffiliation (Ipv4Address dstAddress, Ipv4Address relayAddress);
  void ForwardStoredFramesToSocialNode (Ptr<Ipv4L3Protocol> ipv4L3, Ipv4Address relayAddress);
};

} // namespace ns3

#endif
