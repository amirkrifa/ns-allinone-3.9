
#define NS_LOG_APPEND_CONTEXT                                   \
  if (GetObject<Node> ()) { std::clog << "[node " << GetObject<Node> ()->GetId () << "] "; }
  
///
/// \brief Gets the delay between a given time and the current time.
///
/// If given time is previous to the current one, then this macro returns
/// a number close to 0. This is used for scheduling events at a certain moment.
///
#define DELAY(time) (((time) < (Simulator::Now ())) ? Seconds (0.000001) : \
                     (time - Simulator::Now () + Seconds (0.000001)))


/// Maximum allowed jitter.
#define MEDEHA_MAXJITTER		(m_helloInterval.GetSeconds () / 4)
#define JITTER (Seconds (UniformVariable().GetValue (0, MEDEHA_MAXJITTER)))

#define ENTRY_EXPIRE_TIME    Seconds (4)

#define  BUFFER
//#define  DEBUG
#define  RELAY_METRIC_USED
//#define  SOCIAL_AFFILIATION

#define  MEDEHA_MAX_NUM_FRAMES_TO_SEND  20

#include "medeha-dtn-routing.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/simulator.h"

#include "ns3/random-variable.h"
#include "ns3/inet-socket-address.h"
//#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-route.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/ipv4-header.h"
#include "ns3/address-utils.h"

#include "ns3/arp-l3-protocol.h"
#include "ns3/config.h"
//#include "ns3/simulator.h"
#include "ns3/medeha-handle-notification-ip.h"
#include "ns3/seq-header.h"
#include "ns3/udp-header.h"


NS_LOG_COMPONENT_DEFINE ("MeDeHaDtnRouting");


#define DTN_ROUTING_PORT_NUMBER  698

namespace ns3 {

class MeDeHaDTNPacketHeader;

MeDeHaDTNPacketHeader::MeDeHaDTNPacketHeader ()
  : m_msgType (0)
{}

MeDeHaDTNPacketHeader::~MeDeHaDTNPacketHeader ()
{}

TypeId 
MeDeHaDTNPacketHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MeDeHaDTNPacketHeader")
    .SetParent<Header> ()
    .AddConstructor<MeDeHaDTNPacketHeader> ()
    ;
  return tid;
}

TypeId 
MeDeHaDTNPacketHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void 
MeDeHaDTNPacketHeader::Print (std::ostream &os) const
{
  switch (m_msgType)
    {
    case HELLO_MESSAGE:
      break;
    }
}

uint32_t 
MeDeHaDTNPacketHeader::GetSerializedSize (void) const
{
//  return 12;
  return GetSize ();
}

uint32_t
MeDeHaDTNPacketHeader::GetSize (void) const
{
  uint32_t size = 0;
  if (m_numOfAddresses == 1)
    {
      size = 14;
    }
  else if (m_numOfAddresses == 2)
    {
      size = 18;
    }
  return size;
}

void 
MeDeHaDTNPacketHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU16 (m_msgType);
  i.WriteU16 (m_numOfAddresses);
  i.WriteU16 (m_associated);
  WriteTo (i, m_associatedWith);
  WriteTo (i, m_IPAddress01);
  if (m_numOfAddresses > 1)
    {
      WriteTo (i, m_IPAddress02);
    }
}

uint32_t
MeDeHaDTNPacketHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_msgType = i.ReadU16 ();
  m_numOfAddresses = i.ReadU16 ();
  m_associated = i.ReadU16 ();
  ReadFrom (i, m_associatedWith);
  ReadFrom (i, m_IPAddress01);
  if (m_numOfAddresses > 1)
    {
      ReadFrom (i, m_IPAddress02);
    }
    
  return i.GetDistanceFrom (start);
}

NS_OBJECT_ENSURE_REGISTERED (MeDeHaDtnRouting);

TypeId 
MeDeHaDtnRouting::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MeDeHaDtnRouting")
    .SetParent<Ipv4RoutingProtocol> ()
    .AddConstructor<MeDeHaDtnRouting> ()
    .AddAttribute ("HelloInterval", "HELLO messages emission interval.",
                   TimeValue (Seconds (2.0)),
                   MakeTimeAccessor (&MeDeHaDtnRouting::m_helloInterval),
                   MakeTimeChecker ())
    ;
  return tid;
}

//class MeDeHaDtnRouting;

MeDeHaDtnRouting::MeDeHaDtnRouting ()
  : m_ipv4 (0),
    m_helloTimer (Timer::CANCEL_ON_DESTROY),    
    m_associated (false)    
{
  m_defaultRoute = 0;
  m_niInterval = Seconds (5.0);
  m_txTimer = Seconds (0.05);
  m_txTimerSource = Seconds (0.05);
  m_txTimerMultiCopy = Seconds (0.05);
}

MeDeHaDtnRouting::~MeDeHaDtnRouting ()
{
  if (m_txTimerEvent.IsRunning ())
    {
      m_txTimerEvent.Cancel ();
    }
    
  if (m_txTimerSourceEvent.IsRunning ())
    {
      m_txTimerSourceEvent.Cancel ();
    }
    
  if (m_niTimerEvent.IsRunning ())
    {
      m_niTimerEvent.Cancel ();
    }  
}

void
MeDeHaDtnRouting::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);
  NS_LOG_DEBUG ("Created MeDeHaDtnRouting");
  m_helloTimer.SetFunction (&MeDeHaDtnRouting::HelloTimerExpire, this);

  m_ipv4 = ipv4;
  ConnectToCatchAssocEvent ();

  Simulator::ScheduleNow (&MeDeHaDtnRouting::Start, this);
}

void MeDeHaDtnRouting::DoDispose ()
{
  m_ipv4 = 0;

  for (std::map< Ptr<Socket>, Ipv4Address >::iterator iter = m_socketAddresses.begin ();
       iter != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();

  Ipv4RoutingProtocol::DoDispose ();
}

void MeDeHaDtnRouting::Start ()
{
  if (m_mainAddress == Ipv4Address ())
    {
      Ipv4Address loopback ("127.0.0.1");
      for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
        {
          // Use primary address, if multiple
          Ipv4Address addr = m_ipv4->GetAddress (i, 0).GetLocal ();
          if (addr != loopback)
            {
              m_mainAddress = addr;
              break;
            }
        }

      NS_ASSERT (m_mainAddress != Ipv4Address ());
    }

  NS_LOG_DEBUG ("Starting MeDeHa DTN Routing on node " << m_mainAddress);

  Ipv4Address loopback ("127.0.0.1");
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      Ipv4Address addr = m_ipv4->GetAddress (i, 0).GetLocal ();
      if (addr == loopback)
        continue;

      if (addr != m_mainAddress)
        {
          // Create never expiring interface association tuple entries for our
          // own network interfaces, so that GetMainAddress () works to
          // translate the node's own interface addresses into the main address.
//          IfaceAssocTuple tuple;
//          tuple.ifaceAddr = addr;
//          tuple.mainAddr = m_mainAddress;
//          AddIfaceAssocTuple (tuple);
//          NS_ASSERT (GetMainAddress (addr) == m_mainAddress);
        }

      // Create a socket to listen only on this interface
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (), 
        UdpSocketFactory::GetTypeId()); 
      socket->SetRecvCallback (MakeCallback (&MeDeHaDtnRouting::RecvPacket,  this));
      if (socket->Bind (InetSocketAddress (addr, DTN_ROUTING_PORT_NUMBER)))
        {
          NS_FATAL_ERROR ("Failed to bind() MeDeHa DTN Routing receive socket");
        }
      socket->Connect (InetSocketAddress (Ipv4Address (0xffffffff), DTN_ROUTING_PORT_NUMBER));
      m_socketAddresses[socket] = addr;
    }

//  HelloTimerExpire ();
    Time time = JITTER;
    m_helloTimer.Schedule (time);

  NS_LOG_DEBUG ("MeDeHa DTN Routing on node " << m_mainAddress << " started");
}

void MeDeHaDtnRouting::SetMainInterface (uint32_t interface)
{
  m_mainAddress = m_ipv4->GetAddress (interface, 0).GetLocal ();
}


