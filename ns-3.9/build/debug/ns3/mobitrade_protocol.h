#ifndef MOBITRADE_PROTOCOL
#define MOBITRADE_PROTOCOL

#include "ns3/packet.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/timer.h"
#include "ns3/ipv4-routing-table-entry.h"
#include <semaphore.h>

#include <map>
#include <list>
#include <fstream>
#include <iostream>

#define SIM_DURATION 86500

// TIME window over which we average our measures
#define TIME_WINDOW 86400

#define CONTENT_PACKET_SIZE 1000000
// Whether to activate or not the MobiTrade drop and scheduling policies
#define ACTIVATE_MOBITRADE_POLICIES true
// Whether to activate or not the MobiTrade collaboration (interests forwarding)
#define ACTIVATE_MOBITRADE_COLLABORATION true
// Either to use the tit for tat and force one content for one or not
#define USE_TIT_FOR_TAT
// Whether to track or not the nodes frequency of meetings
#define TRACK_NODES_MEETINGS false
// Whether to Activate the malicious nodes or not
#define ACTIVATE_MALICIOUS_NODES false
#define ENABLE_MERGED_INTERESTS false
// Whether to enable or not that some nodes comes some times after the
#define ENABLE_THAT_SOME_NODES_COME_LATER false

// Updates or not the list of generated Interests during the simulation
#define UPDATE_INTERESTS_DETAILS false
// Either to enable the drop oldest policy + reward only for new contents or not
#define ENABLE_TTL_AWAIRNESS
#define TTL_MAX_DIFF 0

const int NUMBER_OF_NODES = 50;
// Controlling the generation of the Interests, number of Interests to generate
const int NUMBER_SLOTS = 2;
#undef ENABLE_RANDOM_NUMBER_OF_INTERESTS

const int MAX_SIZE_OF_CHANNEL = 20;
#define ENABLE_RANDOM_CHANNELS_SIZE
#define ENABLE_FIXED_SIZES

// Contents and Interests generation intervals
const unsigned int CONTENTS_GENERATION_INTERVAL = 3600;
const unsigned int INTERESTS_GENERATION_INTERVAL = 360000;

// Contents generations times
const int MAX_CONTENTS_GENERATION_TIMES = 20;
const int MAX_INTERESTS_GENERATION_TIMES = 20;

// Contents and Interests TTLs
const double DEFAULT_CONTENT_TTL = 2*SIM_DURATION;
const double DEFAULT_INTEREST_TTL = 2*SIM_DURATION;

// We express Content storage in terms of Bytes and we suppose that all generated Content have the same size of 1 Mo
#define INTERESTS_STORAGE_CAPACITY 600000
#define CONTENTS_STORAGE_CAPACITY 120


const unsigned int INITIAL_LOCAL_INTEREST_UTILITY = CONTENTS_STORAGE_CAPACITY*CONTENT_PACKET_SIZE;


// Generosity level indicating the first number of contents to forward to the remote
// peer even if it does not send as anything back
#define GENEROSITY 1


// Networking parameters
#define MOBITRADE_ROUTING_PORT  2617
#define BROADCAST_ADDR "10.1.1.255"

#define HELLO_MAX_JITTER (helloInterval.GetSeconds () / 4)
#define HELLO_JITTER (Seconds (UniformVariable().GetValue (0, HELLO_MAX_JITTER)))


// Messages IDs
#define SESSION_NOT_FOUND -1
#define HELLO_MSG_RECEIVED 0
#define INTERESTS_SENT 1
#define SENDING_WAITING_FOR_CONTENTS 2
#define END_SESSION_REQUESTED 3

// Logging interval, Sessions cleaning interval, Hello INterval
const unsigned int LOG_INTERVAL = SIM_DURATION - 100;
const unsigned int HELLO_INTERVAL = 60;
const unsigned int SESSIONS_TIMER_INTERVAL = 30;
const unsigned int SESSIONS_TIME_OUT = 5;
const unsigned int MAX_INTERESTS_SIZE = 99999;
#undef DEBUG

namespace ns3{


class Content;
class MOBITRADEProtocol;
class Interest
{
public:

	Interest(std::string resume, bool isForeignInterest, MOBITRADEProtocol*);
	Interest(std::string  description, double ttl, double utility, MOBITRADEProtocol*);

	~Interest();
	bool IsEqual(Interest * i);

	inline void SetUtility(double val)
	{
		utility = val;
		UpdateUtilityOfMatchingContents();
	}
	inline double GetUtility()
	{
		return utility;
	}

	inline std::string GetDescription()
	{
		return description;
	}

