
#include "mobitrade_protocol.h"
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
#include "ns3/arp-l3-protocol.h"
#include "ns3/config.h"
#include "ns3/udp-header.h"
#include <limits.h>
#include <sstream>
#include <algorithm>

using namespace std;
namespace ns3{

// Initial utility for a foreign Interest in case of no Content Match
// We suppose that the generosity parameter BETA is equal to 1 Mo
unsigned int BETA = 20*CONTENT_PACKET_SIZE;
const unsigned int ALPHA = CONTENT_PACKET_SIZE;

// Used if we consider channels with different sizes
std::map<int, int> sizeChannels = std::map<int, int>();
std::map<string, int> sizeChannels2 = std::map<string, int>();

NS_LOG_COMPONENT_DEFINE ("ns3::MOBITRADEProtocol");
NS_OBJECT_ENSURE_REGISTERED (MOBITRADEProtocol);


// ------------------------------------------------------------------------------------------------------------------------------------------------------
// 													MOBITRADEProtocol Implementation
// ------------------------------------------------------------------------------------------------------------------------------------------------------


MOBITRADEProtocol::MOBITRADEProtocol():helloTimer (Timer::CANCEL_ON_DESTROY), sessionsTimeOutTimer(Timer::CANCEL_ON_DESTROY),logTimer(Timer::CANCEL_ON_DESTROY),contentsGenerationTimer(Timer::CANCEL_ON_DESTROY), interestsGenerationTimer(Timer::CANCEL_ON_DESTROY)
{
  numberOfGeneratedInterests = 0;
  m_defaultRoute = NULL;
  helloInterval = Seconds(HELLO_INTERVAL);
  sessionsTimeOut = Seconds(SESSIONS_TIMER_INTERVAL);
  nodeId = 0;

  sem_init(&contentsSem, 0, 1);
  sem_init(&interestsSem, 0, 1);
  sem_init(&sessionsSem, 0, 1);

  numberRequestedContents = 0;
  numberGeneratedContents = 0;
  contentsIndex = 0;
  interestsIndex = 0;
  lastMeeting = 0;
  weight = 0;
  // By default, each node is not malicious and is active
  maliciousNode = false;
  activeNode = true;
}

MOBITRADEProtocol::~MOBITRADEProtocol()
{
	sem_destroy(&contentsSem);
	sem_destroy(&interestsSem);
	sem_destroy(&sessionsSem);

	if (helloTimer.IsRunning ())
    {
      helloTimer.Cancel ();
    }

	if(sessionsTimeOutTimer.IsRunning())
    {
      sessionsTimeOutTimer.Cancel();
    }

	if(contentsGenerationTimer.IsRunning())
	{
		contentsGenerationTimer.Cancel();
	}

	if(interestsGenerationTimer.IsRunning())
	{
		interestsGenerationTimer.Cancel();
	}

	if(m_defaultRoute != NULL)
		delete m_defaultRoute;

  // Cleaning the map of Interests
  while(!mapInterests.empty())
  {
	  map<string, Interest*>::iterator iter = mapInterests.begin();
	  delete iter->second;
	  mapInterests.erase(iter);
  }

  // Cleaning the map of Contents
  while(!mapContents.empty())
  {
	  map<string, Content*>::iterator iter = mapContents.begin();
	  delete iter->second;
	  mapContents.erase(iter);
  }

  // Cleaning the map of Contents
  while(!generatedContents.empty())
  {
	  map<string, Content*>::iterator iter = generatedContents.begin();
	  delete iter->second;
	  generatedContents.erase(iter);
  }

  exchangedBytes.clear();
  currentActiveSessions.clear();
  nodesLastMeetingTimeMap.clear();
  deliveredContents.clear();
  nodesCollaboration.clear();
}

void MOBITRADEProtocol::SendHello()
{
  Ptr<Packet> packet = Create<Packet> ((uint8_t*)"Hello", sizeof("Hello"));

  // Hello msg Header
  MOBITRADEMsgHeader header;
  header.SetMsgType(HELLO_MSG);
  header.SetMsgTTL(0);
  header.SetDestAddress(Ipv4Address (BROADCAST_ADDR));

  packet->AddHeader (header);

  SendPacket (packet, 0);
  
  
}

void MOBITRADEProtocol::SendEndSession(Ipv4Address destDevice)
{
  Ptr<Packet> packet = Create<Packet> ((uint8_t*)"ENDSESSION", sizeof("Hello"));

  // Hello msg Header
  MOBITRADEMsgHeader header;
  header.SetMsgType(END_SESSION_MSG);
  header.SetMsgTTL(0);
  header.SetDestAddress(Ipv4Address (destDevice));

  packet->AddHeader (header);

  SendPacket (packet, 0);
  ChangeSessionState(destDevice, END_SESSION_REQUESTED);

}

void MOBITRADEProtocol::SendListOfInterests(Ipv4Address  destDevice)
{
	CleanOutOfDateInterests();
	int numberOfInterests = 0;

	string tmpInterests = GetListOfInterests(&numberOfInterests);
#ifdef DEBUG
	NS_ASSERT(numberOfInterests <= INTERESTS_STORAGE_CAPACITY);
#endif
	Ptr<Packet> packet = Create<Packet> ((uint8_t*)tmpInterests.c_str(), tmpInterests.length());

	// Interests msg Header
	MOBITRADEMsgHeader header;
	header.SetMsgType(INTEREST_MSG);
	header.SetMsgTTL(0);
	header.SetDestAddress(destDevice);

	packet->AddHeader (header);

	stringstream data2;
	packet->CopyData(&data2, MAX_INTERESTS_SIZE);

	SendPacket (packet, 0);

}

void MOBITRADEProtocol::SendContent(Ipv4Address  destDevice, Content *content)
{
	if(!content->UpdateTTL())
	{

		string tmpContent = content->GetResume();
		Ptr<Packet> packet = Create<Packet> ((uint8_t*)tmpContent.c_str(), tmpContent.size());

		// Interests msg Header
		MOBITRADEMsgHeader header;
		header.SetMsgType(CONTENT_MSG);
		header.SetMsgTTL(0);
		header.SetDestAddress(destDevice);

		packet->AddHeader (header);

		SendPacket (packet, 0);
	}

}


void MOBITRADEProtocol::SendPacket (Ptr<Packet> packet, int msgType)
{
  if(msgType == 0)
  {
    if(helloSendingSocket->Send (packet) == -1)
     {
       NS_FATAL_ERROR ("Unable to send packet");
     }
  }
}

void MOBITRADEProtocol::RecvPacket (Ptr<Socket> socket)
{
  Ptr<Packet> receivedPacket;
  Address sourceAddress;
  receivedPacket = socket->RecvFrom (sourceAddress);

  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
  Ipv4Address senderIfaceAddr = inetSourceAddr.GetIpv4 ();

  MOBITRADEMsgHeader header;
  receivedPacket->RemoveHeader(header);

  uint32_t msgType = header.GetMessageType();
  // We accept either a broadcast hello message or an (Interest/Content) message
  if (header.GetDestAddress ().IsEqual (Ipv4Address (BROADCAST_ADDR)) ||header.GetDestAddress ().IsEqual (mainAddress))
    {
      switch (msgType)
       {
         case HELLO_MSG:
           ProcessHelloMsg (header, senderIfaceAddr);
           break;
         case INTEREST_MSG:
           ProcessInterestMsg (receivedPacket, senderIfaceAddr);
           break;
         case CONTENT_MSG:
           ProcessContentMsg (receivedPacket, senderIfaceAddr);
           break;
        case END_SESSION_MSG:
           ProcessEndSessionMsg(receivedPacket, senderIfaceAddr);
           break;
         default:
        	 cerr<<"Unknown message type received: "<<header.GetMessageType()<<endl;
           break;		
      }
   }
}

Ptr<Ipv4Route> MOBITRADEProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{

  Ptr<Ipv4Route> rtentry = Create<Ipv4Route>();
  rtentry->SetDestination(header.GetDestination());
  rtentry->SetGateway(m_defaultRoute->GetGateway());
  rtentry->SetOutputDevice(m_ipv4->GetNetDevice(m_defaultRoute->GetInterface()));
  rtentry->SetSource(mainAddress);

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

bool MOBITRADEProtocol::RouteInput  (Ptr<const Packet> p, const Ipv4Header &ipHeader, Ptr<const NetDevice> idev,UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
#ifdef DEBUG
  NS_ASSERT (m_ipv4 != 0);
  // Check if input device supports IP 
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
#endif
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev); 

  if (ipHeader.GetDestination ().IsBroadcast () || ipHeader.GetDestination ().IsEqual (Ipv4Address (BROADCAST_ADDR)))
    {
      Ptr<Packet> packetCopy = p->Copy();
      lcb (packetCopy, ipHeader, iif);
      return true;
    }

  cout<<"Received an Unknown packet, don't know what to do with it."<<endl;
  cout<<this << p <<" header: "<< ipHeader<<" source: " << ipHeader.GetSource ()<<" destination: " << ipHeader.GetDestination () <<" dev:  "<< idev<<endl;
  return false;
}

void MOBITRADEProtocol::NotifyInterfaceUp (uint32_t interface)
{
}

void MOBITRADEProtocol::NotifyInterfaceDown (uint32_t interface)
{
}

void MOBITRADEProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}

void MOBITRADEProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{

}

void MOBITRADEProtocol::Start()
{
  // Initializing the local main address
  if (mainAddress == Ipv4Address ())
  {
    Ipv4Address loopback ("127.0.0.1");
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      // Use primary address, if multiple
      Ipv4Address addr = m_ipv4->GetAddress (i, 0).GetLocal ();
      if (addr != loopback)
      {
        mainAddress = addr;
        break;
      }
    }
#ifdef DEBUG
    NS_ASSERT (mainAddress != Ipv4Address ());
#endif
  }

  SetDefaultRoute (mainAddress, m_ipv4->GetInterfaceForAddress(mainAddress));


  // Create a socket to listen on the local interface for incomming messages
  listeningSocket = Socket::CreateSocket (GetObject<Node> (), UdpSocketFactory::GetTypeId());
  listeningSocket->SetAllowBroadcast(true);
  listeningSocket->SetRecvCallback (MakeCallback (&MOBITRADEProtocol::RecvPacket, this));

  if (listeningSocket->Bind (InetSocketAddress (mainAddress, MOBITRADE_ROUTING_PORT)))
   {
     NS_FATAL_ERROR ("Failed to bind() MOBITRADE Protocl receive socket");
   }

  cout<<"Listing at: "<<mainAddress<<":"<<MOBITRADE_ROUTING_PORT<<endl;


  // Creating the scoket that will be used for sending hello messages

  helloSendingSocket = Socket::CreateSocket (GetObject<Node> (), UdpSocketFactory::GetTypeId());
  helloSendingSocket->SetAllowBroadcast(true);
  if (helloSendingSocket->Connect (InetSocketAddress (Ipv4Address (BROADCAST_ADDR), MOBITRADE_ROUTING_PORT)))
   {
     NS_FATAL_ERROR ("Failed to connect MOBITRADE Protocol broadcast socket");
   }

  // Scheduling the Hello timer 
  helloTimer.Schedule (Seconds(UniformVariable().GetInteger(0, HELLO_INTERVAL)));

  stringstream ss;
  ss << mainAddress;
  string adr = ss.str();

  // Scheduling the sessions cleaning Timer
  sessionsTimeOutTimer.Schedule(Seconds(UniformVariable().GetInteger(0, SESSIONS_TIMER_INTERVAL)) + Seconds(SESSIONS_TIMER_INTERVAL));
  cout<<"MOBITRADE protocol started on node " << adr <<endl;

  size_t pos = adr.find_last_of('.');
  nodeId = atoi(adr.substr(pos + 1).c_str());

  // Scheduling the LogTimer
  logTimer.Schedule(Seconds(LOG_INTERVAL));
  // Scheduling the contents generation timer
  contentsGenerationTimer.Schedule(Seconds(0));
  // Scheduling the interests generation timer
  interestsGenerationTimer.Schedule(Seconds(0));

  if(ACTIVATE_MALICIOUS_NODES)
  {
	  // Selecting the malicious nodes, We set 3 nodes per group to be malicious
	  if(nodeId <= 10)
	  {
		  	SetMalicious(true);
	  }
  }

  if(ENABLE_THAT_SOME_NODES_COME_LATER)
  {
	  // Set some nodes to be inactive at the beginning of the simulation
	  if(nodeId <= 10)
	  {
		  SetActive(false);
	  }
  }

#ifdef ENABLE_RANDOM_CHANNELS_SIZE
  if(nodeId == 1)
  {
	  GenerateChannelsWithRandomSizes(NUMBER_OF_NODES);

  }
#endif

}

void MOBITRADEProtocol::GenerateLocalContents(int start, int end)
{

#ifdef ENABLE_RANDOM_CHANNELS_SIZE
	if(contentsIndex < GetChannelSize(nodeId))
	{
#endif

	if(!IsMalicious())
	{
		GenerateLocalContent(nodeId, contentsIndex, true);
	}

	cout<<"Node: "<<nodeId<<" generating content: "<<nodeId<<" details: "<<contentsIndex<<endl;

#ifdef ENABLE_RANDOM_CHANNELS_SIZE
	}
#endif

}


void MOBITRADEProtocol::GenerateLocalInterests(int start, int end)
{

#ifdef ENABLE_RANDOM_NUMBER_OF_INTERESTS
	int nbrI = floor(UniformVariable(7, NUMBER_SLOTS).GetValue());
	cout<<"Node: "<<nodeId<<" generating Interests:"<<endl;
	cout<<"nbrI: "<<nbrI<<endl;
	for(int i = 0; i < nbrI; i++)
#else

	for(int i = 0; i < NUMBER_SLOTS; i++)
#endif
	{
#ifdef ENABLE_RANDOM_NUMBER_OF_INTERESTS
		int tmp = static_cast<int>(ceil(UniformVariable(start + (i*(end - start + 1))/nbrI, start + ((i + 1)*(end - start + 1))/nbrI).GetValue()));
#else
		int tmp = static_cast<int>(ceil(UniformVariable(start + (i*(end - start + 1))/NUMBER_SLOTS, start + ((i + 1)*(end - start + 1))/NUMBER_SLOTS).GetValue()));
#endif
		if(tmp == nodeId)
		{
			if(tmp > start)
				tmp--;
			else tmp++;
		}

		GenerateLocalInterest(tmp, interestsIndex, UPDATE_INTERESTS_DETAILS);


#ifdef ENABLE_RANDOM_NUMBER_OF_INTERESTS
	cout<<" i between: "<<start + (i*(end - start + 1))/nbrI<<" and "<<start + ((i + 1)*(end - start + 1))/nbrI<<" : "<<tmp<<endl;

#else
		cout<<" i between: "<<start + (i*(end - start + 1))/NUMBER_SLOTS<<" and "<<start + ((i + 1)*(end - start + 1))/NUMBER_SLOTS<<" : "<<tmp<<endl;
#endif
	}

}

void MOBITRADEProtocol::GenerateUpdatedInterests()
{
	for(list<int>::iterator iter = generatedInterestsIds.begin(); iter != generatedInterestsIds.end(); iter++)
	{
		GenerateLocalInterest(*iter, interestsIndex, UPDATE_INTERESTS_DETAILS);
	}
}


void MOBITRADEProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{

#ifdef DEBUG
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);
#endif
  m_ipv4 = ipv4;
  // Setting the timers callback functions
  helloTimer.SetFunction (&MOBITRADEProtocol::HelloTimerExpired , this);
  sessionsTimeOutTimer.SetFunction(&MOBITRADEProtocol::SessionsTimerExpired, this);
  logTimer.SetFunction(&MOBITRADEProtocol::LogTimerExpired, this);
  contentsGenerationTimer.SetFunction(&MOBITRADEProtocol::ContentsGenerationTimerExpired, this);
  interestsGenerationTimer.SetFunction(&MOBITRADEProtocol::InterestsGenerationTimerExpired, this);


  Simulator::ScheduleNow (&MOBITRADEProtocol::Start, this);
}

void MOBITRADEProtocol::ContentsGenerationTimerExpired()
{
#ifdef ENABLE_RANDOM_CHANNELS_SIZE
	if(contentsIndex < sizeChannels[nodeId])
	{
#else
	if(contentsIndex < MAX_CONTENTS_GENERATION_TIMES )
	{
#endif
		// Contents are generated periodically during the simulation at a given speed
		GenerateLocalContents(0 , NUMBER_OF_NODES - 1);

		// The content generation interval is proportional to the Sim_dur/ number of contents that will be generated
		unsigned int inter = 0;
#ifdef ENABLE_RANDOM_CHANNELS_SIZE
			inter = static_cast<unsigned int>(floor(SIM_DURATION/sizeChannels[nodeId]));
#else
			inter = static_cast<unsigned int>(floor(CONTENTS_GENERATION_INTERVAL));
#endif
			contentsGenerationTimer.Schedule(Seconds(inter));
			contentsIndex += 1;
	}
}

void MOBITRADEProtocol::InterestsGenerationTimerExpired()
{
	// Interests are generated only Once at the beginning of the simulation
	if(ACTIVATE_MALICIOUS_NODES)
	{
		// A scenario that includes malicious nodes
		if(interestsIndex == 0)
			SetActive(true);

		if(ENABLE_MERGED_INTERESTS)
		{
			if(nodeId <= 5)
			{
				//SetActive(false);
				GenerateLocalInterests(21 , 39);

			}else if( nodeId >= 6 && nodeId <= 20)
			{
				GenerateLocalInterests(21 , 29);

			}else if(nodeId >= 21 && nodeId <= 30)
			{
				GenerateLocalInterests(31 , 39);

			}else if(nodeId >= 31 && nodeId <= 40)
			{
				//GenerateLocalInterests(41 , 48);
				GenerateLocalInterests(21 , 29);

			}else if(nodeId >= 41 && nodeId <= 49)
			{
				//GenerateLocalInterests(11 , 19);
				GenerateLocalInterests(31 , 39);
			}
			return;

		}else
		{
			if(nodeId <= 10)
			{
				//Malicious
				GenerateLocalInterests(41 , 48);

			}
			else if(nodeId >= 11 && nodeId <= 20)
			{
				GenerateLocalInterests(21 , 29);
				//GenerateLocalInterests(41 , 48);
			}
			else if(nodeId >= 21 && nodeId <= 30)
			{
				GenerateLocalInterests(31 , 39);
			}else if(nodeId >= 31 && nodeId <= 40)
			{
				GenerateLocalInterests(21 , 29);

			}else if(nodeId >= 41 && nodeId <= 49)
			{
				GenerateLocalInterests(31 , 39);
			}
			return;

		}
	}else
	{
		// Collaborative scenario

		if(!ENABLE_THAT_SOME_NODES_COME_LATER)
		{
			// All the nodes generate their Interests at the same time, original case
			if(interestsIndex == 0)
			{
				SetActive(true);

#ifdef ENABLE_RANDOM_NUMBER_OF_INTERESTS
				GenerateLocalInterests(1 , 48);

#else
#ifdef	ENABLE_RANDOM_CHANNELS_SIZE
				if(nodeId <= 10)
				{
					if(nodeId <= 4)
						GenerateLocalInterests(11 , 19);
					else 	GenerateLocalInterests(21 , 29);

				}else if( nodeId >= 11 && nodeId <= 20)
				{
					if(nodeId <= 14)
						GenerateLocalInterests(21 , 29);
					else
						GenerateLocalInterests(31 , 39);

				}else if(nodeId >= 21 && nodeId <= 30)
				{
					if(nodeId <= 24)
						GenerateLocalInterests(31 , 39);
					else GenerateLocalInterests(41 , 48);

				}else if(nodeId >= 31 && nodeId <= 40)
				{
					if(nodeId <= 34)
						GenerateLocalInterests(41 , 48);
					else GenerateLocalInterests(1 , 9);

				}else if(nodeId >= 41 && nodeId <= 49)
				{
					if(nodeId <= 44)
						GenerateLocalInterests(1 , 9);
					else
						GenerateLocalInterests(11 , 19);
				}
#else
				if(nodeId <= 10)
				{
					if(nodeId <= 4)
						GenerateLocalInterests(11 , 19);
					else 	GenerateLocalInterests(21 , 29);

				}else if( nodeId >= 11 && nodeId <= 20)
				{
					if(nodeId <= 14)
						GenerateLocalInterests(21 , 29);
					else
						GenerateLocalInterests(31 , 39);

				}else if(nodeId >= 21 && nodeId <= 30)
				{
					if(nodeId <= 24)
						GenerateLocalInterests(31 , 39);
					else GenerateLocalInterests(41 , 48);

				}else if(nodeId >= 31 && nodeId <= 40)
				{
					if(nodeId <= 34)
						GenerateLocalInterests(41 , 48);
					else GenerateLocalInterests(1 , 9);

				}else if(nodeId >= 41 && nodeId <= 49)
				{
					if(nodeId <= 44)
						GenerateLocalInterests(1 , 9);
					else
						GenerateLocalInterests(11 , 19);
				}
#endif
#endif
			}else
			{
				// Update the details of the already generated Interests
				GenerateUpdatedInterests();
				ShowInterests(cout);
			}

		}else
		{
			// Not all the nodes generate Interests at the same time
			// Some nodes will come later
			cout<<"Nodes start and leave in the middle of the simulation"<<endl;

			if(nodeId <= 10 && interestsIndex == 7)
			{
				//SetActive(true);
				//GenerateLocalInterests(11 , 19);

			}else if(nodeId <= 10 && interestsIndex == 14)
			{
				//SetActive(false);
			}
			else if( nodeId > 10 && nodeId <= 19 && interestsIndex == 0)
			{
				SetActive(true);
				GenerateLocalInterests(21 , 29);
			}else if(nodeId >= 20 && nodeId <= 29 && interestsIndex == 0)
			{
				SetActive(true);
				GenerateLocalInterests(31 , 39);

			}else if(nodeId >= 30 && nodeId <= 39 && interestsIndex == 0)
			{
				SetActive(true);
				GenerateLocalInterests(41 , 48);

			}else if(nodeId >= 40 && nodeId <= 49 && interestsIndex == 0)
			{
				SetActive(true);
				GenerateLocalInterests(11 , 19);
			}

		}
	}

	if(ENABLE_THAT_SOME_NODES_COME_LATER || UPDATE_INTERESTS_DETAILS)
	{
		// Some nodes will generate their Interests later on during the simulation
		if(interestsIndex < MAX_INTERESTS_GENERATION_TIMES)
		{
			interestsGenerationTimer.Schedule(Seconds(INTERESTS_GENERATION_INTERVAL));
			interestsIndex += 1;
		}
	}
}

void MOBITRADEProtocol::HelloTimerExpired ()
{
  if(IsActive())
  {
	  SendHello ();
  }

  helloTimer.Schedule (Seconds(HELLO_INTERVAL));
  if(nodeId == 26)
	cout<<"Current simulation time: "<<Simulator::Now().GetSeconds()<<endl;
}

void MOBITRADEProtocol::SessionsTimerExpired()
{
  // Cleaning out of date Sessions
  CleanOutOfDateSessions();
  sessionsTimeOutTimer.Schedule (Seconds(SESSIONS_TIMER_INTERVAL ));
}

void MOBITRADEProtocol::LogTimerExpired()
{
	LogStatistics();
	logTimer.Schedule(Seconds(LOG_INTERVAL));
}

void MOBITRADEProtocol::ProcessHelloMsg (MOBITRADEMsgHeader &header, Ipv4Address sender)
{
  if(IsActive())
  {
	  // cout << "DTN node: " << mainAddress << ", HELLO message received from: "<<sender<<endl;
	  // Start a new session only if you have a bigger ID
	  if(TRACK_NODES_MEETINGS && !IsSessionActive(sender))
	  {
		  // Tracing number of meetings with each node
		  if(nodesNumberMeetings.find(sender) != nodesNumberMeetings.end())
		  {
			  nodesNumberMeetings[sender]++;
		  }else
		  {
			  nodesNumberMeetings[sender] = 1;
		  }
	  }

	  if(!IsSessionActive(sender) && mainAddress.Get() > sender.Get())
	  {
		  //  cout <<mainAddress<<" has a bigger ID, starting a new MobiTrade session with: "<<sender<<endl;
		  // Sending a packet containing the list of Interests
		  SendListOfInterests(sender);
	  }else
	  {

	  }
  }
}

void MOBITRADEProtocol::ProcessInterestMsg (Ptr<Packet> pkt, Ipv4Address sender)
{
	// cout<<"Node: "<<nodeId<<" recv interests: from node: "<<sender<<endl<<endl;
	// Init a new session with the sender
	if(!IsSessionActive(sender))
	{

		if( mainAddress.Get() < sender.Get())
		{
				SendListOfInterests(sender);
		}

		StartNewSession(sender);

	}else
	{
		ShowListSession(cout);
		NS_FATAL_ERROR("Session still active");
		return;
	}

	// Extracting the list of received Interests
	stringstream data;
	pkt->CopyData(&data, MAX_INTERESTS_SIZE);
	string receivedInterests(data.str());
	ChangeSessionState(sender, SENDING_WAITING_FOR_CONTENTS);

	//cout<<"Node: "<<nodeId<<" recv interests: "<<endl<<receivedInterests<<" from node: "<<sender<<endl<<endl;
	if(!receivedInterests.empty())
	{
		list<Interest *> listInterests;

		ReceivedNewInterests(receivedInterests, listInterests);
#ifdef DEBUG
		NS_ASSERT(listInterests.size() <= INTERESTS_STORAGE_CAPACITY);
#endif
		// Adding the list of received Interests
		map<Ipv4Address, StatisticsEntry* >::iterator iter = exchangedBytes.find(sender);
		(iter->second)->SetTheListOfReceivedInterests(listInterests);
		listInterests.clear();
		// Forwarding the first set of matching contents
		listInterests.clear();

		if(!(iter->second)->firstContentsForwarded)
		{
			if(!IsMalicious())
			{


#ifdef USE_TIT_FOR_TAT
				ForwardFirstContents(sender);
#else
				ForwardAllMatchingContents(sender);
#endif
			}
		}
	}

}

void MOBITRADEProtocol::ProcessContentMsg (Ptr<Packet> pkt, Ipv4Address sender)
{
	// Extracting the list of received Interests
	stringstream data;
	pkt->CopyData(&data, MAX_INTERESTS_SIZE);
	string receivedContent(data.str());

	//cout <<"Node: "<<nodeId<<" New content "<<receivedContent<<" msg received from: "<< sender<<endl;


	// Add the content to the local storage
	// Description Data sourceID TTL
	string contentData = GetContentData(receivedContent);
	Content * c = new Content(GetContentDescription(receivedContent), contentData, GetContentTTL(receivedContent), this);
	c->SetSourceId(GetContentSourceID(receivedContent));
	c->SetUtility(GetContentUtility(receivedContent));
	c->SetForeign();

	//cout<<"Node: "<<nodeId<<" recv content: "<<receivedContent<<endl;

	int state = GetSessionState(sender);

	if(state != SESSION_NOT_FOUND)
	{

		map<Ipv4Address, StatisticsEntry*>::iterator iter = exchangedBytes.find(sender);
		if(iter != exchangedBytes.end())
		{
			// Update the number of received bytes
			// I don't add a reward if i've already received this content

			(iter->second)->recvBytes += sizeof(contentData);
#ifdef USE_TIT_FOR_TAT
			if(!IsMalicious())
			{

				Content * next = (iter->second)->GetNextContentToForward();
				if(next != NULL)
				{
					// Sending the content
					SendContent(sender, next);

					//Update the number of sent bytes
					(iter->second)->sentBytes += static_cast<int>(next->GetDataSize());
					(iter->second)->UpdateInterestSentBytes(next->GetMatchingInterest(), next->GetDataSize());

					// Change the status of the Content to sent
					(iter->second)->SetContentForwarded(next);
					ChangeSessionState(sender, SENDING_WAITING_FOR_CONTENTS);

				}else
				{
					// Nothing more to send, ending the current session
					if(GetSessionState(sender) != END_SESSION_REQUESTED)
					{
						SendEndSession(sender);
						//CleanSession(sender);
					}
				}
				next = NULL;

			}
#endif
		}else
		{
			NS_FATAL_ERROR("Invalid session");
		}
	}

	AddNewContent(c);
	//(iter->second)->AddNewReceivedContent(c);
	c = NULL;

}

void MOBITRADEProtocol::ProcessEndSessionMsg (Ptr<Packet> pkt, Ipv4Address dest)
{

	if(IsSessionActive(dest))
	{
		//cout<<Simulator::Now().GetSeconds()<<" me: "<<nodeId<<" A request to close a session is received from: "<< dest<<endl;
		if(GetSessionState(dest) != END_SESSION_REQUESTED)
		{
			SendEndSession(dest);
		}
		CleanSession(dest);
	}
}

void MOBITRADEProtocol::CleanSession (Ipv4Address dest)
{
//	sem_wait(&sessionsSem);

	// Removing the exchanged bytes statistics entry
	map<Ipv4Address, StatisticsEntry *>::iterator iter2 = exchangedBytes.find(dest);
	if(iter2 != exchangedBytes.end())
	{
		// Adding the received interests to the store and updating their utilities as well as the
		// the utilities of local stored interests
		//cout<<"Session expired, adding the received interests and updating utilities"<<endl;
#ifdef DEBUG
		NS_ASSERT(iter2->second != NULL);
#endif
		cout<<Simulator::Now().GetSeconds()<<" Node: "<<nodeId<<" exchanges with: "<<iter2->first<<" recv i: "<<(iter2->second)->numberOfReceivedInterests <<" "<<(iter2->second)->mapReceivedInterests.size()<<" Matching c: "<<(iter2->second)->numberMatchingContents<<" for c: "<<(iter2->second)->numberForwardedContents<<endl;

		addNewInterests(const_cast<Ipv4Address &>(dest));

		delete iter2->second;

		exchangedBytes.erase(iter2);
	}else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::ProcessContentMsg statistics entry not found.");
	}

	currentActiveSessions.erase(dest);
//	sem_post(&sessionsSem);
}

TypeId MOBITRADEProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MOBITRADEProtocol").SetParent<Ipv4RoutingProtocol> ().AddConstructor<MOBITRADEProtocol> ();
  return tid;
}

void  MOBITRADEProtocol::SetDefaultRoute (Ipv4Address nextHop, uint32_t interface)
{
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateDefaultRoute (nextHop, interface);
  if(m_defaultRoute != NULL)
    delete m_defaultRoute;
  m_defaultRoute = route;
  route = NULL;
}