void
MeDeHaDtnRouting::RecvPacket (Ptr<Socket> socket)
{
  Ptr<Packet> receivedPacket;
  Address sourceAddress;
  receivedPacket = socket->RecvFrom (sourceAddress);

  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
  Ipv4Address senderIfaceAddr = inetSourceAddr.GetIpv4 ();
  Ipv4Address receiverIfaceAddr = m_socketAddresses[socket];
  NS_ASSERT (receiverIfaceAddr != Ipv4Address ());
  NS_LOG_DEBUG ("DTN node " << m_mainAddress << " received a DTN message from " << senderIfaceAddr << " to " << receiverIfaceAddr);
  
  NS_ASSERT (inetSourceAddr.GetPort () == DTN_ROUTING_PORT_NUMBER);
  
  Ptr<Packet> packet = receivedPacket;

  MeDeHaDtnPeerInfoHeader header;
  packet->RemoveHeader (header);

#ifdef DEBUG
  std::cout << "Type = " << header.GetHeaderType () << " Length = " << header.GetHeaderLength () << "\n";
#endif

  int size = header.GetHeaderLength () - MEDEHA_DTN_PEER_INFO_HEADER_SIZE;
  
  if (header.GetDestAddress ().IsEqual (Ipv4Address (0xFFFFFFFF)) ||
      header.GetDestAddress ().IsEqual (m_mainAddress))
    {
      while (size > 0)
        {
          MeDeHaDtnMessageHeader msg;
          packet->RemoveHeader (msg);
  
          switch (msg.GetMessageType ())
            {      
              case MeDeHaDtnMessageHeader::CURRENT_PEERS:
                ProcessCurrentPeerInfo (msg, senderIfaceAddr);
                break;
	 
              case MeDeHaDtnMessageHeader::RECENT_PEERS:
                ProcessRecentPeerInfo (msg, senderIfaceAddr);
                break;
	
              case MeDeHaDtnMessageHeader::HELLO_MSG:
                ProcessHello (msg);
                break;
		
              case MeDeHaDtnMessageHeader::MSG_VECTOR:
#ifdef DEBUG
		std::cout << "Processing Message Vector Message\n";
#endif
                ProcessMessageVector (msg);
                break;
            }
          size -= msg.GetSerializedSize ();
        }
    }  
}


void
MeDeHaDtnRouting::ProcessHello (MeDeHaDtnMessageHeader msg)
{
//  m_associated = msg.GetAssociated ();
//  m_associatedWith = msg.GetAssociatedWith ();
    
  Ipv4Address address[2];
 
  MeDeHaDtnMessageHeader::HelloMessage hello = msg.GetHelloMessage ();
  std::vector<Ipv4Address>::iterator iter = hello.m_interfaces.interfaceAddresses.begin ();
  for (int i=0; i<hello.m_interfaces.m_numOfIfces; i++, iter++)
    {
      address[i] = *iter;
    }
    
#ifdef DEBUG
  std::cout << "\nDTN node: " << m_mainAddress << ", HELLO message is received and contains:\n";
  std::cout << "Associated Flag = " << hello.GetAssociated () << ",\nCorresponding AP = " << hello.GetAssociatedWith () << ",\nBuffer State = " << hello.GetBufferState () << ",\nNode Address 01 = " << address[0] << ",\nNode Address 02 = " << address[1] << "\n\n";
#endif
      
/*
  if (header.GetNumOfAddresses () == 2)
    {
      status = RefreshEntryForDest (header.GetIPAddress02 ());
    }
*/
  if (hello.GetBufferState () != BufferClass::MEMORY_FULL ||
      hello.GetBufferState () != BufferClass::MEMORY_CRITICAL)
    {
      UpdateRoutingTable (address[0], address[1], address[0], true);
  
      bool status = 0;
      status = RefreshEntryForDest (address[0], address[0], address[1], 1);
      UpdateContactTable (address[0], address[1], status);
      
      status = RefreshEntryForDest (address[1], address[0], address[0], 0);
      
      Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();
/*      
      if (m_niTimerEvent.IsRunning ())
	{
	  
	}
      else
	{
*/	  // inform AP about this new change
	  // has to check if state
	  if (m_associated)
	    {          
	      HandleNotificationsClass hNotif = ipv4L3->GetHandleNotificationClass ();
      
	      Ptr<Packet> packetToSend = Create<Packet> ();
      
	      Ipv4Address addr;
	      for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
		{
		  addr = m_ipv4->GetAddress (i, 0).GetLocal ();
		  Ipv4Address loopback ("127.0.0.1");
	  
		  if (addr != loopback &&
		      addr != m_mainAddress)
		    {
		      m_addressAlternate = addr; 	      
		      break;
		    }
		}

	      // header.GetIPAddress01 () signifies the ad hoc interface address of a station
	      // while sending information to an associated AP, we keep the infrastructure address "header.GetIPAddress02 ()" of the station first
              hNotif.SendNotificationToAPs (NEIGHBORHOOD_INFO_NOTIF, hello.m_interfaces.m_numOfIfces, addr, address[0], address[1], packetToSend);
              ipv4L3->Send (packetToSend, m_mainAddress, m_associatedWith, 17, NULL);
	    }
//	  m_niTimerEvent = Simulator::Schedule (m_niInterval, &MeDeHaDtnRouting::HandleNITimer, this);
//	}
    
      int numStoredFrames01 = CountNumberOfStoredFrames (address[0]);
      int numStoredFrames02 = CountNumberOfStoredFrames (address[1]);
      
#ifdef DEBUG
      std::cout << "MeDeHaDtnRouting::ProcessHello, Number of Stored Frames for dest = " << address[1] << " are " << numStoredFrames01 << "\n";
#endif
	
      // check if there are still some frames to be sent for the destination "tempAddress"
      if (numStoredFrames01 > 0 ||
          numStoredFrames02 > 0)
	{
	  if (m_txTimerEvent.IsRunning ())
	      m_txTimerEvent.Cancel ();
	  // send frames to the corresponding AP
	  SendStoredFrames (ipv4L3);
	  m_txTimerEvent = Simulator::Schedule (m_txTimer, &MeDeHaDtnRouting::HandleSendTimer, this);	// reschedule the timer for transmission of buffered frames
	}

      SendPeerInfo (address[0]);
    }
}

void
MeDeHaDtnRouting::ProcessCurrentPeerInfo (MeDeHaDtnMessageHeader msg, Ipv4Address senderAddress)
{
  MeDeHaDtnMessageHeader::CurrentPeerInfo currentPI = msg.GetCurrentPeerInfo ();
  
#ifdef DEBUG
  std::cout << "\nDTN node: " << m_mainAddress << ", CURRENT_PEER_INFO message is received from " << senderAddress << " and contains:\n";
  std::cout << "Number of Stations = " << (int) msg.GetNumOfStations () << "\n";
#endif

  std::vector<MeDeHaDtnMessageHeader::Interfaces>::iterator iterIfces = currentPI.m_interfaces.begin ();
  
  RemoveRouteWithGWAsDest (senderAddress);
  for (int i=0; i<msg.GetNumOfStations (); i++, iterIfces++)
    {
      Ipv4Address address[2];
      std::vector<Ipv4Address>::iterator iter = iterIfces->interfaceAddresses.begin ();
      for (int j=0; j<iterIfces->m_numOfIfces; j++, iter++)
        {	  
          address[j] = *iter;
	}
	
      if (address [0] == m_mainAddress ||
          address [1] == m_mainAddress)
	{
	  continue;
	}

#ifdef DEBUG
      std::cout << "address1: " << address[0] << " address2: " << address[1] << "\n";
#endif

      RefreshEntryForDest (address[0], senderAddress, address[1], 0);
      UpdateRoutingTable (address[0], address[1], senderAddress, 0);
      
      RefreshEntryForDest (address [1], senderAddress, address[0], 0);

      Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();

      int numStoredFrames01 = CountNumberOfStoredFrames (address[0]);
      int numStoredFrames02 = CountNumberOfStoredFrames (address[1]);
      
#ifdef DEBUG
      std::cout << "MeDeHaDtnRouting::ProcessCurrentPeerInfo, Number of Stored Frames for dest = " << address[1] << " are " << numStoredFrames01 << "\n";
#endif

      // check if there are still some frames to be sent for the destination "tempAddress"
      if (numStoredFrames01 > 0 ||
          numStoredFrames02 > 0)
        {
          if (m_txTimerEvent.IsRunning ())
              m_txTimerEvent.Cancel ();
          // send frames to the corresponding AP
          SendStoredFrames (ipv4L3);
          m_txTimerEvent = Simulator::Schedule (m_txTimer, &MeDeHaDtnRouting::HandleSendTimer, this);	// reschedule the timer for transmission of buffered frames
        }
	
#ifdef SOCIAL_AFFILIATION
#ifdef DEBUG
      std::cout << "Buffer Size at " << m_mainAddress << ": " << buffer.GetBufferSize () << std::endl;
#endif
      if (buffer.GetBufferSize ())
        {
          ForwardStoredFramesToSocialNode (ipv4L3, senderAddress);
	}
#endif
    }
}