	bool Matches(std::string desc, int & numberOfMatches);

	inline void SetTTL(double ttl)
	{
		this->ttl = ttl;
	}

	inline double GetTTL()
	{
		if(ttl < 0)
		{
			std::cout <<"Interests TTL: "<<ttl<<std::endl;
			NS_FATAL_ERROR("ttl < 0");
		}
		return ttl;
	}

	inline double GetRecvTime()
	{
		return recvTime;
	}

	inline void SetRecvTime(double t)
	{
		recvTime  = t;
	}

	double GetElapsedTimeSinceReceived();

	std::string GetResume();

	bool IsOutOfDate();

	bool UpdateTTL();

	inline bool IsForeign()
	{
		return foreignInterest;
	}
	inline void SetForeign(bool f)
	{
		foreignInterest = f;
	}
	inline void SetSatisfied()
	{
		satisfied = true;
	}
	inline bool IsSatisfied()
	{
		return satisfied;
	}

	// Used only for foreign Interests
	void AddNewMatchingContent(Content * c);
	void RemoveMatchingContent(Content *c);
	int GetNumberOfMatchingContents();
	void SyncWithMatchingContents(Content *c);
	void ShowMatchingContents(std::ostream &strm);
	void SyncWithMatchingContents();
	void UpdateUtilityOfMatchingContents();
	void GetMatchingContents(std::list<Content *> match);

	unsigned int GetTotalSizeOfMatchingContents();
	inline MOBITRADEProtocol *GetControl()
	{
		return _control;
	}

	inline unsigned int GetInterestAllowedBytes()
	{
		return maxNumberOfAllowedBytes;
	}

	inline void SetInterestAllowedStorage(unsigned int i)
	{
		maxNumberOfAllowedBytes = i;
	}

	bool ExeedsAllowedStorage();
	bool ExceedsAllowedUtilityBytes();
	bool HaveOtherMatchingContents(Content *c);
	Content *GetOldestMatchingContent();
	Content * GetRandomMatchingContent();
	inline bool IsLocal()
	{
		return local;
	}
	inline void SetLocal(bool t)
	{
		local = t;
	}
private:

	unsigned int maxNumberOfAllowedBytes;
	double utility;
	std::string description;
	bool foreignInterest;
	double ttl;
	double recvTime;
	bool satisfied;
	// Pointer to the main protocol
	MOBITRADEProtocol * _control;

	// Maps the list of matching contents in the case of a foreign Interest
	// Used to track the Contents and the portion of buffer each Interest is occupying
	std::map<std::string, Content *> matchingContents;
	bool local;
};

class Content
{

public:
	Content(std::string description, std::string data, bool file, MOBITRADEProtocol*);
	Content(std::string description, std:: string data, double ttl, MOBITRADEProtocol*);
	Content(Content *c);
	~Content();

	inline double GetUtility()
	{
		return utility;
	}

	inline std::string GetDescription()
	{
		return description;
	}

	inline bool IsFile()
	{
		return isFile;
	}

	std::string GetData();

	inline double GetDataSize()
	{
		// We suppose that the size of data is by default 1 Mo
		return CONTENT_PACKET_SIZE;
	}

	inline void SetSourceId(std::string source)
	{
		sourceId = source;
	}

	inline void SetTTL(double ttl)
	{
		this->ttl = ttl;
	}

	inline double GetTTL()
	{
		if(ttl < 0)
		{
			std::cout <<"Content TTL: "<<ttl<<std::endl;
			NS_FATAL_ERROR("ttl < 0");
		}
		return this->ttl;
	}
	std::string GetResume();

	bool IsOutOfDate();
	inline void SetUtility(double util)
	{
		utility = util;
	}
	double GetElapsedTimeSinceReceived();
	bool UpdateTTL();
	inline void SetForeign()
	{
		foreign = true;
	}

	inline void SetLocal()
	{
		foreign = false;
	}

	inline bool IsForeign()
	{
		return foreign;
	}

	inline std::string GetSourceId()
	{
		return sourceId;
	}

	inline double GetRecvTime()
	{
		return recvTime;
	}

	// Used to manage the matching foreign Interest
	inline Interest * GetMatchingInterest()
	{
		return matchingInterest;
	}
	void SetMatchingInterest(Interest * i);
	void SyncWithMatchingInterest();
	inline MOBITRADEProtocol * GetControl()
	{
		return _control;
	}
private:

	bool foreign;
	double utility;
	std::string description;
	double dataSize;
	std::string data;
	bool isFile;
	std::string sourceId;
	double ttl;
	double recvTime;
	MOBITRADEProtocol * _control;
	// Points to the matching Interest, for the moment a Content can match only one Interest at once
	Interest * matchingInterest;
};

class StatisticsEntry
{
public:
	StatisticsEntry(MOBITRADEProtocol * _);
	~StatisticsEntry();