void MOBITRADEProtocol::UpdateNodeLastMeetingTime(Ipv4Address node)
{
  map<Ipv4Address,  Time>::iterator iter = nodesLastMeetingTimeMap.find(node);
  if(iter != nodesLastMeetingTimeMap.end())
    {
	  iter->second = Simulator::Now ();
    }
  else
    {
	  nodesLastMeetingTimeMap[node] = Simulator::Now ();
    }
}

bool MOBITRADEProtocol::NodeAlreadyEncountered(Ipv4Address node)
{
	map<Ipv4Address,  Time>::iterator iter = nodesLastMeetingTimeMap.find(node);
	if(iter != nodesLastMeetingTimeMap.end())
	{
		 return true;
	}
	else
	{
		return false;
	}
}

Time MOBITRADEProtocol::GetNodeLastMeetingTime(Ipv4Address node)
{
	map<Ipv4Address, Time>::iterator iter =  nodesLastMeetingTimeMap.find(node);
	if(iter != nodesLastMeetingTimeMap.end())
	{
		  return iter->second;
	}
	NS_FATAL_ERROR("Node has not been already encountered");
}

void MOBITRADEProtocol::StartNewSession(Ipv4Address node)
{
	sem_wait(&sessionsSem);
	currentActiveSessions[node] = HELLO_MSG_RECEIVED;
	InitExchangedBytes(node);


	weight = ( (((Simulator::Now().GetSeconds() - startTime) - lastMeeting)/TIME_WINDOW));

	/*if(NodeAlreadyEncountered(node))
		weight = ( (((Simulator::Now().GetSeconds() - startTime) - GetNodeLastMeetingTime(node).GetSeconds())/TIME_WINDOW));
	else weight = ( (((Simulator::Now().GetSeconds() - startTime))/TIME_WINDOW));
	*/
	if(weight > 1 || weight <= 0)
	{
		NS_FATAL_ERROR("Invalid weight");
	}

	// Update source node last meeting time
	UpdateNodeLastMeetingTime(node);

	lastMeeting = Simulator::Now().GetSeconds() - startTime;
	sem_post(&sessionsSem);
}

bool MOBITRADEProtocol::IsSessionActive(Ipv4Address& node)
{
	sem_wait(&sessionsSem);

	map<Ipv4Address, int >::iterator iter = currentActiveSessions.find(node);
	if(iter != currentActiveSessions.end())
	{
		sem_post(&sessionsSem);
		return true;
	}
	else
	{
		sem_post(&sessionsSem);
		return false;
	}

}

void MOBITRADEProtocol::ChangeSessionState(Ipv4Address& node, int state)
{
//	sem_wait(&sessionsSem);

	map<Ipv4Address, int >::iterator iter = currentActiveSessions.find(node);
	if(iter != currentActiveSessions.end())
	{
		iter->second = state;
	}
	else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::ChangeSessionState Session not found.");
	}
	//sem_post(&sessionsSem);
}

int MOBITRADEProtocol::GetSessionState(Ipv4Address& node)
{

	map<Ipv4Address, int >::iterator iter = currentActiveSessions.find(node);
	if(iter != currentActiveSessions.end())
	{
		int tmp = iter->second;
		return tmp;
	}
	else
	{

		return SESSION_NOT_FOUND;
	}
}


void MOBITRADEProtocol::RemoveSession(Ipv4Address& node)
{
	sem_wait(&sessionsSem);

	map<Ipv4Address, int >::iterator iter = currentActiveSessions.find(node);
	if(iter != currentActiveSessions.end())
	{
		currentActiveSessions.erase(iter);
		cout<<"Session with: "<<node<<" Removed"<<endl;
	}
	else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::RemoveSession Session not found.");
	}
	sem_post(&sessionsSem);
}

// To Verify
void MOBITRADEProtocol::CleanOutOfDateSessions()
{
	sem_wait(&sessionsSem);
	list<Ipv4Address> listToDelete;

	map<Ipv4Address, int >::iterator iter = currentActiveSessions.begin();
	while(iter != currentActiveSessions.end())
	{
		Ipv4Address rem = iter->first;
		map<Ipv4Address, StatisticsEntry* >::iterator iter2 = exchangedBytes.find(iter->first);
		double diff = Simulator::Now().GetSeconds() - GetNodeLastMeetingTime(iter->first).GetSeconds();
		if(diff >= SESSIONS_TIME_OUT)
		{

			// Removing the session if it comes out of date

			// Removing the exchanged bytes statistics entry
			if(iter2 != exchangedBytes.end())
			{
				// Adding the received interests to the store and updating their utilities as well as the
				// the utilities of local stored interests
				//cout<<"Session expired, adding the received interests and updating utilities"<<endl;
				cout<<Simulator::Now().GetSeconds()<<" Node: "<<nodeId<<" exchanges with: "<<iter->first<<" recv i: "<<(iter2->second)->numberOfReceivedInterests <<" "<<(iter2->second)->mapReceivedInterests.size()<<" Matching c: "<<(iter2->second)->numberMatchingContents<<" for c: "<<(iter2->second)->numberForwardedContents<<endl;
				addNewInterests(const_cast<Ipv4Address &>(iter->first));

				//SendEndSession(iter2->first);

				delete iter2->second;
				exchangedBytes.erase(iter2);
			}else
			{
				NS_FATAL_ERROR("MOBITRADEProtocol::CleanOutOfDateSessions statistics entry not found.");
			}

			// Removing the session entry
			listToDelete.push_back(iter->first);

		}
		iter++;
	}

	for(list<Ipv4Address>::iterator iter3 = listToDelete.begin(); iter3 != listToDelete.end(); iter3++)
	{
		currentActiveSessions.erase(*iter3);
	}

	listToDelete.clear();

	sem_post(&sessionsSem);
}

void MOBITRADEProtocol::CleanOutOfDateInterests()
{
	// We don't clean anything if the TTL exceeds the simulation duration ( Infinite TTL)
	if(DEFAULT_INTEREST_TTL < SIM_DURATION)
	{
		sem_wait(&interestsSem);
		std::map<std::string, Interest *>::iterator iter = mapInterests.begin();
		while(iter != mapInterests.end())
		{
			if((iter->second)->UpdateTTL())
			{
				//cout<<"Removing an out of date Interest."<<endl;
				(iter->second)->SyncWithMatchingContents();
				delete iter->second;
				mapInterests.erase(iter);
			}
			iter++;
		}
		sem_post(&interestsSem);
	}
}

void MOBITRADEProtocol::CleanOutOfDateContents()
{
	{
		sem_wait(&contentsSem);
		{
			std::map<std::string, Content *>::iterator iter = mapContents.begin();
			while(iter != mapContents.end())
			{
				if((iter->second)->UpdateTTL())
				{
					(iter->second)->SyncWithMatchingInterest();
					delete iter->second;
					mapContents.erase(iter);
				}
				iter++;
			}
		}

		{
			std::map<std::string, Content *>::iterator iter = generatedContents.begin();
			while(iter != generatedContents.end())
			{
				if((iter->second)->UpdateTTL())
				{
					(iter->second)->SyncWithMatchingInterest();
					delete iter->second;
					generatedContents.erase(iter);
				}
				iter++;
			}
		}
		sem_post(&contentsSem);
	}
}

// Description#Utility#TTL

string MOBITRADEProtocol::GetInterestDescription(string &interest)
{
#ifdef DEBUG
	NS_ASSERT(!interest.empty());
#endif
	size_t pos = interest.find_first_of('#');
	if(pos != string::npos)
	{
		return interest.substr(0, pos );
	}else
	{
		cerr<<"Parsing: "<<interest<<endl;
		NS_FATAL_ERROR("MOBITRADEProtocol::GetInterestDescription Invalid interest.");
	}
}

double MOBITRADEProtocol::GetInterestUtility(string &interest)
{
#ifdef DEBUG
	NS_ASSERT(!interest.empty());
#endif
	string tmp = interest;
	size_t pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	size_t pos2 = tmp.find_first_of('#');

	if(pos2 != string::npos)
	{
		return atof(tmp.substr(0, pos2).c_str());

	}else
	{
		cerr<<"Parsing: "<<interest<<endl;
		NS_FATAL_ERROR("MOBITRADEProtocol::GetInterestUtility Invalid interest.");
	}
}


double MOBITRADEProtocol::GetInterestTTL(string &interest)
{
#ifdef DEBUG
	NS_ASSERT(!interest.empty());
#endif
	string tmp = interest;
	size_t pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	size_t pos2 = tmp.find_first_of('#');
	tmp = tmp.substr(pos2 + 1);

	if(!tmp.empty())
	{
		return atof(tmp.c_str());

	}else
	{
		cerr<<"Parsing: "<<interest<<endl;
		NS_FATAL_ERROR("MOBITRADEProtocol::GetInterestTTL Invalid interest.");
	}
}



// Description#Data#sourceID#TTL#Utility
string MOBITRADEProtocol::GetContentDescription(string & content)
{

#ifdef DEBUG
	NS_ASSERT(!content.empty());
#endif

	size_t pos = content.find_first_of('#');
	if(pos != string::npos)
	{
		string res = content.substr(0, pos);
		if(res.empty())
		{
			cerr<<"Parsing :"<<content<<endl;
			NS_FATAL_ERROR("MOBITRADEProtocol::GetContentDescription Invalid Content");
		}
		return res;
	}
	else
	{
		cerr<<"Parsing :"<<content<<endl;
		NS_FATAL_ERROR("MOBITRADEProtocol::GetContentDescription Invalid Content");
	}
}


// Description#Data#sourceID#TTL#Utility
string MOBITRADEProtocol::GetContentData(string & content)
{
#ifdef DEBUG
	NS_ASSERT(!content.empty());
#endif
	string tmp = content;
	size_t pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');

	if(pos1 != string::npos)
	{
		string res = tmp.substr(0, pos1);
		if(res.empty())
		{
			cerr<<"Parsing :"<<content<<endl;
			NS_FATAL_ERROR("MOBITRADEProtocol::GetContentData Invalid Content");
		}
		return res;
	}
	else
	{
		cerr<<"Parsing :"<<content<<endl;
		NS_FATAL_ERROR("MOBITRADEProtocol::GetContentData Invalid Content");
	}
}

// Description#Data#sourceID#TTL#Utility
string MOBITRADEProtocol::GetContentSourceID(string & content)
{
#ifdef DEBUG
	NS_ASSERT(!content.empty());
#endif
	string tmp = content;
	size_t pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');

	if(pos1 != string::npos)
	{
		string res = tmp.substr(0, pos1);
		if(res.empty())
		{
			cerr<<"Parsing :"<<content<<endl;
			NS_FATAL_ERROR("MOBITRADEProtocol::GetContentSourceID Invalid Content");
		}
		return res;
	}
	else
	{
		cerr<<"Parsing :"<<content<<endl;
		NS_FATAL_ERROR("MOBITRADEProtocol::GetContentSourceID Invalid Content");
	}
}


// Description#Data#sourceID#TTL#Utility
double MOBITRADEProtocol::GetContentTTL(string & content)
{
#ifdef DEBUG
	NS_ASSERT(!content.empty());
#endif
	string tmp = content;
	size_t pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');


	if(pos1 != string::npos)
	{
		string res = tmp.substr(0, pos1);
		if(res.empty())
		{
			cerr<<"Parsing :"<<content<<endl;
			NS_FATAL_ERROR("MOBITRADEProtocol::GetContentTTL Invalid Content");
		}
		return atof(res.c_str());
	}
	else
	{
		cerr<<"Parsing :"<<content<<endl;
		NS_FATAL_ERROR("MOBITRADEProtocol::GetContentTTL Invalid Content");
	}
}

// Description#Data#sourceID#TTL#Utility
double MOBITRADEProtocol::GetContentUtility(string & content)
{
#ifdef DEBUG
	NS_ASSERT(!content.empty());
#endif
	string tmp = content;
	size_t pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');
	tmp = tmp.substr(pos1 + 1);
	pos1 = tmp.find_first_of('#');


	if(pos1 != string::npos)
	{
		string res = tmp.substr(pos1 + 1);
		if(res.empty())
		{
			cerr<<"Parsing :"<<content<<endl;
			NS_FATAL_ERROR("MOBITRADEProtocol::GetContentUtility Invalid Content");
		}
		return atof(res.c_str());
	}
	else
	{
		cerr<<"Parsing :"<<content<<endl;
		NS_FATAL_ERROR("MOBITRADEProtocol::GetContentUtility Invalid Content");
	}
}

Interest * MOBITRADEProtocol::GetTheInterestHavingTheLowestUtility(double & lu)
{
	double smallestUtility = numeric_limits<double>::max();
	Interest * tmp = NULL;
	if(mapInterests.empty())
		return NULL;
	for(map<string,  Interest*>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
	{
		if((iter->second)->GetUtility() < smallestUtility)
		{
			smallestUtility = (iter->second)->GetUtility();
			tmp = iter->second;
		}
	}

	if(tmp == NULL)
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::GetTheInterestHavingTheLowestUtility Invalid Interest found.");
	}
	lu = smallestUtility;
	return tmp;
}

Interest * MOBITRADEProtocol::GetTheInterestHavingTheLowestUtilityWithAssociatedContents(Content *c, double & lu)
{
	double smallestUtility = numeric_limits<double>::max();
	Interest * tmp = NULL;
	if(mapInterests.empty())
		return NULL;
	for(map<string,  Interest*>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
	{
#ifdef DEBUG
		NS_ASSERT(!(iter->second)->IsLocal());
#endif
		if((iter->second)->GetUtility() < smallestUtility &&  (iter->second)->HaveOtherMatchingContents(c))
		{
			smallestUtility = (iter->second)->GetUtility();
			tmp = iter->second;
		}
	}

	if(tmp == NULL)
	{
		cout<<"Considered content: "<<c->GetDescription()<<endl;
		ShowInterestsVsContents(cout);
		ShowContents(cout);
		NS_FATAL_ERROR("MOBITRADEProtocol::GetTheInterestHavingTheLowestUtilityWithAssociatedContents Invalid Interest found.");
	}

	lu = smallestUtility;
#ifdef DEBUG
	NS_ASSERT(tmp != NULL);
#endif
	return tmp;
}

void MOBITRADEProtocol::AddNewInterest(Interest * i)
{

	sem_wait(&interestsSem);

	map<string, Interest * >::iterator iter = mapInterests.find(i->GetDescription());

	if(iter == mapInterests.end())
	{
		// does not exists already
		if(mapInterests.size() < INTERESTS_STORAGE_CAPACITY)
		{
			mapInterests[i->GetDescription()] = i;
			SyncInterestToAvailableContent(i);

		}else
		{

			if(ACTIVATE_MOBITRADE_POLICIES)
			{
				//cout<<"The interests storage is full, ";
				// Find the Interest having the lowest utility value in the local storage
				double su = 0;
				Interest * si = GetTheInterestHavingTheLowestUtility(su);
				if(i->GetUtility() > si->GetUtility())
				{
					// Drop the stored interest
					//TODO:
					//cout<<"Dropping the stored interest having the utility: "<<si->GetUtility()<<endl;
					si->SyncWithMatchingContents();

					mapInterests.erase(si->GetDescription());
					delete si;

					// Add the new Interest
					mapInterests[i->GetDescription()] = i;
					SyncInterestToAvailableContent(i);
				}else
				{
					// Drop the new received Interest
					//cout<<"Dropping the new received interest."<<endl;
					i->SyncWithMatchingContents();
					delete i;
				}
			}else
			{
				// Select random interest and drop it
				/*int toDel = UniformVariable().GetValue(0, mapInterests.size());
				int j = 0;
				map<string, Interest * >::iterator iter2 = mapInterests.begin();
				for(; iter2 != mapInterests.end() && j < toDel; j++,iter2++);
				//cout<<"Dropping a random Interest having the utility: "<<(iter2->second)->GetUtility()<<endl;
				delete iter2->second;
				mapInterests.erase(iter2);
				// Add new Content
				mapInterests[i->GetDescription()] = i;
				*/
				i->SyncWithMatchingContents();
				delete i;
			}

		}
	}else
	{
		// Can happen when the node generates an Interest in the middle of the
		// Simulation while the same Interest has been already generated by
		// another node some time before !

		// Then, we have to convert the foreign Interest to a Local one


		(iter->second)->SetForeign(false);
		(iter->second)->SetUtility(INITIAL_LOCAL_INTEREST_UTILITY);
		(iter->second)->SetTTL(i->GetTTL());
		// Setting that all the contents currently in the buffer and that matches
		// The current Interest as being delivered
		list<Content *> match;
		(iter->second)->GetMatchingContents(match);

		for(list<Content *>::iterator tmpIter = match.begin(); tmpIter != match.end(); tmpIter++)
		{
			AddNewDeliveredContent(*tmpIter, 0);
		}

		match.clear();
		// Dropping the new received one
		delete i;
	}

	sem_post(&interestsSem);
}

Content * MOBITRADEProtocol::GetTheContentHavingTheLowestUtility(double & lu)
{
	double smallestUtility = numeric_limits<double>::max();
	Content * tmp = NULL;
	if(mapContents.empty())
		return NULL;
	for(map<string,  Content*>::iterator iter = mapContents.begin(); iter != mapContents.end(); iter++)
	{
		if((iter->second)->GetUtility() < smallestUtility )
		{
			smallestUtility = (iter->second)->GetUtility();
			tmp = iter->second;
		}
	}
	if(tmp == NULL)
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::GetTheInterestHavingTheLowestUtility Invalid Interest found.");
	}
	lu = smallestUtility;
	return tmp;
}