void
MeDeHaDtnRouting::ProcessRecentPeerInfo (MeDeHaDtnMessageHeader msg, Ipv4Address senderAddress)
{
  MeDeHaDtnMessageHeader::RecentPeerInfo recentPI = msg.GetRecentPeerInfo ();

#ifdef DEBUG  
  std::cout << "\nDTN node: " << m_mainAddress << ", RECENT_PEER_INFO message is received from " << senderAddress << " and contains:\n";
  std::cout << "Number of Stations = " << (int) msg.GetNumOfStations () << "\n\n";
#endif

  std::vector<MeDeHaDtnMessageHeader::StaList>::iterator iterStaList = recentPI.m_staList.begin ();
  
  for (int i=0; i<msg.GetNumOfStations (); i++, iterStaList++)
    {
      Ipv4Address address[2];
      std::vector<Ipv4Address>::iterator iter = iterStaList->interfaceAddresses.begin ();
      for (int j=0; j<iterStaList->m_numOfIfces; j++, iter++)
        {
          address[j] = *iter;
	}
//      UpdateRoutingTable (address[0], address[1], senderAddress, 0);
//      RefreshEntryForDest (address [0], senderAddress, 0);
      
      std::vector<MeDeHaDtnMessageHeader::Metric>::iterator iter2 = iterStaList->m_metricList.begin ();
      int numEncounters = 0;
      Time encounterTime;
      
      for (int j=0; j<iterStaList->m_numOfMetrics; j++, iter2++)
        {
	  int type = iter2->m_metricType;
	  float value = iter2->m_metricValue;
	  
#ifdef DEBUG
	  std::cout << address[0] << ": type = " << type << "\t" << "value = " << value << "\n";
#endif

	  if (type == METRIC_ENCOUNTER_NUMBERS)
            {
              numEncounters = (int) value;
            }
	
          else if (type == METRIC_ENCOUNTER_TIME)
            {
              encounterTime = Seconds (value);
	    }
	}

#ifdef RELAY_METRIC_USED	      
      int numStoredFrames = CountNumberOfStoredFrames (address[0]);
      int numStoredFrames01 = CountNumberOfStoredFrames (address[1]);
      if (numStoredFrames > 0 ||
          numStoredFrames01 > 0)
        {
          Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();      
          MeDeHaDtnRouting::ContactTable* ct = SearchForDestEntryInContactTable (address[0]);

//          bool flag = CompareSocialAffiliation (address[0], senderAddress);
          if (ct != NULL)
	    {
              if ((ct->encounterTime < encounterTime ||
                   ct->numOfEncounters < numEncounters) &&
                   numEncounters >= 2)
//              if (ct->numOfEncounters < numEncounters &&
//                   numEncounters >= 1)// &&
//                   flag == true)
	        {
	          // Add route to this destination with senderAddress as next hop
	          // Send Stored Frames for address[0] to senderAddress
	          RefreshEntryForDest (address[0], senderAddress, address[1], 0);
                  UpdateRoutingTable (address[0], address[1], senderAddress, 0);
	      
	          RefreshEntryForDest (address [1], senderAddress, address[0], 0);

#ifdef DEBUG
	          std::cout << "Forwarding messages to relay 01 " << senderAddress << "\n";
#endif
                  m_destMultiCopy01 = address [0];
		  m_destMultiCopy02 = address [1];
                  CompareSeqNumAndSendPacket (ipv4L3, m_destMultiCopy01, m_destMultiCopy02);
//                  CompareSeqNumAndSendPacket (ipv4L3, address [1]);
	        }	  
            }
	  else
	    {
	      if (numEncounters >= 2)// &&
//                  flag == true)
	        {
		  RefreshEntryForDest (address [0], senderAddress, address[1], 0);
                  UpdateRoutingTable (address[0], address[1], senderAddress, 0);
	      
	          RefreshEntryForDest (address [1], senderAddress, address[0], 0);
	      
#ifdef DEBUG
	          std::cout << "Forwarding messages to relay 02 " << senderAddress << "\n";
#endif
                  m_destMultiCopy01 = address [0];
		  m_destMultiCopy02 = address [1];
                  CompareSeqNumAndSendPacket (ipv4L3, m_destMultiCopy01, m_destMultiCopy02);
//                  CompareSeqNumAndSendPacket (ipv4L3, address [1]);
		}
	    }
        }
#endif
    }
}

void
MeDeHaDtnRouting::ProcessMessageVector (MeDeHaDtnMessageHeader msg)
{
  MeDeHaDtnMessageHeader::MessageVectorList msgVector = msg.GetMessageVectorList ();
  
#ifdef DEBUG
  std::cout << "\nDTN node: " << m_mainAddress << ", MSG_VECTOR message is received and contains:\n";
  std::cout << "Number of Messages = " << msgVector.GetNumOfMsgs () << "\n";
#endif

  m_msgVectorSummeryList.clear ();
  std::vector<MeDeHaDtnMessageHeader::MessageVector>::iterator iterMsgVector = msgVector.m_msgVectorList.begin ();
  
  for (int i=0; i<msgVector.GetNumOfMsgs (); i++)
    {
      VectorSummery msgVectorSummery;
      msgVectorSummery.srcAddress = iterMsgVector->srcAddress;
      msgVectorSummery.dstAddress = iterMsgVector->dstAddress;
      msgVectorSummery.seqNumber = iterMsgVector->seqNumber;
      m_msgVectorSummeryList.push_back (msgVectorSummery);
    }  
}

void
MeDeHaDtnRouting::SendPacket (Ptr<Packet> packet)
{
  NS_LOG_DEBUG ("DTN Routing node " << m_mainAddress << " sending a DTN Routing packet");

  // Send it
  m_socketAddresses.begin ()->first->Send (packet);
}


void
MeDeHaDtnRouting::SendHello ()
{
  NS_LOG_FUNCTION (this);
  Ptr<Packet> packet = Create<Packet> ();
  
  uint16_t addressCounter = 0;
  uint16_t j=0;
  Ipv4Address ipv4AddressAlternate;
//  uint16_t interfaceId[2];
  
  Ipv4Address loopback ("127.0.0.1");
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      Ipv4Address addr = m_ipv4->GetAddress (i, 0).GetLocal ();
      if (addr != loopback &&
          addr != m_mainAddress)
        {
	  ipv4AddressAlternate = addr;
//	  interfaceId[j] = i;
	  addressCounter++;
	  j++;
	}
    }
 
  MeDeHaDtnMessageHeader msg;

  msg.SetNumOfStations (1);
  
  MeDeHaDtnMessageHeader::HelloMessage &helloMsg = msg.GetHelloMessage ();

  helloMsg.SetAssociated (m_associated);
  helloMsg.SetAssociatedWith (m_associatedWith);
  helloMsg.SetBufferState (buffer.GetMemoryState ());
  
  helloMsg.m_interfaces.m_numOfIfces = addressCounter+1;
  helloMsg.m_interfaces.interfaceAddresses.push_back (m_mainAddress);
  if (addressCounter > 0)
    {
      helloMsg.m_interfaces.interfaceAddresses.push_back (ipv4AddressAlternate);
    }

  int size = msg.GetSerializedSize ();  
  packet->AddHeader (msg);
  
  MeDeHaDtnPeerInfoHeader header;
  
  header.SetHeaderType (MEDEHA_DTN_HEADER);
  header.SetHeaderLength (size + MEDEHA_DTN_PEER_INFO_HEADER_SIZE);
  header.SetDestAddress (Ipv4Address (0xFFFFFFFF));
  
  packet->AddHeader (header);
  
/*  
  // Add a header
  MeDeHaDTNPacketHeader header;
  header.SetMsgType (HELLO_MESSAGE);
  header.SetNumOfAddresses (addressCounter+1);
  header.SetAssociatedFlag (m_associated);
  header.SetAssociatedWith (m_associatedWith);  
  header.SetIPAddress01 (m_mainAddress);
  if (addressCounter > 0)
    {
      header.SetIPAddress02 (ipv4AddressAlternate);
    

  
  packet->AddHeader (header);
*/  
//  std::cout << "Having " << addressCounter << " addresses, Sending MeDeHa Header Type = " << header.GetHeaderType () << "\n";
  
  SendPacket (packet);
}