	void AddContentToForward(Content * c, Interest *mi);
	Content * GetNextContentToForward();
	void SetContentForwarded(Content * c);
	void SetTheListOfReceivedInterests(std::list<Interest *> & recvList);
	void ShowTheListOfContentsToForward();
	void AddNewReceivedContent(Content *c);
	void UpdateInterestSentBytes(Interest * , double);
	void EstablishForwardingOrder();
	unsigned int GetInterestWeight(Interest *i);
	void ShowListReceivedInterests();
	void ShowForwardingOrder();
	unsigned int recvBytes;
	unsigned int sentBytes;
	int numberMatchingContents;
	int numberForwardedContents;
	int numberOfReceivedInterests;

	typedef struct ContentState
	{
		bool forwarded;
		Interest * matchingI;
		bool dispatched;
	}ContentState;

	std::map<Interest *, int> mapReceivedInterests;
	std::list<Content *> listReceivedContents;
	std::map<Content*, ContentState*> contentsToForward;
	std::list<Content *> forwardingOrder;
	bool firstContentsForwarded;
	unsigned int sumInterestsUtilities;
private:
	MOBITRADEProtocol * _control;
};

class Ipv4Address;
class MOBITRADEMsgHeader;

class MOBITRADEProtocol: public Ipv4RoutingProtocol
{
public:

  MOBITRADEProtocol();
  ~MOBITRADEProtocol();

  static TypeId GetTypeId (void);
  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  virtual bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,UnicastForwardCallback ucb, MulticastForwardCallback mcb,         LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);

  /**
   * Initializing the node parameters.
   */

  void Start();

  void GenerateLocalInterests(int start, int end);
  void GenerateLocalContents(int start, int end);

  /**
   * Sends a hello message once the Hello timer expires
   */
  void SendHello ();

  /**
   * Sends the list of Interests to the remote peer
   */
  void SendListOfInterests(Ipv4Address destDevice);

  /**
   * Sends a requested Content to the remote peer
   */
  void SendContent(Ipv4Address destDevice, Content * content);

  /**
   * Sends a message announcing that there is no more contents to send so close the session
   */
  void SendEndSession(Ipv4Address destDevice);

  /**
   * Sends a packet.
   */
  void SendPacket (Ptr<Packet> packet, int msgType);

  /**
   * Recv a new packet.
   */
  void RecvPacket (Ptr<Socket> socket);

  /**
   * Triggered once the Hello timer expires.
   */
  void HelloTimerExpired ();
  
  void ContentsGenerationTimerExpired();

  void InterestsGenerationTimerExpired();

  /**
   * Process a new received hello message.
   */
  void ProcessHelloMsg (MOBITRADEMsgHeader &, Ipv4Address);

  /**
   * Process a new received Interest message.
   */
  void ProcessInterestMsg (Ptr<Packet>, Ipv4Address);

  /**
   * Process a new received Content message.
   */
  void ProcessContentMsg (Ptr<Packet>, Ipv4Address);

  /**
   * Process a new received Content message.
   */
  void ProcessEndSessionMsg (Ptr<Packet>, Ipv4Address);

  /*
   * Sets the default Route.
   */
  void SetDefaultRoute (Ipv4Address nextHop, uint32_t interface);
  /**
   * Updates the given node last meeting time.
   */
  void UpdateNodeLastMeetingTime(Ipv4Address node);
  bool NodeAlreadyEncountered(Ipv4Address node);

  /**
   * Returns the last time we met the given node
   */
  Time GetNodeLastMeetingTime(Ipv4Address node);
  /**
   *  Starts a new session with the given node.
   */
  void StartNewSession(Ipv4Address node);
  /**
   * Verifies if a given session is active or not
   */
  bool IsSessionActive(Ipv4Address& node);
  /**
   * Changes the session state
   */
  void ChangeSessionState(Ipv4Address& node, int state);
  /**
   * Get the session state
   */
  int GetSessionState(Ipv4Address& node);

  /**
   * Removes the session associated with the given device
   */
  void RemoveSession(Ipv4Address& node);
  /**
   * Triggered when the SessionsTimer expires.
   */
  void SessionsTimerExpired();

  void LogTimerExpired();
  /**
   * Drops the out of date sessions with other nodes
   */
  void CleanOutOfDateSessions();
  void CleanOutOfDateInterests();
  void CleanOutOfDateContents();