void MOBITRADEProtocol::AddNewDeliveredContent(Content *c, double delay)
{
	if(deliveredContents.empty())
	{
		deliveredContents[c->GetDescription()] = delay;
		deliveredPerChannel[c->GetMatchingInterest()->GetDescription()] = 1;
	}
	else
	{
		map<string, double>::iterator iter = deliveredContents.find(c->GetDescription());
		if(iter == deliveredContents.end())
		{
			deliveredContents[c->GetDescription()] = delay;
			deliveredPerChannel[c->GetMatchingInterest()->GetDescription()] ++;
		}

	}

}


bool MOBITRADEProtocol::ContentExists(std::string desc)
{
	std::map<std::string, Content *>::iterator iter = mapContents.find(desc);
	std::map<std::string, Content *>::iterator iter2 = generatedContents.find(desc);

	return (iter != mapContents.end() || iter2 != generatedContents.end());
}

bool MOBITRADEProtocol::InterestExists(std::string desc)
{
	std::map<std::string, Interest *>::iterator iter = mapInterests.find(desc);
	return (iter != mapInterests.end());

}

void MOBITRADEProtocol::AddNewGeneratedContent(Content *c)
{
	map<string, Content *>::iterator iter = generatedContents.find(c->GetDescription());
	if(iter == generatedContents.end() || generatedContents.empty())
	{
		generatedContents[c->GetDescription()] = c;
	}else
	{
		NS_FATAL_ERROR("The same content has been already generated.");
	}
}


void MOBITRADEProtocol::AddNewContent(Content * c)
{
	if(!c->IsForeign())
	{
		// locally generated Contents are stored in a separate buffer
		SyncContentToAvailableInterests(c);
		if(c->GetMatchingInterest() != NULL)
		{
			c->GetMatchingInterest()->SetLocal(true);
		}

		if(generatedContents.size() + mapContents.size() >= CONTENTS_STORAGE_CAPACITY)
		{
			// re Adjust Content storage
			AddNewContentFullStorage(c);
		}else
		{
			AddNewGeneratedContent(c);
		}
		return;
	}

	CleanOutOfDateContents();

	if(ContentExists(c->GetDescription()))
	{
		// Don't add duplicate contents
		delete c;
		return;
	}

	if(IsMalicious())
	{
		double delay = 0;
		bool valid;
		if(MatchesLocalInterest(c, &delay, valid))
		{
			AddNewDeliveredContent(c, delay);
		}

		// We delete the content once it is delivered, we don't forward it to other users
		c->SyncWithMatchingInterest();
		delete c;
		return;
	}

	bool valid;
	double delay = 0;

	if(MatchesLocalInterest(c, &delay, valid))
	{
		AddNewDeliveredContent(c, delay);
		// Even if the Content is delivered to us( matches a local Interest) we keep it in the buffer, it could be useful for other
		// devices.
	}


	sem_wait(&contentsSem);


	map<string, Content * >::iterator iter = mapContents.find(c->GetDescription());
	if(iter == mapContents.end())
	{
		if(mapContents.size() + generatedContents.size() < CONTENTS_STORAGE_CAPACITY)
		{
			if(ACTIVATE_MOBITRADE_POLICIES)
			{
				// There is still some space, add the content while considering the amount
				//of space allowed for its matching interest
				AddNewContentAvailableStorage(c);
			}else
			{

				// Randomly adding the new content
				mapContents[c->GetDescription()] = c;
			}

		}else
		{
			if(ACTIVATE_MOBITRADE_POLICIES)
			{
				AddNewContentFullStorage(c);
			}else
			{
				// Drop last content | random
				c->SyncWithMatchingInterest();
				delete c;
			}
		}
	}else
	{
		NS_FATAL_ERROR("Content already exists.");
	}
	sem_post(&contentsSem);

}


void MOBITRADEProtocol::DispatchAvailableStorage()
{
	double sumInterestsUtilities = 0;
	if(!mapInterests.empty())
	{
		for(map<std::string, Interest *>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
		{
			sumInterestsUtilities += iter->second->GetUtility();
		}

		double spaceToAdd = 0;
		if(sumInterestsUtilities < CONTENTS_STORAGE_CAPACITY*CONTENT_PACKET_SIZE)
		{
			spaceToAdd = CONTENTS_STORAGE_CAPACITY*CONTENT_PACKET_SIZE - sumInterestsUtilities;
			double percentage = spaceToAdd / 100;
			//cout<<"space to add: "<<spaceToAdd<<" percentage: "<<percentage <<endl;
			map<std::string, Interest *>::iterator iter = mapInterests.begin();

			for(; iter != mapInterests.end(); iter++)
			{
				double w = (iter->second)->GetUtility()*100/sumInterestsUtilities;
				double nu = (iter->second)->GetUtility() + percentage*w;
				(iter->second)->SetUtility(nu);
				spaceToAdd -= percentage*w;
			}
#ifdef DEBUG
			NS_ASSERT(iter == mapInterests.end());
#endif
			UpdateInterestsAllowedBytes();

		}
	}

}

void MOBITRADEProtocol::CleanUpTheStorage()
{

	// Cleaning up Channels that exceeds their allowed utilities
	while(mapContents.size() + generatedContents.size() > CONTENTS_STORAGE_CAPACITY)
	{
			Interest * selectedI = GetInterestThatExceedsTheMostItsShare();
#ifdef DEBUG
			NS_ASSERT(selectedI != NULL);
			//While the occupied space is still greater then the utility
			NS_ASSERT(!selectedI->IsLocal());
#endif
			while(selectedI->ExceedsAllowedUtilityBytes() && mapContents.size() + generatedContents.size() > CONTENTS_STORAGE_CAPACITY)
			{

				// Get the oldest or random content
				Content * sc = NULL;
#ifdef ENABLE_TTL_AWAIRNESS
				sc = selectedI->GetOldestMatchingContent();
#else
				sc = selectedI->GetRandomMatchingContent();
#endif
				// Deleting the selected Content
				if(sc == NULL)
				{
					cout<<"Interest util: "<<selectedI->GetUtility()<<" allowed storage: "<<selectedI->GetInterestAllowedBytes()<<" occupied: "<<selectedI->GetTotalSizeOfMatchingContents() <<" local: "<<selectedI->IsLocal()<<endl;
					selectedI->ShowMatchingContents(cout);
					ShowInterests(cout);
					NS_FATAL_ERROR("Invalid selected Content");
				}

				sc->SyncWithMatchingInterest();

				bool removed = false;
#ifdef DEBUG
				NS_ASSERT(sc->IsForeign());
#endif
				removed = RemoveForeignContent(sc);
#ifdef DEBUG
				NS_ASSERT(removed == true);
#endif
				delete sc;
			}
	}
}
void MOBITRADEProtocol::AddNewContentAvailableStorage(Content *c)
{
	UpdateInterestsAllowedBytes();

	// The number of contents within a channel could exceed the channel share but never the
	// allowed number of bytes expressed by the channel utility
	Interest * i = c->GetMatchingInterest();
	Content *sc = NULL;
#ifdef DEBUG
	NS_ASSERT(i != NULL);
#endif
	if(i->ExceedsAllowedUtilityBytes())
	{

		if(i->GetNumberOfMatchingContents() == 1)
		{
			c->SyncWithMatchingInterest();
			delete c;
			return;
		}

#ifdef ENABLE_TTL_AWAIRNESS
		sc = i->GetOldestMatchingContent();
#else
		sc = i->GetRandomMatchingContent();
#endif
		// Deleting the selected Content
		if(sc == NULL)
		{
			cout<<"Node: "<<nodeId<<" New received Content: "<<c->GetDescription()<<endl;
			cout<<"Interest util: "<<i->GetUtility()<<" allowed storage: "<<i->GetInterestAllowedBytes()<<endl;
			i->ShowMatchingContents(cout);
			ShowInterests(cout);
			NS_FATAL_ERROR("Invalid selected Content");
		}

		sc->SyncWithMatchingInterest();
		if(sc != c)
		{
			bool removed = false;
			if(sc->IsForeign())
				removed = RemoveForeignContent(sc);
			else removed = RemoveLocalContent(sc);
#ifdef DEBUG
			NS_ASSERT(removed == true);
#endif
			// Add the new Content
			mapContents[c->GetDescription()] = c;
		}
		delete sc;

	}else
	{
		// Add the new Content
		mapContents[c->GetDescription()] = c;
#ifdef DEBUG
		NS_ASSERT(mapContents.size() <= CONTENTS_STORAGE_CAPACITY);
#endif
	}
	i = NULL;
}

void MOBITRADEProtocol::AddNewContentFullStorage(Content *c)
{

	UpdateInterestsAllowedBytes();

	{
		// See first whether the matching interest exceeds its allowed bytes or storage share
		Interest * mi = c->GetMatchingInterest();
		if(mi != NULL)
		{
			if(mi->ExceedsAllowedUtilityBytes() || mi->ExeedsAllowedStorage())
			{
				Content *sc = NULL;
#ifdef ENABLE_TTL_AWAIRNESS
				sc = mi->GetOldestMatchingContent();
#else
				sc = mi->GetRandomMatchingContent();
#endif

				if(sc == NULL)
				{
					cout<<"NodeId: "<<nodeId<<" Interest: "<<mi->GetDescription()<<" local: "<<mi->IsLocal()<<endl;
					mi->ShowMatchingContents(cout);
					NS_FATAL_ERROR("Invalid selected Content");
				}

				sc->SyncWithMatchingInterest();

				if(sc != c)
				{
					bool removed = false;
					if(sc->IsForeign())
						removed = RemoveForeignContent(sc);
					else removed = RemoveLocalContent(sc);
#ifdef DEBUG
					NS_ASSERT(removed == true);
#endif
					if(c->IsForeign())
						mapContents[c->GetDescription()] = c;
					else
						generatedContents[c->GetDescription()] = c;

				}
				delete sc;
				return;
			}
		}
	}

	// We should look for the Interest That exceeds the most it storage share and remove one content
	// in counter part of the new received one

	// We should also consider the Drop Oldest utility if if the option is enabled
	Interest * selectedI = GetInterestThatExceedsTheMostItsShare();
	//Interest * selectedI = GetInterestWithSmallestUtilityThatExceedsItsAllowedStorage();
	if(selectedI == NULL)
	{
		// There is no Interest that exceed its storage
		// Get the interest with the smallest utility that have some associated contents

		double su = 0;
		selectedI = GetTheInterestHavingTheLowestUtilityWithAssociatedContents(c, su);

		if(selectedI != NULL)
		{
			Content *sc = NULL;
#ifdef ENABLE_TTL_AWAIRNESS
			sc = selectedI->GetOldestMatchingContent();
#else
			sc = selectedI->GetRandomMatchingContent();
#endif

			if(sc == NULL)
			{
				cout<<"Interest: "<<selectedI->GetDescription()<<" local: "<<selectedI->IsLocal()<<endl;
				selectedI->ShowMatchingContents(cout);
				NS_FATAL_ERROR("Invalid selected Content");
			}

			sc->SyncWithMatchingInterest();

			if(sc != c)
			{
				bool removed = false;
				if(sc->IsForeign())
					removed = RemoveForeignContent(sc);
				else removed = RemoveLocalContent(sc);
#ifdef DEBUG
				NS_ASSERT(removed == true);
#endif
				if(c->IsForeign())
					mapContents[c->GetDescription()] = c;
				else
					generatedContents[c->GetDescription()] = c;

				if(c->GetMatchingInterest() != NULL)
				{
					if(c->GetMatchingInterest()->ExceedsAllowedUtilityBytes())
					{
						cout<<"Interest: "<<c->GetMatchingInterest()->GetDescription()<<" local: "<<c->GetMatchingInterest()->IsLocal()<<" utility: "<<c->GetMatchingInterest()->GetUtility()<<" TotalS: "<<c->GetMatchingInterest()->GetTotalSizeOfMatchingContents()<<endl;
						c->GetMatchingInterest()->ShowMatchingContents(cout);
						NS_FATAL_ERROR("Invalid Interest utility");
					}
				}

			}
			delete sc;

		}else
		{
			NS_FATAL_ERROR("Invalid Interest/Content Match");
		}

	}else
	{
		// An Interest that exceeds its amount of storage is found
		Content *sc = NULL;

#ifdef ENABLE_TTL_AWAIRNESS
		sc = selectedI->GetOldestMatchingContent();
#else
		sc = selectedI->GetRandomMatchingContent();
#endif
		if(sc == NULL)
		{
			selectedI->ShowMatchingContents(cout);
			ShowInterests(cout);
			cout<<"Received Content: "<<c->GetDescription()<<endl;
			NS_FATAL_ERROR("Invalid selected Content");
		}

		sc->SyncWithMatchingInterest();
		if(sc != c)
		{
			bool removed = false;
			if(sc->IsForeign())
				removed = RemoveForeignContent(sc);
			else removed = RemoveLocalContent(sc);
#ifdef DEBUG
			NS_ASSERT(removed == true);
#endif
			if(c->IsForeign())
				mapContents[c->GetDescription()] = c;
			else
				generatedContents[c->GetDescription()] = c;

			if(c->GetMatchingInterest() != NULL)
			{
				if(c->GetMatchingInterest()->ExceedsAllowedUtilityBytes())
				{
					cout<<"Interest: "<<c->GetMatchingInterest()->GetDescription()<<" local: "<<c->GetMatchingInterest()->IsLocal()<<" utility: "<<c->GetMatchingInterest()->GetUtility()<<" TotalS: "<<c->GetMatchingInterest()->GetTotalSizeOfMatchingContents()<<endl;
					c->GetMatchingInterest()->ShowMatchingContents(cout);
					NS_FATAL_ERROR("Invalid Interest utility");
				}
			}
		}

		delete sc;

		selectedI = NULL;
	}

	if(mapContents.size() + generatedContents.size() > CONTENTS_STORAGE_CAPACITY)
	{
		cout<<"Stored Contents: "<<mapContents.size()<<endl;
		ShowContents(cout);
		NS_FATAL_ERROR("INVALID Content storage size.");
	}
}

bool MOBITRADEProtocol::RemoveForeignContent(Content *c)
{
	map<string, Content *>::iterator iter = mapContents.find(c->GetDescription());
	if(iter != mapContents.end())
	{
		mapContents.erase(iter);
		return true;
	}
	return false;
}

bool MOBITRADEProtocol::RemoveLocalContent(Content *c)
{
	map<string, Content *>::iterator iter = generatedContents.find(c->GetDescription());
	if(iter != generatedContents.end())
	{
		generatedContents.erase(iter);
		return true;
	}
	return false;
}

string MOBITRADEProtocol::GetListOfInterests(int * numberOfInterests)
{
	CleanOutOfDateInterests();

	if(mapInterests.empty())
	{
		*numberOfInterests = 0;
		return string("");
	}

	map<std::string, Interest *>::iterator iter = mapInterests.begin();
	string resume;
	while(iter != mapInterests.end())
	{
		if(iter != mapInterests.begin())
		{
			resume.append("\n");
		}
		resume.append(iter->second->GetResume());
		(*numberOfInterests)++;
		iter++;
	}

	return resume;
}

bool MOBITRADEProtocol::ReceivedNewInterests(std::string interests, list<Interest *> & listInterests)
{
	if(!interests.empty())
	{
		// Parse the string containing the list of received Interests

		stringstream ss;
		ss << interests;
		char tmpLine[1024];

		ss.getline(tmpLine, 1023, '\n');
		while( strlen(tmpLine) > 0)
		{
			Interest * tmpI = new Interest(string(tmpLine), true, this);
			listInterests.push_back(tmpI);
			tmpI = NULL;
			ss.getline(tmpLine, 1023, '\n');
		}

		return true;
	}else
	{
		// The list of received Interests is empty
		cerr<<"The list of received Interests is empty"<<endl;
		return false;
	}

}

void MOBITRADEProtocol::ForwardAllMatchingContents(Ipv4Address & sender)
{
	// Forwarding the matching Contents for generosity
	CleanOutOfDateContents();
	sem_wait(&sessionsSem);

	map<Ipv4Address, StatisticsEntry*>::iterator iter = exchangedBytes.find(sender);
	std::map<Interest *, int> & recvInterests = (iter->second)->mapReceivedInterests;

	(iter->second)->firstContentsForwarded = true;


	// Looking for matching Contents
	if(mapContents.size() > 0 || generatedContents.size() > 0)
	{
		int i =0;

		for(map<Interest*, int>::iterator iter2 = recvInterests.begin(); iter2 != recvInterests.end(); iter2++, i++)
		{
			AddContentsToForward(sender, iter2->first, i);
		}

		// Forwarding the matching Contents for generosity
		if(iter != exchangedBytes.end())
		{

			Content * next = NULL;
			while( (next = (iter->second)->GetNextContentToForward()) != NULL)
			{
				// Sending the content
				SendContent(sender, next);

				// Update the number of sent bytes
				(iter->second)->sentBytes += static_cast<int>(next->GetDataSize());
				(iter->second)->UpdateInterestSentBytes(next->GetMatchingInterest(), next->GetDataSize());

				// Change the status of the Content to sent
				(iter->second)->SetContentForwarded(next);
				next = NULL;
			}

			// All contents are forwarder, cleaning the session
			// Nothing more to send, ending the current session
			SendEndSession(sender);
			//CleanSession(sender);

		}else
		{
			NS_FATAL_ERROR("MOBITRADEProtocol::ForwardAllMatchingContents Trying to forward contents to an invalid device entry.");
		}
	}

	sem_post(&sessionsSem);

}

void MOBITRADEProtocol::ForwardFirstContents(Ipv4Address & sender)
{
	// Forwarding the matching Contents for generosity
	CleanOutOfDateContents();

	sem_wait(&sessionsSem);

	map<Ipv4Address, StatisticsEntry*>::iterator iter = exchangedBytes.find(sender);
	std::map<Interest *, int> & recvInterests = (iter->second)->mapReceivedInterests;

	(iter->second)->firstContentsForwarded = true;

	// Looking for matching Contents
	if(mapContents.size() > 0 || generatedContents.size() > 0)
	{

		int i =0 ;
		for(map<Interest*, int>::iterator iter2 = recvInterests.begin(); iter2 != recvInterests.end(); iter2++, i++)
		{
			AddContentsToForward(sender, iter2->first, i);
		}

		if(iter != exchangedBytes.end())
		{
			int nbr = GENEROSITY;

			while(nbr > 0)
			{
				Content * next = (iter->second)->GetNextContentToForward();
				if(next != NULL)
				{
#ifdef DEBUG
					NS_ASSERT(next->GetMatchingInterest() != NULL);
#endif
					// Sending the content
					SendContent(sender, next);

					// Update the number of sent bytes
					(iter->second)->sentBytes += static_cast<int>(next->GetDataSize());
					(iter->second)->UpdateInterestSentBytes(next->GetMatchingInterest(), next->GetDataSize());

					// Change the status of the Content to sent
					(iter->second)->SetContentForwarded(next);
					next = NULL;
					nbr--;
				}else
				{
					break;
				}
			}

		}else
		{
			NS_FATAL_ERROR("MOBITRADEProtocol::ForwardFirstContents Trying to forward contents to an invalid device entry.");
		}
	}
	sem_post(&sessionsSem);

}

void MOBITRADEProtocol::AddContentsToForward(Ipv4Address & sender, Interest * i, int index)
{
	map<Ipv4Address, StatisticsEntry*>::iterator iter = exchangedBytes.find(sender);

	if(iter != exchangedBytes.end())
	{
		for(map<std::string, Content *>::iterator iter2 = mapContents.begin(); iter2 != mapContents.end(); iter2++)
		{
			int numberOfMatchs = 0;
#ifdef DEBUG
			NS_ASSERT(iter2->second != NULL);
#endif
			if(i->Matches((iter2->second)->GetDescription(), numberOfMatchs))
			{
				i->SetSatisfied();
				(iter2->second)->SetUtility(i->GetUtility());
				(iter->second)->AddContentToForward(iter2->second, i);
			}
		}

		for(map<std::string, Content *>::iterator iter2 = generatedContents.begin(); iter2 != generatedContents.end(); iter2++)
		{
			int numberOfMatchs = 0;
			if(i->Matches((iter2->second)->GetDescription(), numberOfMatchs))
			{
				i->SetSatisfied();
				(iter2->second)->SetUtility(i->GetUtility());
				(iter->second)->AddContentToForward(iter2->second, i);
			}
		}

		if(((iter->second)->numberMatchingContents > CONTENTS_STORAGE_CAPACITY + static_cast<int>(generatedContents.size())) && !IsMalicious())
		{
			cout<<"foreign contents: "<<mapContents.size()<<endl;
			cout<<"generated contents: "<<generatedContents.size()<<endl;

			cout<<" Recv Interest: "<<i->GetDescription()<<" Number of matching contents: "<<(iter->second)->numberMatchingContents<<endl;
			ShowContents(cout);
			(iter->second)->ShowTheListOfContentsToForward();
			NS_FATAL_ERROR("Invalid List of Contents");
		}
	}else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::AddContentsToForward Invalid device statistics entry.");
	}
}