void
MeDeHaDtnRouting::SendPeerInfo (Ipv4Address destAddress)
{
  Ptr<Packet> packet = Create<Packet> ();
  
  MeDeHaDtnMessageHeader msg01, msg02, msg03;
  int numOfStations = 0;
  int size = 0;

  numOfStations = 0;
  MeDeHaDtnMessageHeader::RecentPeerInfo &recentPI = msg02.GetRecentPeerInfo ();
  
  CTbleI iter;
  
  for (iter = m_contactTable.begin (); iter != m_contactTable.end (); iter++)
    {
      if (iter->staAddress == destAddress ||
          iter->staAddressAlternate == destAddress)
        {
          continue;
        }
	
      bool flag = false;
      flag = SearchForDestEntryInRoutingTable (iter->staAddress);
      
      if (flag)
        {
          continue;
        }
		    
      MeDeHaDtnMessageHeader::StaList staList;
      staList.interfaceAddresses.push_back (iter->staAddress);
      if (iter->staAddressAlternate.IsEqual ("102.102.102.102"))
        {
          staList.m_numOfIfces = 1; 
        }
      else
        {
          staList.interfaceAddresses.push_back (iter->staAddressAlternate);
          staList.m_numOfIfces = 2;
        }
	
      MeDeHaDtnMessageHeader::Metric metric;
      metric.m_metricType = METRIC_ENCOUNTER_NUMBERS;
      metric.m_metricValue = iter->numOfEncounters;
      staList.m_metricList.push_back (metric);
      
      metric.m_metricType = METRIC_ENCOUNTER_TIME;
      metric.m_metricValue = iter->encounterTime.GetSeconds ();
      staList.m_metricList.push_back (metric);

      staList.m_numOfMetrics = 2;
      
      recentPI.m_staList.push_back (staList);
      numOfStations++;
    }
    
  msg02.SetNumOfStations (numOfStations);
  
  size += msg02.GetSerializedSize ();
  packet->AddHeader (msg02);
  
  MeDeHaDtnMessageHeader::CurrentPeerInfo &currentPI = msg01.GetCurrentPeerInfo ();
  
  numOfStations = 0;
  RTbleI iterator;
   
  for (iterator = m_routingTable.begin (); iterator != m_routingTable.end (); iterator++)
    {
      if ((iterator->staAddress == destAddress &&
           iterator->immediateNeighbor) ||
          (iterator->staAddressAlternate == destAddress &&
           iterator->immediateNeighbor) ||
          iterator->nextHop == destAddress ||
          !iterator->immediateNeighbor)
        {
          continue;
        }
      MeDeHaDtnMessageHeader::Interfaces interfaces;
      interfaces.interfaceAddresses.push_back (iterator->staAddress);
      if (iterator->staAddressAlternate.IsEqual ("102.102.102.102"))
        {
          interfaces.m_numOfIfces = 1; 
        }
      else
        {
          interfaces.interfaceAddresses.push_back (iterator->staAddressAlternate);
          interfaces.m_numOfIfces = 2;
        }
      currentPI.m_interfaces.push_back (interfaces);
      numOfStations++;
    }
    
  msg01.SetNumOfStations (numOfStations);
  
  size += msg01.GetSerializedSize ();
  packet->AddHeader (msg01);
  
  int bufferSize = buffer.GetBufferSize ();
  numOfStations = 0;

  if (bufferSize > 0)
    {  
      MeDeHaDtnMessageHeader::MessageVectorList &msgVector = msg03.GetMessageVectorList ();  

      msgVector.SetNumOfMsgs (bufferSize); 
/*
      VSummeryI msgIter;
      for (msgIter = m_msgVectorSummeryList.begin (); msgIter != m_msgVectorSummeryList.end (); msgIter++)
        {
          MeDeHaDtnMessageHeader::MessageVector vecSum;
          vecSum.srcAddress = msgIter->srcAddress;
          vecSum.dstAddress = msgIter->dstAddress;
          vecSum.seqNumber = msgIter->seqNumber;
          msgVector.m_msgVectorList.push_back (vecSum);
        }
*/
      msgVector.m_msgVectorList.clear ();
      std::list<BufferedFramesSt>::iterator newIterator;		// link list iterator
      for (newIterator = buffer.m_pktList.begin(); newIterator != buffer.m_pktList.end(); newIterator++)
        {
          MeDeHaDtnMessageHeader::MessageVector vecSum;
          vecSum.srcAddress = newIterator->src;
          vecSum.dstAddress = newIterator->dst;
          vecSum.seqNumber = newIterator->seqNum;
          msgVector.m_msgVectorList.push_back (vecSum);
        }
		  	
      msg03.SetNumOfStations (numOfStations);
      
      size += msg03.GetSerializedSize ();
      packet->AddHeader (msg03);
    }  
  
  MeDeHaDtnPeerInfoHeader header;
  
  header.SetHeaderType (MEDEHA_DTN_HEADER);
  header.SetHeaderLength (size + MEDEHA_DTN_PEER_INFO_HEADER_SIZE);
  header.SetDestAddress (Ipv4Address (0xFFFFFFFF));

//  std::cout << "size is " << size << "\n";
      
  packet->AddHeader (header);

  SendPacket (packet);
}

///
/// \brief Sends a HELLO message and reschedules the HELLO timer.
/// \param e The event which has expired.
///
void
MeDeHaDtnRouting::HelloTimerExpire ()
{
  if (Simulator::Now ().GetSeconds () < 7198)
    {
      if (m_ipv4->GetNInterfaces () > 2 || m_associated == false)
        {
          SendHello ();
          Time time = m_helloInterval+JITTER;
//          std::cout << "Total = " << time << "\n";
          m_helloTimer.Schedule (time);
        }
    }  
}

///
/// \brief Clears the routing table and frees the memory assigned to each one of its entries.
///
void
MeDeHaDtnRouting::Clear ()
{
  NS_LOG_FUNCTION_NOARGS ();
//  m_table.clear ();
}

Ptr<Ipv4Route>
MeDeHaDtnRouting::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, uint32_t oif, Socket::SocketErrno &sockerr)
{  
  Ipv4Address destination = header.GetDestination ();
  Ptr<Ipv4Route> rtentry = 0;

  rtentry = LookupMeDeHaDtn (destination);
  if (rtentry)
    { 
      sockerr = Socket::ERROR_NOTERROR;
    }
  else
    { 
      sockerr = Socket::ERROR_NOROUTETOHOST;
    }
  return rtentry;
}

bool MeDeHaDtnRouting::RouteInput  (Ptr<const Packet> p, 
  const Ipv4Header &header, Ptr<const NetDevice> idev,
  UnicastForwardCallback ucb, MulticastForwardCallback mcb,             
  LocalDeliverCallback lcb, ErrorCallback ecb)
{   
  NS_LOG_FUNCTION (this << " " << m_ipv4->GetObject<Node> ()->GetId() << " " << header.GetDestination ());
//
// This is a unicast packet.  Check to see if we have a route for it.
//
  NS_LOG_LOGIC ("Unicast destination");
  Ptr<Ipv4Route> rtentry = LookupMeDeHaDtn (header.GetDestination ());
  if (rtentry != 0)
    {
      NS_LOG_LOGIC ("Found unicast destination- calling unicast callback");
      ucb (rtentry, p, header);  // unicast forwarding callback
      return true;
    }
  else
    {
      NS_LOG_LOGIC ("Did not find unicast destination- returning false");
      return false; // Let other routing protocols try to handle this
    }
}
void 
MeDeHaDtnRouting::NotifyInterfaceUp (uint32_t i)
{}
void 
MeDeHaDtnRouting::NotifyInterfaceDown (uint32_t i)
{}
void 
MeDeHaDtnRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{}
void 
MeDeHaDtnRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{}

void
MeDeHaDtnRouting::ConnectToCatchAssocEvent (void)
{
  m_node = m_ipv4->GetObject<Node> ();
  std::ostringstream oss;

  oss << "/NodeList/" << m_node->GetId () << "/DeviceList/*/Mac/$ns3::NqstaWifiMac/Assoc";
  Config::Connect (oss.str (), MakeCallback (&MeDeHaDtnRouting::ReceiveAssocNotif, this));

  std::ostringstream oss2;  
  oss2 << "/NodeList/" << m_node->GetId () << "/DeviceList/*/Mac/$ns3::NqstaWifiMac/DeAssoc";
  Config::Connect (oss2.str (), MakeCallback (&MeDeHaDtnRouting::ReceiveDisAssocNotif, this));
}

void
MeDeHaDtnRouting::ReceiveAssocNotif (std::string context, Mac48Address apAddress)
{
  Ptr<ArpL3Protocol> arp = m_ipv4->GetObject<ArpL3Protocol> ();
  
//  Ipv4Address ipv4AddressSta = arp->SearchForIpv4Address (staAddress);
  Ipv4Address ipv4AddressAP = arp->SearchForIpv4Address (apAddress);
  m_associatedWith = ipv4AddressAP;
  m_associated = true;

  uint32_t interface = 0;  
  uint16_t numOfIfces = m_ipv4->GetNInterfaces ();
  
  if (numOfIfces == 2)
    {
      interface = 1;
    }
  else 
    {
//      std::cout << "MeDeHaDtnRouting::ReceiveAssocNotif, main address is " << m_mainAddress << "\n";
      for (uint32_t i = 0; i < numOfIfces; i++)
        {
          Ipv4Address addr = m_ipv4->GetAddress (i, 0).GetLocal ();      
          Ipv4Address loopback ("127.0.0.1");
      
          if (addr != loopback &&
              addr != m_mainAddress)
            {
 	      interface = i;
//	      std::cout << "default interface = " << interface << " and address is " << addr << "\n";
	      break;
	    }
        }
    }
  
  SetDefaultRoute (ipv4AddressAP, interface);

#ifdef DEBUG
  std::cout << "MeDeHaDtnRouting::ReceiveAssocNotif (), Associated Event received: station mac = " << apAddress << ", station ip = " << ipv4AddressAP << "\n";
#endif
  
  Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();
  HandleNotificationsClass hNotif = ipv4L3->GetHandleNotificationClass ();
  
  Ptr<Packet> packetToSend = Create<Packet> ();
  
  Ipv4Address addr;
  if (numOfIfces == 2)
    {
      addr = m_mainAddress;
    }
  else 
    {
      for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
        {
          addr = m_ipv4->GetAddress (i, 0).GetLocal ();      
          Ipv4Address loopback ("127.0.0.1");
  
          if (addr != loopback &&
              addr != m_mainAddress)
            { 	      
              break;
	    }
        }
    }
    
  Time t = Simulator::Now ();

  hNotif.SendNotificationToAPs (STA_ASS_NOTIF, 2, ipv4AddressAP, addr, m_mainAddress, packetToSend);
  ipv4L3->Send (packetToSend, m_mainAddress, m_associatedWith, 17, NULL);

#ifdef DEBUG  
  std::cout << "MeDeHaDtnRouting::ReceiveDisAssocNotif, buffer size is " << buffer.m_listSize << std::endl;
#endif

  if (buffer.m_listSize > 0)
    {
      if (m_txTimerSourceEvent.IsRunning ())
	      m_txTimerSourceEvent.Cancel ();
//      ForwardStoredFramesToAssociatedAPs (ipv4L3);
      m_txTimerSourceEvent = Simulator::Schedule (m_txTimerSource, &MeDeHaDtnRouting::HandleSourceStoredFrames, this);	// reschedule the timer for transmission of buffered frames
    }
      
}