  /**
   * Loads a list of contents from a given file and makes them available in the local storage
   */
  void AddNewInterest(Interest * i);

  /**
   * Returns the Interest having the lowest utility
   */
  Interest * GetTheInterestHavingTheLowestUtility(double & lu);
  Interest * GetTheInterestHavingTheLowestUtilityWithAssociatedContents(Content *c, double & lu);

  /**
   * Returns the Content having the lowest utility
   */
  Content * GetTheContentHavingTheLowestUtility(double & lu);

  /**
   * Adds a new Interest to the storage
   */
  void AddNewContent(Content * c);
  void AddNewGeneratedContent(Content * c);
  void AddNewContentAvailableStorage(Content *c);
  void AddNewContentFullStorage(Content *c);
  void DispatchAvailableStorage();
  void CleanUpTheStorage();
  /**
   * Parses the list of received interests, triggers the forwarding block to forward contents
   */
  bool ReceivedNewInterests(std::string interests, std::list<Interest *> & listInterests);

  /**
   * Finds the matching contents to the list of received interests and forward them.
   */
  void ForwardFirstContents(Ipv4Address & sender);

  void ForwardAllMatchingContents(Ipv4Address & sender);

  /**
   * Returns the list of contents that match the given Interest
   */
  void AddContentsToForward(Ipv4Address & sender, Interest * i, int index);

  /**
   * Adds the list of received Interests and updates Interests utilities.
   */
  void addNewInterests(Ipv4Address &sender);
  /**
   * Returns the list of Interests to send to the other peer
   */

  std::string GetListOfInterests(int * numberOfInterests);

  // Parses an interest string and return its description
  /**
   * Parses an Interest description and returns its description
   */
  static std::string GetInterestDescription(std::string &interest);

  /**
   * Parses an Interest description and returns its Utility
   */
  static  double GetInterestUtility(std::string &interest);

  /**
   * Parses an Interest and returns its TTL
   */
  static double GetInterestTTL(std::string &interest);

  static double GetContentUtility(std::string & content);

  /**
   * Parses a Content and returns its Description
   */
  static  std::string GetContentDescription(std::string & content);


  /**
   * Parses a content and returns its Data
   */
  static std::string GetContentData(std::string & content);

  /**
   * Parses a Content and returns its Source ID
   */
  static std::string GetContentSourceID(std::string & content);

  /**
   * Parses a Content and returns its TTL
   */
  static double GetContentTTL(std::string & content);

  /**
   * Returns the number of exchanged bytes with a given peer.
   */
  void InitExchangedBytes(Ipv4Address  adr);

  /**
   * Returns the number of exchanged bytes with a given peer.
   */
  bool ContinueSending(Ipv4Address & adr);

  /**
   * Update the number of exchanged bytes with a given peer.
   */
  void UpdateSentBytes(Ipv4Address & adr, int nbrBytes);

  /**
   * Update the number of exchanged bytes with a given peer.
   */
  void UpdateRecvBytes(Ipv4Address & adr, int nbrBytes);

  /**
   * Returns the size in bytes of the received contents from the remote peer
   */
  int GetSizeOfReceivedContents(Ipv4Address & adr);

  /**
   * Sets the number of matching contents for a given device
   */
  void SetNumberOfMatchingContents(Ipv4Address & adr, int nb);

  /**
   * Returns the number of matching contents for a given device
   */
  int GetNumberOfMatchingContents(Ipv4Address & adr);
  /**
   * Updates the local Interests utilities given the list of received Interests
   */
  void UpdateLocalInterestsUtilities(Ipv4Address &sender, std::map<Interest*, int> & receivedInterests);

  void UpdateInterestsAllowedBytes();

  /**
   * Generates a interest and adds it to the local storage
   */
  void GenerateLocalInterest(int from, int to);
  void GenerateLocalInterest(int index, int details, bool withDetails);

  /**
   * Generates a content and adds it to the local storage
   */
  void GenerateLocalContent(int min, int max);
  void GenerateLocalContent(int index, int details, bool withDetails);

  void ShowContents(std::ostream & strm);
  void ShowNodesCollaboration(std::ostream & strm);
  void ShowNodesNumberMeetings(std::ostream & strm);
  void ShowInterestsVsContents(std::ostream & strm);
  void ShowListSession(std::ostream & strm);
  void ShowInterests(std::ostream & strm);

  void ShowDeliveredContents(std::ostream & strm);
  void ShowDeliveredContentsPerChannel(std::ostream & strm);

  inline void AddNewRequestedContent()
  {
	  numberRequestedContents++;
  }

  inline unsigned int GetNumberRequestedContents()
  {
	  return numberRequestedContents;
  }