bool MOBITRADEProtocol::MatchesLocalContent(Interest *i)
{
	for(map<std::string, Content *>::iterator iter2 = mapContents.begin(); iter2 != mapContents.end(); iter2++)
	{
		int numberOfMatchs = 0;
		if(i->Matches((iter2->second)->GetDescription(), numberOfMatchs))
		{
			i->SetSatisfied();
			(iter2->second)->SetUtility(i->GetUtility());
			return true;
		}
	}

	return false;
}

bool MOBITRADEProtocol::MatchesLocalInterest(Content *c, double * del, bool& mFroeign)
{
	bool foreign = false;
	bool exist = false;

	map<std::string, Interest *>::iterator iter = mapInterests.begin();
	for(; iter != mapInterests.end(); iter++)
	{
		int numberMatches;
		if((iter->second)->Matches(c->GetDescription(), numberMatches))
		{
			exist = true;
			foreign = (iter->second)->IsForeign();
			if(!foreign)
			{
				// Add the delivery delay record
				double deliveryDelay = DEFAULT_CONTENT_TTL - c->GetTTL();

				// Tracking bugs
				if(deliveryDelay > DEFAULT_CONTENT_TTL)
				{
					cout<<"Node: "<<nodeId<<" Content: "<<c->GetDescription()<<" TTL: "<<c->GetTTL()<<endl;
					NS_FATAL_ERROR("deliveryDelay > DEFAULT_CONTENT_TTL.");
				}

				if(deliveryDelay < 0)
				{
					cout<<"Node: "<<nodeId<<" Content: "<<c->GetDescription()<<" TTL: "<<c->GetTTL()<<endl;
					NS_FATAL_ERROR("deliveryDelay < 0");
				}

				*del = deliveryDelay;
			}else
			{
				// A content that match a foreign Interest stored locally
			}

			// Establishing the link with the matching Interest
			SyncInterestToContent(iter->second, c);

			break;
		}
	}

 	/*if(iter != mapInterests.end())
	{
		delete iter->second;
		mapInterests.erase(iter);
	}*/

	mFroeign = exist;
	return (!foreign && exist);
}


void MOBITRADEProtocol::SyncInterestToAvailableContent(Interest * i)
{
	for(std::map<std::string, Content *>::iterator iter = mapContents.begin(); iter != mapContents.end(); iter++)
	{
		int numberMatches = 0;
		if(i->Matches(iter->first, numberMatches))
		{
			// Establishing the link with the matching Interest
			SyncInterestToContent(i, iter->second);
		}
	}

	for(std::map<std::string, Content *>::iterator iter = generatedContents.begin(); iter != generatedContents.end(); iter++)
	{
		int numberMatches = 0;
		if(i->Matches(iter->first, numberMatches))
		{
			// Establishing the link with the matching Interest
			SyncInterestToContent(i, iter->second);
		}
	}

}

void MOBITRADEProtocol::SyncContentToAvailableInterests(Content *c)
{
	for(map<string, Interest *>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
	{
		int numberMatches = 0;
		if(iter->second->Matches(c->GetDescription(), numberMatches))
		{
			// Establishing the link with the matching Interest
			SyncInterestToContent(iter->second, c);
			// We suppose there is only one matching interest
			break;
		}
	}
}

void MOBITRADEProtocol::SyncInterestToContent(Interest * i, Content *c)
{
#ifdef DEBUG
	NS_ASSERT(i != NULL);
	NS_ASSERT(c != NULL);
#endif
	i->AddNewMatchingContent(c);
	c->SetMatchingInterest(i);
}


void MOBITRADEProtocol::addNewInterests(Ipv4Address &sender)
{
	CleanOutOfDateInterests();

	map<Ipv4Address, StatisticsEntry *>::iterator stat = exchangedBytes.find(sender);
	map<Interest *, int> & receivedInterests = stat->second->mapReceivedInterests;
	//(stat->second)->ShowListReceivedInterests();

	if(!receivedInterests.empty())
	{
		if(ACTIVATE_MOBITRADE_COLLABORATION && !IsMalicious())
		{

			// Start by Updating the utilities of the local foreign Interests
			UpdateLocalInterestsUtilities(sender, receivedInterests);

			// Update the utilities of the non existent new received interests and add them to the local store

			for(map<Interest *, int>::iterator iter = receivedInterests.begin(); iter != receivedInterests.end(); iter++)
			{

				if(!iter->first->IsLocal())
				{
					double nu = 0;
					unsigned int sentBytes = iter->second;
					// Updating the channel utility
					if(sentBytes < BETA)
					{
						nu = (1 - 0.01)*max(ALPHA, 2*sentBytes);

					}else
					{
						nu = sentBytes + ALPHA;
					}

#ifdef DEBUG
					NS_ASSERT(nu != 0);
#endif
					(iter->first)->SetUtility(nu);
					// Adding the interest to the local store
					AddNewInterest(iter->first);
				}else
				{
					// We don't store Interests that match local contents
					iter->first->SyncWithMatchingContents();
					delete iter->first;
				}
			}

			//cout<<"Number of received interests: "<<receivedInterests.size()<<" number of accepted: "<<numberOfAccepted+1<<endl;

			// Each time some new Interests are added, we update the Whole Interest storage share
			UpdateInterestsAllowedBytes();
		}
		receivedInterests.clear();
	}

}

void MOBITRADEProtocol::UpdateLocalInterestsUtilities(Ipv4Address &sender, std::map<Interest*, int> & receivedInterests)
{
	for(map<string, Interest*>::iterator iter = mapInterests.begin(); iter != mapInterests.end();iter++)
	{
		bool found = false;
		map<Interest *, int>::iterator iter2 = receivedInterests.begin();

		for(; iter2 != receivedInterests.end(); iter2++)
		{
#ifdef DEBUG
			NS_ASSERT(iter2->first != NULL);
#endif
			if((iter->second)->IsEqual(iter2->first))
			{
				found = true;
				break;
			}
		}
		// If one of the received Interests Matches a local foreign Interests then, Update its Utility
		// And remove it from the list
		if(found)
		{
			// The exchanged bytes in the context of that interest
			unsigned int sentBytes = iter2->second;
			//cout<<"Node: "<<nodeId<<" with "<<sender<<" Updating local utility, sent bytes: "<<sentBytes<<" int: "<<iter2->first->GetDescription()<<endl;

			// Remove the foreign Interest from the received list so, it will not be considered
			// as a new received one
			(iter->second)->SetLocal(iter2->first->IsLocal());
			delete (iter2->first);
			receivedInterests.erase(iter2);

			map<Ipv4Address, StatisticsEntry* >::iterator stat = exchangedBytes.find(sender);
#ifdef DEBUG
			NS_ASSERT(stat != exchangedBytes.end());
#endif
			double lu = (iter->second)->GetUtility();
			double nu = 0 ;

			if(sentBytes < BETA)
			{
				nu = lu*(0.01) + (1-0.01)*max(ALPHA, 2*sentBytes);

			}else
			{
				nu = lu*(0.01) + (1-0.01)*(ALPHA + sentBytes);
			}

			(iter->second)->SetUtility(nu);

		}else
		{
			double lu = (iter->second)->GetUtility();
			(iter->second)->SetUtility(lu*0.01);

		}
	}
}

void MOBITRADEProtocol::UpdateInterestsAllowedBytes()
{
	DispatchAvailableStorage();

	// Calculating the sum of utilities
	double sumFU = 0;
	double sumLU = 0;
	double totalU = 0;
	for(map<string, Interest*>::iterator iter = mapInterests.begin(); iter != mapInterests.end();iter++)
	{
		if((iter->second)->IsForeign())
			sumFU +=	(iter->second)->GetUtility();
		else
			sumLU +=	(iter->second)->GetUtility();
	}
	totalU = sumFU + sumLU;


	// Updating each interest allowed size of bytes
	for(map<string, Interest*>::iterator iter = mapInterests.begin(); iter != mapInterests.end();iter++)
	{
		double res = ((iter->second)->GetUtility()/totalU)*CONTENTS_STORAGE_CAPACITY*CONTENT_PACKET_SIZE;

		(iter->second)->SetInterestAllowedStorage(static_cast<unsigned int>(floor(res)));

		if(floor((iter->second)->GetInterestAllowedBytes() - (iter->second)->GetUtility()) > 0)
		{
			cout<<"Foreign: "<<(iter->second)->IsForeign()<<" Utility : "<<(iter->second)->GetUtility()<<" Allowed bytes: "<<(iter->second)->GetInterestAllowedBytes()<<endl;
			ShowInterests(cout);
			NS_FATAL_ERROR("Invalid allowed storage.");
		}
	}

	CleanUpTheStorage();
}

void MOBITRADEProtocol::InitExchangedBytes(Ipv4Address  adr)
{
	map<Ipv4Address, StatisticsEntry* >::iterator iter = exchangedBytes.find(adr);
	if(iter == exchangedBytes.end())
	{
		StatisticsEntry * se = new StatisticsEntry(this);
		se->recvBytes = 0;
		se->sentBytes = 0;
		se->mapReceivedInterests.clear();
		exchangedBytes[adr] = se;
	}else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::InitExchangedBytes Entry already available.");
	}
}