void
MeDeHaDtnRouting::ReceiveDisAssocNotif (std::string context, Mac48Address apAddress)
{
  Ptr<ArpL3Protocol> arp = this->GetObject<ArpL3Protocol> ();
  
//  Ipv4Address ipv4AddressSta = arp->SearchForIpv4Address (staAddress);
  Ipv4Address ipv4AddressAP = arp->SearchForIpv4Address (apAddress);
  m_associated = false;
  m_associatedWith = "0.0.0.0";
  
//  m_niTimerEvent.Cancel ();
  m_defaultRoute = NULL;

#ifdef DEBUG
  std::cout << "MeDeHaDtnRouting::Disassociated Event received: station mac = " << apAddress << ", station ip = " << ipv4AddressAP << "\n";
#endif
}


void 
MeDeHaDtnRouting::AddHostRouteTo (Ipv4Address dest, 
                                   Ipv4Address nextHop, 
                                   uint32_t interface)
{
  NS_LOG_FUNCTION_NOARGS ();
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateHostRouteTo (dest, nextHop, interface);
  m_hostRoutes.push_back (route);
}

void 
MeDeHaDtnRouting::AddHostRouteTo (Ipv4Address dest, 
                                   uint32_t interface)
{
  NS_LOG_FUNCTION_NOARGS ();
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateHostRouteTo (dest, interface);
  m_hostRoutes.push_back (route);
}

void 
MeDeHaDtnRouting::SetDefaultRoute (Ipv4Address nextHop, 
                                    uint32_t interface)
{
  NS_LOG_FUNCTION_NOARGS ();
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateDefaultRoute (nextHop, interface);
  delete m_defaultRoute;
  m_defaultRoute = route;
}

uint32_t 
MeDeHaDtnRouting::GetNRoutes (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  uint32_t n = 0;
  if (m_defaultRoute != 0)
    {
      n++;
    }
  n += m_hostRoutes.size ();
  return n;
}

Ipv4RoutingTableEntry
MeDeHaDtnRouting::GetDefaultRoute ()
{
  NS_LOG_FUNCTION_NOARGS ();
  if (m_defaultRoute != 0)
    {
      return *m_defaultRoute;
    }
  else
    {
      return Ipv4RoutingTableEntry ();
    }
}

Ipv4RoutingTableEntry 
MeDeHaDtnRouting::GetRoute (uint32_t index)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (index == 0 && m_defaultRoute != 0)
    {
      return *m_defaultRoute;
    }
  if (index > 0 && m_defaultRoute != 0)
    {
      index--;
    }
  if (index < m_hostRoutes.size ())
    {
      uint32_t tmp = 0;
      for (HostRoutesCI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          if (tmp  == index)
            {
              return *i;
            }
          tmp++;
        }
    }
  NS_ASSERT (false);
  // quiet compiler.
  return 0;
}

void 
MeDeHaDtnRouting::RemoveRoute (uint32_t index)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (index == 0 && m_defaultRoute != 0)
    {
      delete m_defaultRoute;
      m_defaultRoute = 0;
    }
  if (index > 0 && m_defaultRoute != 0)
    {
      index--;
    }
  if (index < m_hostRoutes.size ())
    {
      uint32_t tmp = 0;
      for (HostRoutesI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          if (tmp  == index)
            {
              delete *i;
              m_hostRoutes.erase (i);
              return;
            }
          tmp++;
        }
    }
  NS_ASSERT (false);
}

Ptr<Ipv4Route>
MeDeHaDtnRouting::LookupMeDeHaDtn (Ipv4Address dest)
{
  NS_LOG_FUNCTION_NOARGS ();

#ifdef DEBUG
  std::cout << "At DTN Node " << m_mainAddress << " MeDeHa route lookup is called\n";
#endif

  Ipv4Address outDest;
  bool status = GetIPAddressForDtnMode (dest, &outDest);
  
  Ptr<Ipv4Route> rtentry = 0;
  if (status)
    {
      for (HostRoutesCI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          NS_ASSERT ((*i)->IsHost ());
          if ((*i)->GetDest ().IsEqual (dest))
            {
              NS_LOG_LOGIC ("Found global host route" << *i);
              Ipv4RoutingTableEntry* route = (*i);
              rtentry = Create<Ipv4Route> ();
              uint32_t interfaceIdx = route->GetInterface ();
              rtentry->SetDestination (route->GetDest ());
              rtentry->SetSource (SourceAddressSelection (interfaceIdx, route->GetDest ()));
              rtentry->SetGateway (route->GetGateway ());
              rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));
#ifdef DEBUG
	      std::cout << Simulator::Now () << ", At DTN Node " << m_mainAddress << " MeDeHa DTN Route Lookup for " << dest << ", Gateway is " << route->GetGateway () << ", and Interface is " << route->GetInterface () << "\n";
#endif
	      NS_LOG_LOGIC ("At DTN Node " << m_mainAddress << " MeDeHa DTN Route Lookup for " << dest << ", Gateway is " << route->GetGateway () << ", and Interface is " << route->GetInterface ());
              return rtentry;
            }
        }
    }

  if (m_defaultRoute != 0) 
    {
      NS_ASSERT (m_defaultRoute->IsDefault ());
      NS_LOG_LOGIC ("Found global network route" << m_defaultRoute);
      Ipv4RoutingTableEntry* route = m_defaultRoute;
      rtentry = Create<Ipv4Route> ();
      uint32_t interfaceIdx = route->GetInterface ();
      rtentry->SetDestination (route->GetDest ());
      rtentry->SetSource (SourceAddressSelection (interfaceIdx, route->GetDest ()));
      rtentry->SetGateway (route->GetGateway ());
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));
#ifdef DEBUG
      std::cout << "MeDeHaDtnRouting::LookupMeDeHaDtn (), At " << m_mainAddress << ", Returning default route\n";
#endif
      return rtentry;
    }
  return 0;
}

Ipv4Address
MeDeHaDtnRouting::SourceAddressSelection (uint32_t interfaceIdx, Ipv4Address dest)
{
  if (m_ipv4->GetNAddresses (interfaceIdx) == 1)  // common case
    {
      return m_ipv4->GetAddress (interfaceIdx, 0).GetLocal ();
    }
  // no way to determine the scope of the destination, so adopt the
  // following rule:  pick the first available address (index 0) unless
  // a subsequent address is on link (in which case, pick the primary
  // address if there are multiple)
  Ipv4Address candidate = m_ipv4->GetAddress (interfaceIdx, 0).GetLocal ();
  for (uint32_t i = 0; i < m_ipv4->GetNAddresses (interfaceIdx); i++)
    {
      Ipv4InterfaceAddress test = m_ipv4->GetAddress (interfaceIdx, i);
      if (test.GetLocal ().CombineMask (test.GetMask ()) == dest.CombineMask (test.GetMask ()))
        {
          if (test.IsSecondary () == false) 
            {
              return test.GetLocal ();
            }
        }
    }
  return candidate;
}