  inline unsigned int GetNumberDeliveredContents()
  {
	  return deliveredContents.size();
  }

  inline void AddNewGeneratedContent()
  {
	  numberGeneratedContents++;
  }

  inline unsigned int GetNumberGeneratedContents()
  {
	  return numberGeneratedContents;
  }

  bool MatchesLocalInterest(Content *c, double * delay, bool & mFroeign);
  bool MatchesLocalContent(Interest *i);
  void SyncInterestToContent(Interest * i, Content *c);
  void SyncInterestToAvailableContent(Interest * i);
  void SyncContentToAvailableInterests(Content *c);

  void LogStatistics();
  void AddNewDeliveredContent(Content *c, double delay);
  void CleanSession (Ipv4Address dest);
  double GetAverageDeliveryDelay();


  bool ContentExists(std::string desc);
  bool InterestExists(std::string desc);

  double GetSumOfInterestsUtilities();


  inline double GetLastMeetingTimeStamp()
  {
	  return lastMeeting;
  }

  inline void SetLastMeetingTimeStamp(double time)
  {
	  lastMeeting = time;
  }

  inline void SetMalicious(bool val)
  {
	  maliciousNode = val;
  }

  inline bool IsMalicious()
  {
	  return maliciousNode;
  }

  inline void SetActive(bool val)
  {
	  if(val)
	  {
		  startTime = Simulator::Now().GetSeconds();
	  }

	  activeNode = val;
  }

  inline bool IsActive()
  {
	  return activeNode;
  }

  typedef struct CollXY
  {
	 double time;
	 double coll;
   }CollXY;
  // Tracks the list of met nodes
  std::map<Ipv4Address, std::list<CollXY> > nodesCollaboration;
  std::map<Ipv4Address, int > nodesNumberMeetings;

  // Used only when we want to update an already generated set
  // of Interests
  std::list<int> generatedInterestsIds;
  void AddNewInterestId(int i);
  void GenerateUpdatedInterests();

  // Used to generate Channels with random sizes
  void GenerateChannelsWithRandomSizes(int nbrChannels);
  int GetSizeOfRequestedInterests();
  int GetChannelSize(int channelId);
  int GetChannelSizeId(std::string channelId);

  void SortAvailableInterests();
  Interest * GetInterestWithSmallestUtilityThatExceedsItsAllowedStorage();
  Interest * GetInterestWithSmallestUtilityThatExceedsItsShare();
  Interest * GetInterestWithSmallestUtilityThatExceedsItsUtility();
  Interest * GetInterestThatExceedsTheMostItsShare();
  unsigned int GetSumInterestsUtilities();
  bool RemoveForeignContent(Content *c);
  bool RemoveLocalContent(Content *c);
  unsigned int GetMaxUtility();


private:

  // Neighbors last meeting time map
  std::map<Ipv4Address,  Time> nodesLastMeetingTimeMap;

  // The list of current active sessions
  std::map<Ipv4Address, int > currentActiveSessions;
  std::map<Ipv4Address, StatisticsEntry* > exchangedBytes;

  // Interests Map
  std::map<std::string, Interest *> mapInterests;
  std::vector<Interest *> sortedInterests;


  // Contents Map
  std::map<std::string, Content *> mapContents;
  std::map<std::string, Content *> generatedContents;

  std::map<std::string, double> deliveredContents;
  std::map<std::string, int> deliveredPerChannel;


  sem_t contentsSem;
  sem_t interestsSem;
  sem_t sessionsSem;

  // Local socket
  Ptr<Socket> helloSendingSocket;
  Ptr<Socket> listeningSocket;

  // Main local addess
  Ipv4Address mainAddress;
  Ptr<Ipv4> m_ipv4;

  // Hello Timer
  Timer helloTimer;
  Timer sessionsTimeOutTimer;
  Timer logTimer;
  Timer contentsGenerationTimer;
  Timer interestsGenerationTimer;

  int contentsIndex;
  int interestsIndex;

  Time helloInterval;
  Time sessionsTimeOut;

  Ipv4RoutingTableEntry *m_defaultRoute;
  int nodeId;

  // For Statistics
  unsigned int numberRequestedContents;
  unsigned int numberGeneratedContents;

  // For the time weighted moving average
  double lastMeeting;
  double weight;

  // Indicates whether the node is malicious or not
  bool maliciousNode;
  // Indicates whether the node is active or not. So, it will join the sharing process later
  bool activeNode;
  // Time at which the node joined the MobiTrade network
  double startTime;
  int numberOfGeneratedInterests;
};

}
#endif