bool MOBITRADEProtocol::ContinueSending(Ipv4Address & adr)
{
	map<Ipv4Address, StatisticsEntry* >::iterator iter = exchangedBytes.find(adr);
	if(iter != exchangedBytes.end())
	{
		return true;
	}else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::GetExchangedBytes statistics Entry not found.");
		return false;
	}
}

void MOBITRADEProtocol::UpdateSentBytes(Ipv4Address & adr, int nbrBytes)
{
	map<Ipv4Address, StatisticsEntry* >::iterator iter = exchangedBytes.find(adr);
	if(iter != exchangedBytes.end())
	{
		(iter->second)->sentBytes += nbrBytes;
	}else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::GetExchangedBytes statistics Entry not found.");
	}
}

void MOBITRADEProtocol::UpdateRecvBytes(Ipv4Address & adr, int nbrBytes)
{
	map<Ipv4Address, StatisticsEntry* >::iterator iter = exchangedBytes.find(adr);
	if(iter != exchangedBytes.end())
	{
		(iter->second)->recvBytes += nbrBytes;
	}else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::GetExchangedBytes statistics Entry not found.");
	}
}

int MOBITRADEProtocol::GetSizeOfReceivedContents(Ipv4Address & adr)
{
	map<Ipv4Address, StatisticsEntry*>::iterator iter = exchangedBytes.find(adr);
	if(iter != exchangedBytes.end())
	{
		return (iter->second)->recvBytes;
	}
	else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::GetSizeOfReceivedContents remote peer statistics entry not found.");
	}
	return 0;
}

void MOBITRADEProtocol::SetNumberOfMatchingContents(Ipv4Address & adr, int nb)
{
	map<Ipv4Address, StatisticsEntry*>::iterator iter = exchangedBytes.find(adr);
	if(iter != exchangedBytes.end())
	{
		 (iter->second)->numberMatchingContents = nb;
	}
	else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::GetSizeOfReceivedContents remote peer statistics entry not found.");
	}

}

int MOBITRADEProtocol::GetNumberOfMatchingContents(Ipv4Address & adr)
{
	map<Ipv4Address, StatisticsEntry*>::iterator iter = exchangedBytes.find(adr);
	if(iter != exchangedBytes.end())
	{
		 return (iter->second)->numberMatchingContents;
	}
	else
	{
		NS_FATAL_ERROR("MOBITRADEProtocol::GetSizeOfReceivedContents remote peer statistics entry not found.");
	}
	return 0;
}


// Description Utility TTL
void MOBITRADEProtocol::GenerateLocalInterest(int from, int to)
{
#ifdef DEBUG
	NS_ASSERT(from <= to);
#endif
	for(int i = from; i <= to; i++)
	{
		char description[512];
		sprintf(description, "Content%i-", i);

		double ttl = DEFAULT_INTEREST_TTL;

		double utility = INITIAL_LOCAL_INTEREST_UTILITY;

		Interest * i = new Interest(string(description), ttl, utility, this);
		i->SetForeign(false);
		AddNewInterest(i);
		AddNewRequestedContent();
		i = NULL;

	}
}

void MOBITRADEProtocol::GenerateLocalInterest(int index, int details, bool withDetails)
{

#ifdef ENABLE_RANDOM_CHANNELS_SIZE
	AddNewInterestId(index);
#endif

	if(UPDATE_INTERESTS_DETAILS)
	{
		// We keep track of that only when we want to
		// update later on the details of the list of generated Interests
		AddNewInterestId(index);
	}

	char description[512];
	if(!withDetails)
	{
		sprintf(description, "Content%i-", index);
	}
	else
	{
		sprintf(description, "Content%i-Details%i", index, details);

	}

	double ttl = DEFAULT_INTEREST_TTL;

	double utility = INITIAL_LOCAL_INTEREST_UTILITY;

	Interest * i = new Interest(string(description), ttl, utility, this);
	i->SetForeign(false);
	AddNewInterest(i);
	AddNewRequestedContent();
	i = NULL;
}

// Description Data sourceID TTL
void MOBITRADEProtocol::GenerateLocalContent(int from, int to)
{
#ifdef DEBUG
	NS_ASSERT(from <= to);
#endif
	for(int i = from; i <= to; i++)
	{
		char description[512];
		sprintf(description, "Content%i", i);

		char data[512];
		sprintf(data, "Data%i", i);

		string sourceId;
		sourceId.assign("Unknown");


		Content * c = new Content(description, data, DEFAULT_CONTENT_TTL, this);
		c->SetSourceId(sourceId);
		AddNewContent(c);
		AddNewGeneratedContent();
		c = NULL;
	}
}

void MOBITRADEProtocol::GenerateLocalContent(int index, int details, bool withDetails)
{
	char description[512];
	if(!withDetails)
	{
		sprintf(description, "Content%i", index);
	}
	else
	{
		sprintf(description, "Content%i-Details%i", index, details);

	}

	char data[512];
	sprintf(data, "Data%i", index);

	string sourceId;
	sourceId.assign("Unknown");


	Content * c = new Content(description, data, DEFAULT_CONTENT_TTL, this);
	c->SetSourceId(sourceId);
	c->SetUtility(INITIAL_LOCAL_INTEREST_UTILITY);
	AddNewContent(c);
	AddNewGeneratedContent();
	c = NULL;

}

void MOBITRADEProtocol::ShowContents(ostream & strm)
{


	strm<<Simulator::Now().GetSeconds()<<" "<<"Node: "<<nodeId<<" list contents: "<<endl;
	for(map<std::string, Content *>::iterator iter = mapContents.begin();iter != mapContents.end(); iter++)
	{
		strm<<(iter->second)->GetResume()<<" foreign: "<<iter->second->IsForeign()<<endl;
	}

	for(map<std::string, Content *>::iterator iter = generatedContents.begin();iter != generatedContents.end(); iter++)
	{
		strm<<(iter->second)->GetResume()<<" foreign: "<<iter->second->IsForeign()<<endl;
	}

	strm<<endl;

}

