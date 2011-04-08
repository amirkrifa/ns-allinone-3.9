/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mobitrade-protocol-helper.h"
#include "ns3/mobitrade_protocol.h"


#include <fstream>

using namespace ns3;
static NodeContainer c;

NS_LOG_COMPONENT_DEFINE ("MOBITRADEScenario");

void ReceivePacket (Ptr<Socket> socket)
{
  NS_LOG_UNCOND ("Received one packet!");
}

/*
static Ipv4Address GetIp(Ptr<Node> &n)
{
  Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
  return iaddr.GetLocal (); 
}

static void ShowNodesPositions()
{
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;

      Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
      if (! mob) 
        {
          cout<<"Strange, node does not have a mobility model !"<<endl;
          continue; 
        }
      Vector pos = mob->GetPosition ();
      std::cout << "At "<<Simulator::Now ()<<" Node "<<GetIp(node)<<" is at (" << pos.x << ", " << pos.y<<")"<<endl;
    }
}
*/

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, 
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      //ShowNodesPositions();	
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic, socket, pktSize, pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}


int main (int argc, char *argv[])
{

  std::string phyMode ("DsssRate1Mbps");
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1000;
  double interval = 60.0; // seconds
  bool verbose = false;
  int numberNodes = 90;

  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  
  cmd.Parse (argc, argv);

  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));

  c.Create (numberNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

  YansWifiChannelHelper wifiChannel ;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel"); 
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue(phyMode),
                                   "ControlMode",StringValue(phyMode));
  // Set it to AdHoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  // Note that with FixedRssLossModel, the positions below are not 
  // used for received signal strength. 
  MobilityHelper mobility;


  // Used to test the wifi range
  /*Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (100.0, 0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  */

  ObjectFactory pos;

  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", RandomVariableValue (UniformVariable (0, 2000)));
  pos.Set ("Y", RandomVariableValue (UniformVariable (0, 2000)));

  Ptr<PositionAllocator> positionAlloc = pos.Create ()->GetObject<PositionAllocator> ();

  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel", "Speed", RandomVariableValue (UniformVariable(1,2)),                                       "Pause", RandomVariableValue (ConstantVariable (10)),                "PositionAllocator", PointerValue (positionAlloc));

  mobility.SetPositionAllocator (positionAlloc);

  mobility.Install (c);

  InternetStackHelper internet;
  MOBITRADEProtocolHelper mobitradeHelper;

  internet.SetRoutingHelper(mobitradeHelper);

  internet.Install (c);
  
  Ipv4AddressHelper ipv4;
  NS_LOG_UNCOND ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // Output what we are doing
  // NS_LOG_UNCOND ("Testing " << numPackets  << " packets sent with receiver rss " << rss);
  
  //Simulator::ScheduleWithContext (source->GetNode ()->GetId (),Seconds (1.0), &GenerateTraffic,source, packetSize, numPackets, interPacketInterval);

  Simulator::Stop (Seconds(36100.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