bool
MeDeHaDtnRouting::RefreshEntryForDest (Ipv4Address destAddress, Ipv4Address nextHopAddr, Ipv4Address destAddressAlternate, bool iN)
{
  uint32_t interface = 0;
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      Ipv4Address addr = m_ipv4->GetAddress (i, 0).GetLocal ();
      if (addr == m_mainAddress)
        {
	  interface = i;
	  break;
	}
    }

  bool status = 0;

  if (iN)
    {
      status = RemovingRoutingEntry (destAddress);
      
#ifdef DEBUG
      std::cout << "MeDeHaDtnRouting::RefreshEntryForDest, Route is added for destination " << destAddress << "\n";
#endif

      AddHostRouteTo (destAddress, interface);  
    }
  else
    {
      bool result = SearchForDestEntryAsImmediateNeighbor (destAddress, destAddressAlternate);
      if (!result)
        {
          status = RemovingRoutingEntry (destAddress);
	  
#ifdef DEBUG
          std::cout << "MeDeHaDtnRouting::RefreshEntryForDest, Route is added for destination " << destAddress << "\n";
#endif

	  AddHostRouteTo (destAddress, nextHopAddr, interface);
	}
    }
  
  return status;
}
/*
void
MeDeHaDtnRouting::HandleNITimer (void)
{
//  std::cout << "MeDeHaDtnRouting::HandleNITimer, Timer is called\n";
  
  // Here, We are just sending only the routing table information one-by-one
  // TODO:: We need to send contact table information as well
  // If a station's information is found in the routing table, then we don't send its contact table information
  
  CheckForExpiredEntries ();
  if (m_associated)
    {
      Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();
      HandleNotificationsClass hNotif = ipv4L3->GetHandleNotificationClass ();
      RTbleI iterator;
      
      Ipv4Address addr;
      for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
        {
          addr = m_ipv4->GetAddress (i, 0).GetLocal ();
          Ipv4Address loopback ("127.0.0.1");
      
          if (addr != loopback &&
              addr != m_mainAddress)
            { 	      
              break;
            }
        }
      
      for (iterator = m_routingTable.begin (); iterator != m_routingTable.end (); iterator++)
        {
          Ptr<Packet> packetToSend = Create<Packet> ();
 	    
	  // header.GetIPAddress01 () signifies the ad hoc interface address of a station
	  // while sending information to an associated AP, we keep the infrastructure address "header.GetIPAddress02 ()" of the station first
          hNotif.SendNotificationToAPs (NEIGHBORHOOD_INFO_NOTIF, 2, addr, iterator->staAddress, iterator->staAddressAlternate, packetToSend);
          ipv4L3->Send (packetToSend, m_mainAddress, m_associatedWith, 17, NULL);
	}
    }
    
  m_niTimerEvent = Simulator::Schedule (m_niInterval, &MeDeHaDtnRouting::HandleNITimer, this);
}
*/
void
MeDeHaDtnRouting::UpdateContactTable (Ipv4Address address01, Ipv4Address address02, bool status)
{
  CTbleI iterator;
  
  for (iterator = m_contactTable.begin (); iterator != m_contactTable.end (); iterator++)
    {
      if ((iterator->staAddress == address01 &&
           iterator->staAddressAlternate == address02) ||
          (iterator->staAddress == address02 &&
           iterator->staAddressAlternate == address01))
	{
	  iterator->encounterTime = Simulator::Now ();
	  if (status == 0)
	    {
              iterator->numOfEncounters++;
	    }
	  return;
	}
    }

  struct ContactTable ct;
  ct.staAddress = address01;
  ct.staAddressAlternate = address02;
  ct.encounterTime = Simulator::Now ();
  ct.numOfEncounters = 1;
  
  m_contactTable.push_back (ct);
}

void
MeDeHaDtnRouting::UpdateRoutingTable (Ipv4Address address01, Ipv4Address address02, Ipv4Address nextHop, bool iN)
{
  RTbleI iterator;
  
  CheckForExpiredEntries ();
  for (iterator = m_routingTable.begin (); iterator != m_routingTable.end (); iterator++)
    {
      if ((iterator->staAddress == address01 &&
           iterator->staAddressAlternate == address02) ||
          (iterator->staAddress == address02 &&
           iterator->staAddressAlternate == address01))
	{
	  iterator->nextHop = nextHop;
	  iterator->entryTime = Simulator::Now ();
	  iterator->immediateNeighbor = iN;
	  return;
	}
    }

  struct RoutingTable rt;
  rt.staAddress = address01;
  rt.staAddressAlternate = address02;
  rt.nextHop = nextHop;
  rt.entryTime = Simulator::Now ();
  rt.immediateNeighbor = iN;
  
  m_routingTable.push_back (rt);
}

bool
MeDeHaDtnRouting::GetIPAddressForDtnMode (Ipv4Address inputAddress, Ipv4Address* outputAddress)
{
  RTbleI iterator;
  
  for (iterator = m_routingTable.begin (); iterator != m_routingTable.end (); iterator++)
    {
      if (iterator->staAddress == inputAddress)
        {
	  *outputAddress = inputAddress;
	  return true;
	}
      else if (iterator->staAddressAlternate == inputAddress)
        {
	  *outputAddress = iterator->staAddress;
	  return true;
	}
    }
  return false;
}

void
MeDeHaDtnRouting::CheckForExpiredEntries (void)
{
  Ipv4Address addr;
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      addr = m_ipv4->GetAddress (i, 0).GetLocal ();
      Ipv4Address loopback ("127.0.0.1");
      
      if (addr != loopback &&
          addr != m_mainAddress)
        {
          m_addressAlternate = addr; 	      
	  break;
	}
    }

  RTbleI iterator;
  Time currentTime = Simulator::Now ();
  
  for (iterator = m_routingTable.begin (); iterator != m_routingTable.end (); )
    {
      if ((currentTime - (iterator->entryTime)) > ENTRY_EXPIRE_TIME)
        {
#ifdef DEBUG
	  std::cout << "Calling from CheckForExpiredEntries () " << ENTRY_EXPIRE_TIME << " Entry Time = " << iterator->entryTime << " Current Time = " << currentTime << "\n";	  
#endif
	  RemovingRoutingEntry (iterator->staAddress);
	  RemovingRoutingEntry (iterator->staAddressAlternate);
	  if (m_associated)
	    {
	      Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();
              HandleNotificationsClass hNotif = ipv4L3->GetHandleNotificationClass ();
              Ptr<Packet> packetToSend = Create<Packet> ();
	      std::cout << "CheckForExpiredEntries (), sender: " << addr << " first: " << iterator->staAddress << " second: " << iterator->staAddressAlternate << "\n";
	      hNotif.SendNotificationToAPs (NEIGHBORHOOD_DISASS_NOTIF, 2, addr, iterator->staAddress, iterator->staAddressAlternate, packetToSend);
              ipv4L3->Send (packetToSend, m_mainAddress, m_associatedWith, 17, NULL);	  
	    }
          iterator = m_routingTable.erase (iterator);
	  continue;
	}
      else
        {
	  iterator++;
	}
    }
}

bool
MeDeHaDtnRouting::RemovingRoutingEntry (Ipv4Address destAddress)
{
  bool status = 0;
  for (uint32_t i = 0; i < GetNRoutes (); i++)
    {
      Ipv4RoutingTableEntry route = GetRoute (i);
      if (route.GetDest () == destAddress)
        {
#ifdef DEBUG
	  std::cout << "MeDeHaDtnRouting::RemovingRoutingEntry, Removing Route for " << destAddress << ", Gateway is " << route.GetGateway () << ", and Interface is " << route.GetInterface () << "\n";
#endif
          RemoveRoute (i);
	  status = 1;
          break;
        }
    }
  return status;
}

bool
MeDeHaDtnRouting::SearchForDestEntryAsImmediateNeighbor (Ipv4Address destAddress, Ipv4Address destAddressAlternate)
{
  for (uint32_t i = 0; i < GetNRoutes (); i++)
    {
      Ipv4RoutingTableEntry route = GetRoute (i);
      if (route.GetDest () == destAddress)
        {
	  if (route.GetGateway () == Ipv4Address ("0.0.0.0") ||
	      route.GetGateway () == destAddressAlternate)
	    {
	      return 1;
	    }
        }
    }
  return 0;
}

bool
MeDeHaDtnRouting::SearchForDestEntryInRoutingTable (Ipv4Address destAddress)
{
  RTbleI iterator;
   
  if (!m_routingTable.empty ())
  {
  for (iterator = m_routingTable.begin (); iterator != m_routingTable.end (); iterator++)
    {
      if (iterator->staAddress == destAddress ||
          iterator->staAddressAlternate == destAddress ||
	  iterator->nextHop == destAddress)
        {
	  return true;
	}
    }
  }
  return false;
}

void
MeDeHaDtnRouting::MeDeHaBufferPacketLocally (Ptr<const Packet> packet, Ipv4Header ipHeader, UdpHeader* udpHeader)
{
#ifdef BUFFER
  UdpHeader udpHdr;
  Packet pkt = *packet;

  if (udpHeader != 0)
    {
      udpHdr = *udpHeader;
    }
  else
    {
      pkt.RemoveHeader (udpHdr);
    }
  SeqNumHeader seqHdr;
  pkt.RemoveHeader (seqHdr);
  int seqNum = seqHdr.GetSeqNum ();
  int priority = seqHdr.GetPriority ();
  int copies = seqHdr.GetNumOfCopies ();
  bool copyGiven = seqHdr.GetCopyGiven ();
//  seqHdr.SetNumOfCopies (copies-1);
//  std::cout << "MeDeHaDtnRouting::MeDeHaBufferPacketLocally, Packet with Seq Num " << seqNum << " buffered\n";
  pkt.AddHeader (seqHdr);
  pkt.AddHeader (udpHdr);
  
  BufferedFramesSt frameForBuffer;
  frameForBuffer.packet = ConstCast<Packet> (packet);
  frameForBuffer.src = ipHeader.GetSource ();
  frameForBuffer.dst = ipHeader.GetDestination ();
  frameForBuffer.seqNum = seqNum;
  frameForBuffer.protocol = ipHeader.GetProtocol ();
  frameForBuffer.priority = priority;
  frameForBuffer.copies = copies;
  frameForBuffer.bbCopyGiven = copyGiven;
  frameForBuffer.ttl = 20;
  
  bool status = false;
  status = RemoveDuplicateCopiesFromBuffer (frameForBuffer);
  
  if (!status)
    {
      buffer.AddElementToList(frameForBuffer);
      std::cout << "MeDeHaDtnRouting::MeDeHaBufferPacketLocally, At " << m_mainAddress << ", buffering packet no. " << seqNum << " for " << ipHeader.GetDestination () << "\n";
    }
#endif
}