void MOBITRADEProtocol::ShowInterestsVsContents(ostream & strm)
{
	strm<<endl<<"NodeId: "<<nodeId<<" Number of stored Interests: "<<mapInterests.size()<<endl;
	for(map<std::string, Interest *>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
	{
		strm<<"Interest: "<<(iter->second)->GetDescription()<<" Foreign: "<<(iter->second)->IsForeign()<<" Utility: "<<fixed<<(iter->second)->GetUtility()<<" Allowed Storage: "<<(iter->second)->GetInterestAllowedBytes()<<" Occupied space: "<<(iter->second)->GetTotalSizeOfMatchingContents()<<endl;
		//(iter->second)->ShowMatchingContents(strm);
	}
}


void MOBITRADEProtocol::ShowNodesNumberMeetings(ostream & strm)
{
	strm<<Simulator::Now().GetSeconds()<<" "<<"Node: "<<nodeId<<" Number of met nodes: "<<nodesNumberMeetings.size()<<" nodes meetings: "<<endl;
	for(map<Ipv4Address, int >::iterator iter = nodesNumberMeetings.begin();iter != nodesNumberMeetings.end(); iter++)
	{
		strm<<'\t'<<iter->first<<" "<<iter->second<<endl;
	}

}

void MOBITRADEProtocol::ShowNodesCollaboration(ostream & strm)
{
	strm<<Simulator::Now().GetSeconds()<<" "<<"Node: "<<nodeId<<" Number of met nodes: "<<nodesCollaboration.size()<<" nodes collaboration: "<<endl;
	for(map<Ipv4Address, list<CollXY> >::iterator iter = nodesCollaboration.begin();iter != nodesCollaboration.end(); iter++)
	{
		strm<<iter->first<<" : ";
		for(list<CollXY>::iterator iter2 = iter->second.begin(); iter2 != iter->second.end();iter2++)
		{
			strm<<" "<<(*iter2).coll<<" "<<(*iter2).time<<endl;
		}
		strm<<" nbr meetings: "<<nodesNumberMeetings[iter->first]<<endl;

	}
	strm<<endl;
}

void MOBITRADEProtocol::ShowDeliveredContents(ostream & strm)
{
	strm<<Simulator::Now().GetSeconds()<<" "<<"Node: "<<nodeId<<" list of delivered contents: "<<endl;
	for(map<std::string, double>::iterator iter = deliveredContents.begin();iter != deliveredContents.end(); iter++)
	{
		strm<<iter->first<<endl;
	}
	strm<<endl;
}

void MOBITRADEProtocol::ShowDeliveredContentsPerChannel(ostream & strm)
{
	strm<<Simulator::Now().GetSeconds()<<" "<<"Node: "<<nodeId<<" list of delivered contents per channel: "<<endl;
	for(map<std::string, int>::iterator iter = deliveredPerChannel.begin();iter != deliveredPerChannel.end(); iter++)
	{
#ifndef ENABLE_RANDOM_CHANNELS_SIZE
			strm<<iter->first<<" "<<iter->second<<endl;
#else
			strm<<iter->first<<" "<<iter->second<<" "<<GetChannelSizeId(iter->first)<<endl;
#endif
	}
	strm<<endl;
}

void MOBITRADEProtocol::ShowInterests(ostream & strm)
{
	CleanOutOfDateInterests();

	sem_wait(&interestsSem);

	strm<<Simulator::Now().GetSeconds()<<" "<<"Node: "<<nodeId<<" list interests: "<<endl;
	for(map<std::string, Interest *>::iterator iter = mapInterests.begin();iter != mapInterests.end(); iter++)
	{
		if((iter->second)->IsForeign())
			strm<<"foreign: "<<(iter->second)->GetResume()<<endl;
		else
			strm<<"local: "<<(iter->second)->GetResume()<<endl;
	}
	strm<<endl;

	sem_post(&interestsSem);
}

void MOBITRADEProtocol::LogStatistics()
{
	ofstream file1, file2, file3, file4,file5, file6, file7, file8;

	file1.open("MobiTradelogCollaboration.txt", ofstream::app);
	file2.open("MobiTradelogDeliveryRate.txt", ofstream::app);
	file3.open("MobiTradelogNbrMeetings.txt", ofstream::app);
	file4.open("MobiTradelogDRMalicious.txt", ofstream::app);
	file5.open("MobiTradelogInterests.txt", ofstream::app);
	file6.open("MobiTradelogContents.txt", ofstream::app);
	file7.open("MobiTradelogInterestsVsContents.txt", ofstream::app);
	file8.open("MobiTradeDelPerChannel.txt", ofstream::app);

	if(!IsMalicious())
	{
		#ifdef ENABLE_RANDOM_CHANNELS_SIZE
		file2 <<"NCONTENTS_STORAGE_CAPACITY: "<<CONTENTS_STORAGE_CAPACITY <<" "<<Simulator::Now().GetSeconds()<<" Node: "<<nodeId<<" Requested: "<<GetSizeOfRequestedInterests()<<" Delivered: "<<GetNumberDeliveredContents()<<" Average Delay: "<<GetAverageDeliveryDelay()<<endl;
		#else
		file2 <<"WTFTCONTENTS_STORAGE_CAPACITY2: "<<CONTENTS_STORAGE_CAPACITY <<" "<<Simulator::Now().GetSeconds()<<" Node: "<<nodeId<<" Requested: "<<numberRequestedContents<<" Delivered: "<<GetNumberDeliveredContents()<<" Average Delay: "<<GetAverageDeliveryDelay()<<endl;
		#endif
	}
	else
	{
		#ifdef ENABLE_RANDOM_CHANNELS_SIZE
		file4 <<"NCONTENTS_STORAGE_CAPACITY: "<<CONTENTS_STORAGE_CAPACITY <<" "<<Simulator::Now().GetSeconds()<<" Node: "<<nodeId<<" Requested: "<<GetSizeOfRequestedInterests()<<" Delivered: "<<GetNumberDeliveredContents()<<" Average Delay: "<<GetAverageDeliveryDelay()<<endl;
		#else
		file4 <<"FCONTENTS_STORAGE_CAPACITY: "<<CONTENTS_STORAGE_CAPACITY <<" "<<Simulator::Now().GetSeconds()<<" Malicious Node: "<<nodeId<<" Requested: "<<numberRequestedContents<<" Delivered: "<<GetNumberDeliveredContents()<<" Average Delay: "<<GetAverageDeliveryDelay()<<endl;
		#endif
	}

	//ShowInterests(file5);
	//ShowNodesCollaboration(file1);
	//ShowNodesNumberMeetings(file3);
	//ShowContents(file6);
	ShowInterestsVsContents(file7);
	ShowDeliveredContentsPerChannel(file8);
	//ShowInterests(file);
	//ShowDeliveredContents(file);

	file1.close();
	file2.close();
	file3.close();
	file4.close();
	file5.close();
	file6.close();
	file7.close();
	file8.close();
}


double MOBITRADEProtocol::GetAverageDeliveryDelay()
{
	if(deliveredContents.empty())
		return 0;
	double total = 0;

	for(map<string, double>::iterator iter = deliveredContents.begin(); iter != deliveredContents.end(); iter++)
	{
		total += iter->second;
	}

	double avgDelay = total/deliveredContents.size();
#ifdef DEBUG
	NS_ASSERT(avgDelay < DEFAULT_CONTENT_TTL);
#endif
	return avgDelay;
}



void MOBITRADEProtocol::ShowListSession(std::ostream & strm)
{
	  for(map<Ipv4Address, int >::iterator iter = currentActiveSessions.begin(); iter !=currentActiveSessions.end();iter++)
	  {
		  strm<<"Session: "<<iter->first<<" State: "<<iter->second<<endl;
	  }
}


void MOBITRADEProtocol::AddNewInterestId(int i)
{
	list<int>::iterator iter = generatedInterestsIds.begin();
	for(; iter != generatedInterestsIds.end(); iter++)
	{
		if(*iter == i)
			break;
	}

	if(iter == generatedInterestsIds.end())
	{
		generatedInterestsIds.push_back(i);
	}
}

double MOBITRADEProtocol::GetSumOfInterestsUtilities()
{
	double sum = 0;
	for(map<std::string, Interest *>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
	{
		sum += (iter->second)->GetUtility();
	}
	return sum;
}


void MOBITRADEProtocol::GenerateChannelsWithRandomSizes(int nbrChannels)
{
	if(sizeChannels.size() == 0)
	{
#ifdef ENABLE_FIXED_SIZES
		for(int i = 1; i <= nbrChannels; i++)
		{
//Choosing a fixed channel size
			if((i >= 31 && i <= 49))
			{
				sizeChannels[i] = 100;
			}else
			{
				sizeChannels[i] = 5;
			}
			char id[200];
			sprintf(id,"Content%i-", i);
			cout<<"Channel: "<<id<<" size: "<<sizeChannels[i]<<endl;
			sizeChannels2[string(id)] = sizeChannels[i];
		}
#else
		for(int i = 1; i <= nbrChannels; i++)
		{
#ifdef DEBUG
			NS_ASSERT(MAX_SIZE_OF_CHANNEL > 10);
#endif
			sizeChannels[i] = static_cast<int>(floor(UniformVariable(1, MAX_SIZE_OF_CHANNEL).GetValue()));
			char id[200];
			sprintf(id,"Content%i-", i);
			cout<<"Channel: "<<id<<" size: "<<sizeChannels[i]<<endl;
			sizeChannels2[string(id)] = sizeChannels[i];
		}
#endif
	}
}

int MOBITRADEProtocol::GetChannelSize(int channelId)
{
	  std::map<int, int>::iterator iter = sizeChannels.find(channelId);
	  if(iter == sizeChannels.end())
	  {
		  cout<<"Size request for : "<<channelId<<endl;
		  cout<<"generated channels: "<<endl;
		  for(map<int, int>::iterator iter = sizeChannels.begin(); iter != sizeChannels.end();iter++)
		  {
			  cout<<"Ch: "<<iter->first<<" size: "<<iter->second<<endl;
		  }
		  NS_FATAL_ERROR("Invalid Channel");
	  }
	  return iter->second;
}

int MOBITRADEProtocol::GetChannelSizeId(std::string channelId)
{
	  std::map<string, int>::iterator iter = sizeChannels2.find(channelId);
#ifdef DEBUG
	  NS_ASSERT(iter != sizeChannels2.end());
#endif
	  return iter->second;
}

int MOBITRADEProtocol::GetSizeOfRequestedInterests()
{
	int totalExpectedContents = 0;
	for(list<int>::iterator iter = generatedInterestsIds.begin(); iter != generatedInterestsIds.end(); iter++)
	{
		totalExpectedContents += GetChannelSize(*iter);
	}

	return totalExpectedContents;
}


void MOBITRADEProtocol::SortAvailableInterests()
{
	sortedInterests.clear();
	for(map<string, Interest * >::iterator iter = mapInterests.begin();iter != mapInterests.end(); iter++)
	{
		sortedInterests.push_back(iter->second);
	}

	// Sort the vector
	for(unsigned int i = 0; i < sortedInterests.size(); i++)
	{
		unsigned int min = i;
		for(unsigned int j = min + 1; j < sortedInterests.size(); j++)
		{
			if(sortedInterests[j]->GetUtility() < sortedInterests[min]->GetUtility())
			{
				min = j;
			}
		}

		if(min != i)
		{
			// swap
			Interest * tmp = sortedInterests[i];
			sortedInterests[i] = sortedInterests[min];
			sortedInterests[min] = tmp;
			tmp = NULL;
		}
	}

	/*
	ShowInterests(cout);
	cout<<" Sorted list: "<<endl;
	for(unsigned int i = 0; i < sortedInterests.size(); i++)
	{
		cout<<"Interest: "<<sortedInterests[i]->GetDescription()<<" util: "<<sortedInterests[i]->GetUtility()<<endl;
	}

	NS_ASSERT(sortedInterests.size() == mapInterests.size());
*/
#ifdef DEBUG
	NS_ASSERT(!sortedInterests.empty());
#endif
}


Interest * MOBITRADEProtocol::GetInterestWithSmallestUtilityThatExceedsItsShare()
{
	SortAvailableInterests();
	for(vector<Interest*>::iterator iter = sortedInterests.begin(); iter != sortedInterests.end(); iter++)
	{
#ifdef DEBUG
		NS_ASSERT(!(*iter)->IsLocal());
#endif
		if((*iter)->ExeedsAllowedStorage() )
			return *iter;
	}

	return NULL;
}

Interest * MOBITRADEProtocol::GetInterestThatExceedsTheMostItsShare()
{
	Interest * selectedI = NULL;
	unsigned int shareExceed = 0;
	for(map<string, Interest *>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
	{
		unsigned int tmpExceed = 0;
		NS_ASSERT(!(iter->second)->IsLocal());

		if((iter->second)->GetTotalSizeOfMatchingContents() > (iter->second)->GetInterestAllowedBytes())
		{
			tmpExceed = static_cast<unsigned int>(ceil((iter->second)->GetTotalSizeOfMatchingContents() - (iter->second)->GetInterestAllowedBytes()));
			if(tmpExceed > shareExceed)
			{
				selectedI = iter->second;
				shareExceed = tmpExceed;
			}
		}
	}

	return selectedI;
}

Interest * MOBITRADEProtocol::GetInterestWithSmallestUtilityThatExceedsItsAllowedStorage()
{
	SortAvailableInterests();
	for(vector<Interest*>::iterator iter = sortedInterests.begin(); iter != sortedInterests.end(); iter++)
	{
		NS_ASSERT(!(*iter)->IsLocal());
		if((*iter)->ExeedsAllowedStorage())
			return *iter;
	}

	return NULL;
}


Interest * MOBITRADEProtocol::GetInterestWithSmallestUtilityThatExceedsItsUtility()
{
	SortAvailableInterests();
	for(vector<Interest*>::iterator iter = sortedInterests.begin(); iter != sortedInterests.end(); iter++)
	{
		NS_ASSERT(!(*iter)->IsLocal());
		if((*iter)->ExceedsAllowedUtilityBytes())
			return *iter;
	}

	return NULL;
}

unsigned int MOBITRADEProtocol::GetSumInterestsUtilities()
{
	unsigned int sum = 0;
	for(std::map<std::string, Interest *>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
	{
		sum += static_cast<unsigned int>(iter->second->GetUtility());
	}
	return sum;
}

unsigned int MOBITRADEProtocol::GetMaxUtility()
{
	unsigned int max = 0;
	for(std::map<std::string, Interest *>::iterator iter = mapInterests.begin(); iter != mapInterests.end(); iter++)
	{
		if(iter->second->GetUtility() > max)
			max += iter->second->GetUtility();
	}
	return max;

}
// ------------------------------------------------------------------------------------------------------------------------------------------------------
// 													Interest Implementation
// ------------------------------------------------------------------------------------------------------------------------------------------------------

// Description Utility TTL
Interest::Interest(string resume, bool isForeignInterest, MOBITRADEProtocol * c)
{
#ifdef DEBUG
	NS_ASSERT(!resume.empty());
#endif
	maxNumberOfAllowedBytes = 0;
	_control = c;
	satisfied = false;
	description = MOBITRADEProtocol::GetInterestDescription(resume);
	utility = MOBITRADEProtocol::GetInterestUtility(resume);
	ttl = MOBITRADEProtocol::GetInterestTTL(resume);

	foreignInterest = isForeignInterest;
	recvTime = Simulator::Now().GetSeconds();
	local = false;
}

Interest::Interest(std::string  description, double ttl, double utility, MOBITRADEProtocol *c)
{
	maxNumberOfAllowedBytes = 0;
	_control = c;
	satisfied = false;
	this->description = description;
	this->utility = utility;
	this->ttl = ttl;
	this->recvTime = Simulator::Now().GetSeconds();
	local = false;
}

Interest::~Interest()
{
	matchingContents.clear();
	_control = NULL;
}

bool Interest::UpdateTTL()
{
	ttl = floor(ttl - (Simulator::Now().GetSeconds() - recvTime));
	recvTime = Simulator::Now().GetSeconds();
	if(ttl < 0)
		ttl = 0;
	return (ttl == 0);
}

double Interest::GetElapsedTimeSinceReceived()
{
	return Simulator::Now().GetSeconds() - recvTime;
}

bool Interest::Matches(std::string desc, int & numberOfMatches)
{
    // Trying to see whether there is some common keywords between desc and the Interest description and returning the number of matches
	// We suppose that a description is a set of keywords separated by spaces.
	size_t pos = desc.find(this->description);
	if(pos != string::npos)
	{
		numberOfMatches = 1;
		//cout<<" content: "<<desc<<" matches: "<<GetDescription()<<" Pos: "<<pos<<endl;
		return true;
	}
	return false;

}

// Description Utility TTL
string Interest::GetResume()
{
	// Return the Interest resume which will be sent to the remote peer
	// Append the Interest description
	string tmp;
	tmp.append(this->description);


	// Append the Interest utility
	stringstream ss;
	if(IsForeign())
	{
		ss << this->utility;
	}else
	{
		ss << 0.01*ALPHA;
	}

	tmp.append("#");
	tmp.append(ss.str());

	tmp.append("#");
	// Append the Interest TTL
	UpdateTTL();

	if(ttl < 0)
	{
		cout<<"Invalid Interest TTL: "<<ttl<<endl;
		NS_FATAL_ERROR("INVALID Interest TTL");
	}

	stringstream ss2;
	ss2 << this->ttl;
	tmp.append(ss2.str());
	return tmp;
}

bool Interest::IsEqual(Interest * i)
{
	return (i->GetDescription().compare(this->description) == 0);
}

bool Interest::IsOutOfDate()
{
	return Simulator::Now().GetSeconds() >= ttl;
}

// Used to manage the amount of storage used by the different Interests

void Interest::AddNewMatchingContent(Content * c)
{
	if(matchingContents.empty())
	{
		matchingContents[c->GetDescription()] = c;
	}else
	{
		std::map<std::string, Content *>::iterator iter = matchingContents.find(c->GetDescription());
		if(iter == matchingContents.end())
		{
			matchingContents[c->GetDescription()] = c;
		}else
		{
			NS_FATAL_ERROR("Matching Content Already here");
		}
	}
}

void Interest::RemoveMatchingContent(Content *c)
{
	std::map<std::string, Content *>::iterator iter = matchingContents.find(c->GetDescription());
	if(iter != matchingContents.end())
	{
		iter->second = NULL;
		matchingContents.erase(iter);
	}

}

int Interest::GetNumberOfMatchingContents()
{
	return matchingContents.size();
}

void Interest::SyncWithMatchingContents(Content *c)
{
	std::map<std::string, Content *>::iterator iter = matchingContents.find(c->GetDescription());
	if(iter != matchingContents.end())
	{
		matchingContents.erase(iter);
	}else
	{
		NS_FATAL_ERROR("Invalid list of matching contents SyncWithMatchingContents");
	}
}

void Interest::SyncWithMatchingContents()
{
	for(std::map<std::string, Content *>::iterator iter = matchingContents.begin(); iter != matchingContents.end(); iter++)
	{
		(iter->second)->SetMatchingInterest(NULL);
	}
	matchingContents.clear();
}

void Interest::ShowMatchingContents(ostream& strm)
{
	strm<<"Matching Contents to Interest: "<<GetDescription()<<" is foreign: "<<foreignInterest<<endl;
	if(matchingContents.empty())
	{
		strm<<"No matching Contents in the local buffer"<<endl;
	}else
	{
		for(std::map<std::string, Content *>::iterator iter = matchingContents.begin(); iter != matchingContents.end(); iter++)
		{
			strm<<'\t'<<" Content: "<<iter->first<<" Utility: "<<(iter->second)->GetUtility()<<" foreign: "<<iter->second->IsForeign()<<endl;
		}
	}
}

void Interest::UpdateUtilityOfMatchingContents()
{
	for(map<std::string, Content *>::iterator iter = matchingContents.begin(); iter != matchingContents.end(); iter++)
	{
#ifdef DEBUG
		NS_ASSERT(iter->second != NULL);
#endif
		(iter->second)->SetUtility(GetUtility());
	}
}

void Interest::GetMatchingContents(std::list<Content *> match)
{
	for(map<std::string, Content *>::iterator iter = matchingContents.begin(); iter != matchingContents.end(); iter++)
	{
		match.push_back(iter->second);
	}
}

unsigned int Interest::GetTotalSizeOfMatchingContents()
{
	unsigned int totalBytes = 0;
	for(map<std::string, Content *>::iterator iter = matchingContents.begin(); iter != matchingContents.end(); iter++)
	{
		totalBytes += static_cast<unsigned int>((iter->second)->GetDataSize());
	}
	return totalBytes;
}



Content *Interest::GetOldestMatchingContent()
{
	double oldest = numeric_limits<double>::max();
	Content *res = NULL;
#ifdef DEBUG
	NS_ASSERT(!matchingContents.empty());
#endif
	for(std::map<std::string, Content *>::iterator iter = matchingContents.begin(); iter != matchingContents.end(); iter++)
	{
		if((iter->second)->GetTTL() < oldest + TTL_MAX_DIFF && (iter->second)->IsForeign())
		{
			res = (iter->second);
			oldest = (iter->second)->GetTTL();
		}
	}

	return res;
}

Content *Interest::GetRandomMatchingContent()
{
	int rand = UniformVariable().GetInteger(0, matchingContents.size() -1);
	Content *res = NULL;
	int i = 0;
	std::map<std::string, Content *>::iterator iter = matchingContents.begin();
	while(i < rand && iter != matchingContents.end())
	{
		iter++;
		i++;
	}

	res = iter->second;

//	cout<<"F: "<<IsForeign()<<" Rand: "<<rand<<" matchingContents: "<<matchingContents.size()<<" Recv C: "<<c->GetDescription()<<" sel: "<<iter->second->GetDescription()<<endl;

	if(res == NULL)
	{
		NS_FATAL_ERROR("Invalid random Content selection");
	}

	return res;
}

bool Interest::ExeedsAllowedStorage()
{
	return (GetTotalSizeOfMatchingContents() > maxNumberOfAllowedBytes);
}

bool Interest::ExceedsAllowedUtilityBytes()
{
	return (GetTotalSizeOfMatchingContents() > utility);
}

bool Interest::HaveOtherMatchingContents(Content *c)
{
	for(std::map<std::string, Content *>::iterator iter = matchingContents.begin(); iter != matchingContents.end(); iter++)
	{
		if(iter->first.compare(c->GetDescription()) != 0)
			return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// 													Content Implementation
// ------------------------------------------------------------------------------------------------------------------------------------------------------

Content::Content(string description, string data, bool file, MOBITRADEProtocol *c)
{
#ifdef DEBUG
	NS_ASSERT(!description.empty());
#endif
	_control = c;

	this->description = description;
	this->isFile = file;
	if(file)
	{
		// Get data from the file and  store it
		ifstream tmpFile;
		tmpFile.open(data.c_str());
		string tmpLine;
		while(tmpFile >> tmpLine)
		{
			this->data.append(tmpLine);
		}
		tmpFile.close();

		// Setting the data size
#ifdef DEBUG
		NS_ASSERT(!this->data.empty());
#endif
		dataSize = this->data.length();
	}else
	{
		this->data = data;
		dataSize = this->data.length();
	}

	// Setting the initial utility
	this->utility = 1;
	this->recvTime = Simulator::Now().GetSeconds();
	foreign = false;
	matchingInterest = NULL;

}

Content::Content(std::string description, std:: string data, double ttl, MOBITRADEProtocol *c)
{
#ifdef DEBUG
	NS_ASSERT(!description.empty());
#endif
	_control = c;
	this->isFile = false;
	this->description = description;
	this->data = data;
	this->utility = 0;
	this->ttl = ttl;
	this->recvTime = Simulator::Now().GetSeconds();
	foreign = false;
	matchingInterest = NULL;

}

Content::Content(Content *c)
{
	foreign = c->IsForeign();
	utility = c->GetUtility();
	description = c->GetDescription();
	dataSize = c->GetDataSize();
	data = c->GetData();
	isFile = c->IsFile();
	sourceId = c->GetSourceId();
	ttl = c->GetTTL();
	recvTime = c->GetRecvTime();
	// Do not copy the matching Interest
	matchingInterest = NULL;
	this->_control = c->GetControl();
}

Content::~Content()
{
	// Sync with the matching Interest if any
	matchingInterest = NULL;
	_control = NULL;
}

bool Content::UpdateTTL()
{
	ttl = floor(ttl - (Simulator::Now().GetSeconds() - recvTime));
	recvTime = Simulator::Now().GetSeconds();
	if(ttl < 0)
		ttl = 0;
	return (ttl == 0);
}

double Content::GetElapsedTimeSinceReceived()
{
	return Simulator::Now().GetSeconds() - recvTime;
}

string Content::GetData()
{
	return data;
}



// Description#Data#sourceID#TTL#Utility
string Content::GetResume()
{
	string tmp;

	// Adding the description
	tmp.append(this->description);

	tmp.append("#");

	// Adding the data
	tmp.append(this->data);

	tmp.append("#");

	// Adding the sourceId
	tmp.append(this->sourceId);

	tmp.append("#");
	UpdateTTL();

	// Adding the TTL
	if(ttl < 0)
	{
		cout<<"Invalid Content TTL: "<<ttl<<endl;
		NS_FATAL_ERROR("INVALID Content TTL");
	}

	stringstream ss;
	ss << this->ttl;
	tmp.append(ss.str());

	tmp.append("#");

	// Adding the utility
	stringstream ss2;
	ss2 << this->utility;
	tmp.append(ss2.str());

	return tmp;
}

bool Content::IsOutOfDate()
{
	return Simulator::Now().GetSeconds() >= ttl;
}


void Content::SetMatchingInterest(Interest * i)
{
	//if(i != NULL && matchingInterest != NULL)
	//	NS_FATAL_ERROR("The matching interest is already set");

	matchingInterest = i;
}

void Content::SyncWithMatchingInterest()
{
	if(matchingInterest != NULL)
	{
		matchingInterest->SyncWithMatchingContents(this);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// 													Content Implementation
// ------------------------------------------------------------------------------------------------------------------------------------------------------

StatisticsEntry::StatisticsEntry(MOBITRADEProtocol * _c)
{
	_control = _c;
	sumInterestsUtilities = 0;
	recvBytes = 0;
	sentBytes = 0;
	numberMatchingContents = 0;
	numberForwardedContents = 0;
	numberOfReceivedInterests = 0;
	firstContentsForwarded = false;
}

StatisticsEntry::~StatisticsEntry()
{
	while(!contentsToForward.empty())
	{
		map<Content*,  ContentState*>::iterator iter = contentsToForward.begin();
		delete iter->first;
		free(iter->second);
		contentsToForward.erase(iter);
	}

	contentsToForward.clear();
	forwardingOrder.clear();
	mapReceivedInterests.clear();
	listReceivedContents.clear();
}


void StatisticsEntry::AddContentToForward(Content * c, Interest *mi)
{
#ifdef DEBUG
	NS_ASSERT(mi != NULL);
#endif
	if(!c->IsForeign())
		mi->SetLocal(true);

	if(contentsToForward.empty())
	{
		Content *tmp = new Content(c);
#ifdef DEBUG
		NS_ASSERT(tmp->IsForeign() == c->IsForeign());
#endif
		ContentState *cs = (ContentState *)malloc(sizeof(ContentState));
		cs->forwarded = false;
		cs->matchingI = mi;
		cs->dispatched = false;
		contentsToForward[tmp] = cs;
		numberMatchingContents++;
		tmp = NULL;
		cs = NULL;

	}else
	{
		map<Content*, ContentState*>::iterator iter = contentsToForward.begin();
		for(; iter != contentsToForward.end();iter++)
		{
			if((iter->first)->GetDescription().compare(c->GetDescription()) == 0)
				break;
		}

		if(iter == contentsToForward.end())
		{
			Content *tmp = new Content(c);
			ContentState *cs = (ContentState *)malloc(sizeof(ContentState));
			cs->forwarded = false;
			cs->matchingI = mi;
			cs->dispatched = false;
			contentsToForward[tmp] = cs;
			numberMatchingContents++;
			tmp = NULL;
			cs = NULL;
		}else NS_FATAL_ERROR("COntent already exists");
	}
}

void StatisticsEntry::ShowTheListOfContentsToForward()
{
	cout<<"List of contents to forward: "<<endl;
	for(map<Content*, ContentState*>::iterator iter = contentsToForward.begin(); iter != contentsToForward.end(); iter++)
	{
		cout<<"Content: "<<(iter->first)->GetDescription()<<" Associated Interest: "<<(iter->second)->matchingI->GetDescription()<<" foreign: "<<(iter->first)->IsForeign()<<endl;
	}
}

void StatisticsEntry::ShowForwardingOrder()
{
	cout<<"Forwarding Order: "<<endl;
	for(list<Content *>::iterator  iter = forwardingOrder.begin(); iter != forwardingOrder.end(); iter++)
	{
		cout<<"Content: "<<(*iter)->GetDescription()<<endl;
	}
}

unsigned int StatisticsEntry::GetInterestWeight(Interest *i)
{
	//ShowListReceivedInterests();
#ifdef DEBUG
	NS_ASSERT(sumInterestsUtilities > 0);
#endif
	unsigned int w = static_cast<unsigned int>(ceil(i->GetUtility()*100/sumInterestsUtilities));
	if(w  > 1000)
	{
		w = 1000 + w%1000;
	}
	return w;
}

// Establishes the Round robin forwarding order
void StatisticsEntry::EstablishForwardingOrder()
{
	if(!contentsToForward.empty() && forwardingOrder.empty())
	{
		// Choosing random start position
		int start = static_cast<int>(UniformVariable().GetValue(0, mapReceivedInterests.size()));

		// cout<<"Number Interests: "<<mapReceivedInterests.size()<<" Random Start: "<<start<<" contentsToForward: "<<contentsToForward.size()<<endl;
		// We stop only when all the contents are dispatched
		while(forwardingOrder.size() < contentsToForward.size())
		{
			int i = 0;
			// Advancing to the start position
			for(map<Interest*, int>::iterator iter = mapReceivedInterests.begin(); iter != mapReceivedInterests.end(); iter++, i++)
			{
				if(i >= start)
				{
					unsigned int j = 0;
					unsigned int interestWeight = GetInterestWeight(iter->first);
					//cout<<"1 Interest: "<<iter->first->GetDescription()<<" weight: "<<interestWeight<<" Utility: "<<iter->first->GetUtility()<<endl;
					interestWeight = max(interestWeight, static_cast<unsigned int>(1));

					// Add interestWeight newest contents not forwarded from the map
					if(interestWeight > 0)
					{
						bool allforwarded = false;
						while((j < interestWeight) && !allforwarded)
						{
							double newest = -1;
							std::map<Content*, ContentState*>::iterator sel = contentsToForward.end();
							allforwarded = true;
							for(std::map<Content*, ContentState*>::iterator iter2 = contentsToForward.begin(); iter2 != contentsToForward.end() ; iter2++)
							{
#ifdef DEBUG
								NS_ASSERT((iter2->second)->matchingI != NULL);
#endif
								if(!(iter2->second)->dispatched && (iter2->second)->matchingI == iter->first)
								{
									if(iter2->first->GetTTL() > newest)
									{
										allforwarded = false;
										newest = iter2->first->GetTTL();
										sel = iter2;
									}
								}
							}

							if(!allforwarded)
							{
								(sel->second)->dispatched = true;
								forwardingOrder.push_back(sel->first);
								j++;
							}
						}
					}
				}
			}

			// Dealing with the Interests left at the beginning of the map
			i = 0;
			for(map<Interest*, int>::iterator iter = mapReceivedInterests.begin(); iter != mapReceivedInterests.end(); iter++, i++)
			{
				if(i < start)
				{
					unsigned int j = 0;
					unsigned int interestWeight = GetInterestWeight(iter->first);
#ifdef DEBUG
					NS_ASSERT(interestWeight != 0);
#endif
					// Add interestWeight contents not forwarded from the map
					//cout<<"2 Interest: "<<iter->first->GetDescription()<<" weight: "<<interestWeight<<" Utility: "<<iter->first->GetUtility()<<endl;

					// Add interestWeight newest contents not forwarded from the map
					interestWeight = max(interestWeight, static_cast<unsigned int>(1));
					bool allforwarded = false;
					while((j < interestWeight) && !allforwarded)
					{
						double newest = -1;
						std::map<Content*, ContentState*>::iterator sel = contentsToForward.end();
						allforwarded = true;
						for(std::map<Content*, ContentState*>::iterator iter2 = contentsToForward.begin(); iter2 != contentsToForward.end(); iter2++)
						{
#ifdef DEBUG
							NS_ASSERT((iter2->second)->matchingI != NULL);
#endif
							if(!(iter2->second)->dispatched && (iter2->second)->matchingI == iter->first)
							{
								if(iter2->first->GetTTL() > newest)
								{
									allforwarded = false;
									newest = iter2->first->GetTTL();
									sel = iter2;
								}
							}
						}

						if(!allforwarded)
						{
							(sel->second)->dispatched = true;
							forwardingOrder.push_back(sel->first);
							j++;
						}
					}
				}else
				{
					break;
				}
			}
		}

		if(forwardingOrder.size() != contentsToForward.size())
		{

			cout<<" Size forwardingOrder: "<<forwardingOrder.size()<<endl;
			ShowTheListOfContentsToForward();

			NS_FATAL_ERROR("forwardingOrder.size() != contentsToForward.size()");
		}

	}
}


Content * StatisticsEntry::GetNextContentToForward()
{
	Content * sel = NULL;
	Interest * mi = NULL;

	if(ACTIVATE_MOBITRADE_POLICIES)
	{
		// Establish the weighted round robin forwarding order if not yet done
		if(forwardingOrder.empty())
			EstablishForwardingOrder();

		//ShowListReceivedInterests();
		//ShowTheListOfContentsToForward();
		//ShowForwardingOrder();

#ifdef DEBUG
		NS_ASSERT(forwardingOrder.size() == contentsToForward.size());
#endif
		for(std::list<Content *>::iterator iter = forwardingOrder.begin(); iter != forwardingOrder.end(); iter++)
		{
			if(!contentsToForward[*iter]->forwarded)
			{
				sel = *iter;
				mi = contentsToForward[*iter]->matchingI;
				sel->SetMatchingInterest(mi);
#ifdef DEBUG
				NS_ASSERT(sel != NULL);
				NS_ASSERT(mi != NULL);
#endif
				break;
			}
		}

	}else
	{
			// Random scheduling
			for(map<Content*, ContentState*>::iterator iter = contentsToForward.begin(); iter != contentsToForward.end(); iter++)
			{
				if(!(iter->second)->forwarded)
				{
					sel = iter->first;
					mi = (iter->second)->matchingI;
					sel->SetMatchingInterest(mi);
#ifdef DEBUG
					NS_ASSERT(sel != NULL);
					NS_ASSERT(mi != NULL);
#endif
					break;
				}
			}
	}
	return sel;
}

void StatisticsEntry::UpdateInterestSentBytes(Interest *i , double b)
{
#ifdef DEBUG
	NS_ASSERT(i != NULL);
#endif
	map<Interest *, int>::iterator iter = mapReceivedInterests.begin();
	for(;iter != mapReceivedInterests.end(); iter++)
	{
		if(iter->first->GetDescription().compare(i->GetDescription()) == 0)
			break;
	}
	if(iter != mapReceivedInterests.end())
	{
		iter->second += static_cast<int>(ceil(b));
	}else NS_FATAL_ERROR("INVALID INTEREST");
}

void StatisticsEntry::SetContentForwarded(Content * c)
{
	if(contentsToForward.empty())
		NS_FATAL_ERROR("StatisticsEntry::SetContentForwarded the contents map is empty.");
	else
	{
		map<Content* , ContentState*>::iterator iter = contentsToForward.begin();
		for(;iter!=contentsToForward.end();iter++)
		{
			if((iter->first)->GetDescription().compare(c->GetDescription()) == 0)
				break;
		}

		if(iter != contentsToForward.end())
		{
			numberForwardedContents++;
			(iter->second)->forwarded = true;
		}else
		{
			NS_FATAL_ERROR("StatisticsEntry::SetContentForwarded Modifying the status of a Content that does not exist.");
		}
	}
}


void StatisticsEntry::SetTheListOfReceivedInterests(std::list<Interest *> & recvList)
{
	if(!recvList.empty())
	{
#ifdef DEBUG
		NS_ASSERT(mapReceivedInterests.empty());
		NS_ASSERT(recvList.size() <= INTERESTS_STORAGE_CAPACITY);
#endif
		sumInterestsUtilities = 0;
		for(list<Interest*>::iterator iter = recvList.begin(); iter != recvList.end(); iter++)
		{
			mapReceivedInterests[*iter] = 0;
			sumInterestsUtilities += static_cast<unsigned int>((*iter)->GetUtility());
		}
#ifdef DEBUG
		NS_ASSERT(mapReceivedInterests.size() == recvList.size());
#endif
		numberOfReceivedInterests = mapReceivedInterests.size();
	}
}

void StatisticsEntry::AddNewReceivedContent(Content *c)
{
	if(listReceivedContents.empty())
	{
		listReceivedContents.push_back(c);
	}else
	{
		list<Content *>::iterator iter = listReceivedContents.begin();
		for(; iter != listReceivedContents.end();iter++)
		{
			if((*iter)->GetDescription().compare(c->GetDescription()) == 0)
			{
				break;
			}
		}
		if(iter == listReceivedContents.end())
		{
			listReceivedContents.push_back(c);
		}
	}
}


void StatisticsEntry::ShowListReceivedInterests()
{
	cout<<"Total sent bytes: "<<sentBytes<<endl;
	double sumu = 0;
	for(map<Interest *, int>::iterator iter = mapReceivedInterests.begin(); iter != mapReceivedInterests.end(); iter++)
	{
		sumu += (iter->first)->GetUtility();
		cout<<"Interest: "<<(iter->first)->GetDescription()<<" sentBytes: "<<iter->second<<" util: "<<iter->first->GetUtility()<<endl;
	}
	cout<<"Sum Utility: " <<sumu<<endl;
}

}