bool
MeDeHaDtnRouting::RemoveDuplicateCopiesFromBuffer (BufferedFramesSt frame)
{
  std::list<BufferedFramesSt>::iterator newIterator;		// link list iterator
  for (newIterator = buffer.m_pktList.begin(); newIterator != buffer.m_pktList.end(); )
    {
      if (newIterator->seqNum == frame.seqNum &&
          newIterator->src == frame.src &&
          newIterator->dst == frame.dst)
        {
          return true;
//          newIterator = buffer.m_pktList.erase (newIterator);
        }
      else
        {
          newIterator++;
        }
    }
  return false;
}


void
MeDeHaDtnRouting::SendStoredFrames (Ptr<Ipv4L3Protocol> ipv4L3)
{
  RTbleI iterator;  
  
  Ptr<Packet> packet;
  int counter = 0;
  int i = 0;
  Ipv4Address destAddress, alternateAddress;
  
  std::cout << "At " << m_mainAddress << ", SendStoredFrames is Called\n";

  bool flag = 0;

  for (iterator = m_routingTable.begin (), i=0; ;)
    {
      destAddress = iterator->staAddress;      
      alternateAddress = iterator->staAddressAlternate;
//    std::cout << "SendStoreFramesToDest(): dest address = " << destAddress << " my address = " << myAddress << endl;		

      int count01 = CountNumberOfStoredFrames (destAddress);
      int count02 = CountNumberOfStoredFrames (alternateAddress);
      
//    std::cout << "SendStoreFramesToDest(): Count for dest " << destAddress << " is " << count << endl;

      if (count01 > 0)
        {
          std::list<BufferedFramesSt>::iterator newIterator;		// link list iterator
          for (newIterator = buffer.m_pktList.begin(); newIterator != buffer.m_pktList.end(); )
            {
              packet = (newIterator->packet);

              if (destAddress == newIterator->dst)
                {
                  flag = 1;
#ifdef DEBUG
		  std::cout << "SendStoredFrames (): Sending message to " << destAddress << "\n";
#endif
		  ipv4L3->Send (packet, newIterator->src, newIterator->dst, newIterator->protocol, NULL);
		  newIterator = buffer.m_pktList.erase(newIterator);
                  (buffer.m_listSize)--;
		  buffer.SetMemoryState ();
//                  i++;
                  break;
                }
              else
                {
                  counter++;
                  newIterator++;
                }                   
            }
        }
	
      if (count02 > 0)
        {
          std::list<BufferedFramesSt>::iterator newIterator;		// link list iterator
          for (newIterator = buffer.m_pktList.begin(); newIterator != buffer.m_pktList.end(); )
            {
              packet = (newIterator->packet);

              if (alternateAddress == newIterator->dst)
                {
                  flag = 1;
#ifdef DEBUG
		  std::cout << "SendStoredFrames (): Sending message to " << alternateAddress << "\n";
#endif
		  ipv4L3->Send (packet, newIterator->src, newIterator->dst, newIterator->protocol, NULL);
		  newIterator = buffer.m_pktList.erase(newIterator);
                  (buffer.m_listSize)--;
		  buffer.SetMemoryState ();
//                  i++;
                  break;
                }
              else
                {
                  counter++;
                  newIterator++;
                }
            }
        }

      if (flag)
        {
	  i++;
	}
	
      iterator++;		
      if (i == (MEDEHA_MAX_NUM_FRAMES_TO_SEND) || buffer.m_pktList.size() == 0)
        {				
          break;
        }
      if (iterator == m_routingTable.end())
        {
          if (flag)
            {
              iterator = m_routingTable.begin();
              flag = 0;
            }
          else
            {
              break;
            }
        }
    }    
}

int
MeDeHaDtnRouting::CountNumberOfStoredFrames (Ipv4Address destAddress)
{
  int counter = 0;
  
  std::list<BufferedFramesSt>::iterator newIterator;

  for (newIterator = buffer.m_pktList.begin(); newIterator != buffer.m_pktList.end(); newIterator++)
    {
      if (destAddress == newIterator->dst)
        {
          counter++;
        }      
    }
  return counter;
}

void
MeDeHaDtnRouting::HandleSendTimer (void)
{
  Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();
  if (buffer.m_listSize > 0)
    {
      NS_LOG_INFO("Timer Expires, Sending stored frames");
      SendStoredFrames (ipv4L3);
    }
  if (buffer.m_listSize > 0)
    {
      m_txTimerEvent = Simulator::Schedule (m_txTimer, &MeDeHaDtnRouting::HandleSendTimer, this);	// reschedule the timer for transmission of buffered frames
    }
  else
    {
      m_txTimerEvent.Cancel ();
    }
}

MeDeHaDtnRouting::ContactTable*
MeDeHaDtnRouting::SearchForDestEntryInContactTable (Ipv4Address destAddress)
{
  CTbleI iterator;
  
  for (iterator = m_contactTable.begin (); iterator != m_contactTable.end (); iterator++)
    {
      if (iterator->staAddress == destAddress ||
          iterator->staAddressAlternate == destAddress)	  
        {
	  return &(*iterator);
	}
    }
  return NULL;
}


void
MeDeHaDtnRouting::HandleMultiCopyTimer (void)
{
  Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();

  bool status = 0;
  if (buffer.m_listSize > 0)
    {
      NS_LOG_INFO("MeDeHaDtnRouting::HandleMultiCopyTimer, Timer Expires, Sending stored frames");
      status = CompareSeqNumAndSendPacket (ipv4L3, m_destMultiCopy01, m_destMultiCopy02);
    }
    
  if (buffer.m_listSize > 0 && status == 1)
    {
      m_txTimerMultiCopyEvent = Simulator::Schedule (m_txTimerMultiCopy, &MeDeHaDtnRouting::HandleMultiCopyTimer, this);	// reschedule the timer for transmission of buffered frames
    }
    
  else
    {
      m_txTimerMultiCopyEvent.Cancel ();
    }
}


bool
MeDeHaDtnRouting::CompareSeqNumAndSendPacket (Ptr<Ipv4L3Protocol> ipv4L3, Ipv4Address destAddress01, Ipv4Address destAddress02)
{
  uint16_t i=0;
  Ptr<Packet> packet;  
  std::list<BufferedFramesSt>::iterator newIterator;		// link list iterator
  
  for (newIterator = buffer.m_pktList.begin(); newIterator != buffer.m_pktList.end(); )
    {
      packet = (newIterator->packet);

      if (newIterator->dst == destAddress01 ||
          newIterator->dst == destAddress02)
        {
#ifdef DEBUG
          std::cout << "CompareSeqNumAndSendPacket (): Sending message to " << destAddress01 << "\n";
#endif
          bool status = CheckCommonStoredPacket (newIterator->src, newIterator->dst, newIterator->seqNum);
          if (!status)
            {
              Ptr<Packet> copy = packet->Copy ();
	  
              UdpHeader udp;
              SeqNumHeader seqHdr;
              packet->RemoveHeader (udp);
              packet->RemoveHeader (seqHdr);
              int copies = seqHdr.GetNumOfCopies ();
              seqHdr.SetNumOfCopies (1);
              packet->AddHeader (seqHdr);
              packet->AddHeader (udp);
	      
              ipv4L3->Send (packet, newIterator->src, newIterator->dst, newIterator->protocol, NULL);
              copies--;
	      
              if (copies > 0)
                {
                  copy->RemoveHeader (udp);
                  copy->RemoveHeader (seqHdr);
                  		  
                  seqHdr.SetNumOfCopies (copies);
                  std::cout << "CompareSeqNumAndSendPacket (), Remaining Copies for msg " << newIterator->seqNum << " = " << copies << std::endl;
                  copy->AddHeader (seqHdr);
                  copy->AddHeader (udp);
		  
                  newIterator->packet = copy;
               	  newIterator++;                  
              	}
              else
                {
                  newIterator = buffer.m_pktList.erase(newIterator);
                  (buffer.m_listSize)--;
                  buffer.SetMemoryState ();
              	}
              i++;
              if (i == 20)
                  break;
            }
          else
            {
              newIterator++;
            }                   
        }
      else
        {
          newIterator++;
        }
    }
    
  if (i == 0)
    {
    	m_msgVectorSummeryList.clear ();
      return 0;
    }
    
  return 1;
}

bool
MeDeHaDtnRouting::CheckCommonStoredPacket (Ipv4Address src, Ipv4Address dst, uint16_t seqNum)
{
  VSummeryI iter;
  
  for (iter=m_msgVectorSummeryList.begin (); iter!=m_msgVectorSummeryList.end (); iter++)
    {      
      if (iter->srcAddress == src &&
          iter->dstAddress == dst &&
          iter->seqNumber == seqNum)
        {
	  std::cout << "src: " << iter->srcAddress << ", " << src << " dst: " << iter->dstAddress << ", " << dst << " seqNum: " << iter->seqNumber << ", " << seqNum << "\n";
          return 1;
        }
    }
  return 0;
}

void
MeDeHaDtnRouting::HandleSourceStoredFrames (void)
{
  Ptr<Ipv4L3Protocol> ipv4L3 = m_node->GetObject <Ipv4L3Protocol> ();

  bool status = 0;
  if (buffer.m_listSize > 0)
    {
      NS_LOG_INFO("MeDeHaDtnRouting::HandleSourceStoredFrames, Timer Expires, Sending stored frames");
      status = ForwardStoredFramesToAssociatedAPs (ipv4L3);
    }
  if (buffer.m_listSize > 0 && status == 1)
    {
      m_txTimerSourceEvent = Simulator::Schedule (m_txTimerSource, &MeDeHaDtnRouting::HandleSourceStoredFrames, this);	// reschedule the timer for transmission of buffered frames
    }
  else
    {
      m_txTimerSourceEvent.Cancel ();
    }
}

bool
MeDeHaDtnRouting::ForwardStoredFramesToAssociatedAPs (Ptr<Ipv4L3Protocol> ipv4L3)
{
  Ptr<Packet> packet;  
  std::list<BufferedFramesSt>::iterator newIterator;		// link list iterator

#ifdef DEBUG
  std::cout << "MeDeHaDtnRouting::ForwardStoredFramesToAssociatedAPs, Forwarding frames to AP\n";
#endif

  int i=0;
  for (newIterator = buffer.m_pktList.begin(); newIterator != buffer.m_pktList.end(); )
    {
      packet = (newIterator->packet);

      if (!(newIterator->bbCopyGiven))
        {
#ifdef DEBUG
          std::cout << "MeDeHaDtnRouting::ForwardStoredFramesToAssociatedAPs, FLAG for dst " << newIterator->dst << " is " << newIterator->bbCopyGiven << std::endl;
#endif
#ifdef SOCIAL_AFFILIATION
	  bool flag = CompareSocialAffiliationWithAP (newIterator->dst, m_associatedWith);
	  if (flag)
	    {
#endif
	  Ptr<Packet> copy = packet->Copy ();
	  
          UdpHeader udp;
          SeqNumHeader seqHdr;
          packet->RemoveHeader (udp);
          packet->RemoveHeader (seqHdr);
          int copies = seqHdr.GetNumOfCopies ();
          seqHdr.SetNumOfCopies (1);
          packet->AddHeader (seqHdr);
          packet->AddHeader (udp);
          ipv4L3->Send (packet, newIterator->src, newIterator->dst, newIterator->protocol, NULL);
          
          copies--;
	  
          if (copies > 0)
            {
              copy->RemoveHeader (udp);
              copy->RemoveHeader (seqHdr);
              seqHdr.SetCopyGiven (true);
              seqHdr.SetNumOfCopies (copies);
              std::cout << "MeDeHaDtnRouting::ForwardStoredFramesToAssociatedAPs (), Remaining Copies for msg " << newIterator->seqNum << " = " << copies << std::endl;
              copy->AddHeader (seqHdr);
              copy->AddHeader (udp);

              newIterator->packet = copy;
              newIterator->bbCopyGiven = true;
              newIterator++;
	          }
          else
            {
              newIterator = buffer.m_pktList.erase (newIterator);
              (buffer.m_listSize)--;
              buffer.SetMemoryState ();
            }
	  
          i++;
          if (i == 10)
              break;
#ifdef SOCIAL_AFFILIATION
	    }
	  else
	    {
	      newIterator++;
	    }
#endif
        }
      else
        {
          newIterator++;
        }
    }
    
  if (i == 0)
    {
      return 0;
    }
    
  return 1;
}

void
MeDeHaDtnRouting::RemoveRouteWithGWAsDest (Ipv4Address gatewayAddress)
{
  Ipv4Address destAddress;
  
  for (uint32_t i = 0; i < GetNRoutes (); i++)
    {
      Ipv4RoutingTableEntry route = GetRoute (i);
      if (route.GetGateway () == gatewayAddress)
        {
          destAddress = route.GetDest ();
#ifdef DEBUG
          std::cout << "Ipv4L3Protocol::RemoveRouteWithGWAsDest, Removing Route for " << route.GetDest () << ", Gateway is " << route.GetGateway () << ", and Interface is " << route.GetInterface () << "\n";
#endif
          RemoveRoute (i);
          RemoveFromRoutingTable (destAddress);
	  
          continue;
        }
    }
}

bool
MeDeHaDtnRouting::RemoveFromRoutingTable (Ipv4Address destAddress)
{
  RTbleI iterator;
   
  if (!m_routingTable.empty ())
    {
      for (iterator = m_routingTable.begin (); iterator != m_routingTable.end ();)
        {
          if (iterator->staAddress == destAddress ||
              iterator->staAddressAlternate == destAddress ||
              iterator->nextHop == destAddress)
            {
              iterator = m_routingTable.erase (iterator);
              return true;
            }
          else
            {
              iterator++;
            }
        }
    }    
  return false;
}


bool
MeDeHaDtnRouting::CompareSocialAffiliationWithAP (Ipv4Address dstAddress, 
					    Ipv4Address apAddress)
{
  uint32_t dst = dstAddress.Get () & 0xff;
  uint32_t relay = apAddress.Get () & 0xff;
  uint32_t compare = dstAddress.Get() >> 24;
  
  std::cout << "CompareSocialAffiliation (), " << dst << ", " << relay << ", " << compare << std::endl;
  
  if (compare == 0x0a)
    {
      dst -= 9;
    }

  std::cout << "CompareSocialAffiliation (), " << dstAddress << ", " << apAddress << std::endl;
  
  if (dst >= 1 && dst <= 14)
    {
      if (relay >= 1 && relay <= 3)
        {
          return true;
        }
    }
  else if (dst >= 15 && dst <= 28)
    {
      if (relay >= 4 && relay <= 6)
        {
          return true;
        }
    }
  else if (dst >= 29 && dst <= 42)
    {
      if (relay >= 7 && relay <= 9)
        {
          return true;
        }
    }
    
  return false;
}


bool
MeDeHaDtnRouting::CompareSocialAffiliation (Ipv4Address dstAddress, 
					    Ipv4Address relayAddress)
{
  uint32_t dst = dstAddress.Get () & 0xff;
  uint32_t relay = relayAddress.Get () & 0xff;
  uint32_t compare = dstAddress.Get() >> 24;
  
  std::cout << "CompareSocialAffiliation (), " << dst << ", " << relay << ", " << compare << std::endl;
  
  if (compare == 0x0a)
    {
      dst -= 9;
    }

  std::cout << "CompareSocialAffiliation (), " << dstAddress << ", " << relayAddress << std::endl;
  
  if (dst >= 1 && dst <= 14)
    {
      if (relay >= 43 && relay <= 48)
        {
          return true;
        }
    }
  else if (dst >= 15 && dst <= 28)
    {
      if (relay >= 49 && relay <= 54)
        {
          return true;
        }
    }
  else if (dst >= 29 && dst <= 42)
    {
      if (relay >= 55 && relay <= 60)
        {
          return true;
        }
    }
    
  return false;
}


void
MeDeHaDtnRouting::ForwardStoredFramesToSocialNode (Ptr<Ipv4L3Protocol> ipv4L3, Ipv4Address relayAddress)
{
  Ptr<Packet> packet;  
  std::list<BufferedFramesSt>::iterator newIterator;		// link list iterator
  
  for (newIterator = buffer.m_pktList.begin(); newIterator != buffer.m_pktList.end(); )
    {
      packet = (newIterator->packet);
      bool status = CompareSocialAffiliation (newIterator->dst, relayAddress);
      
      if (status)
        {
	  bool flag = CheckCommonStoredPacket (newIterator->src, newIterator->dst, newIterator->seqNum);
	  
	  if (!flag)
	    {
              std::cout << "Forwarding frame to social node " << relayAddress << "\n";
              UpdateRoutingTable (newIterator->dst, newIterator->dst, relayAddress, 0);
              RefreshEntryForDest (newIterator->dst, relayAddress, newIterator->dst, 0);
	  
	      Ptr<Packet> copy = packet->Copy ();
	  
              UdpHeader udp;
              SeqNumHeader seqHdr;
              packet->RemoveHeader (udp);
              packet->RemoveHeader (seqHdr);
              int copies = seqHdr.GetNumOfCopies ();
              seqHdr.SetNumOfCopies (1);
              packet->AddHeader (seqHdr);
              packet->AddHeader (udp);
	      
              ipv4L3->Send (packet, newIterator->src, newIterator->dst, newIterator->protocol, NULL);
              copies--;
	    
              if (copies > 0)
                {
                  copy->RemoveHeader (udp);
                  copy->RemoveHeader (seqHdr);
                  		  
                  seqHdr.SetNumOfCopies (copies);
                  std::cout << "ForwardStoredFramesToSocialNode (), Remaining Copies for msg " << newIterator->seqNum << " = " << copies << std::endl;
                  copy->AddHeader (seqHdr);
                  copy->AddHeader (udp);
		  
                  newIterator->packet = copy;
                  newIterator++;                  
                }
              else
                {
                  newIterator = buffer.m_pktList.erase(newIterator);
                  (buffer.m_listSize)--;
                  buffer.SetMemoryState ();
                }
            }
	  else
	    {
	      newIterator++;	
	    }
	}
      else
        {
	  newIterator++;
	}
    }
}

}
