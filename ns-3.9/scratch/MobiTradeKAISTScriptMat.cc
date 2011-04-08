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

  /*                                "DataMode", StringValue ("OfdmRate54Mbps"));
                                "DataMode", StringValue ("OfdmRate48Mbps"));
                                "DataMode", StringValue ("OfdmRate36Mbps"));
                                "DataMode", StringValue ("OfdmRate24Mbps"));
                                "DataMode", StringValue ("OfdmRate18Mbps"));
                                "DataMode", StringValue ("OfdmRate12Mbps"));
                                "DataMode", StringValue ("OfdmRate9Mbps"));
                               "DataMode", StringValue ("OfdmRate6Mbps"));
 */

#include "ns3/core-module.h"
#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mobitrade-protocol-helper.h"
#include "ns3/MobiTradePositionAllocator.h"

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

  
  int numberNodes = 49;

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("OfdmRate36Mbps"));


  c.Create (numberNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate36Mbps"));

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
   wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  // Note that with FixedRssLossModel, the positions below are not 
  // used for received signal strength. 
  MobilityHelper mobility;


  // Creating MobiTrade Positions allocators
  Ptr<MobiTradePositionAllocator> positionAlloc [numberNodes];
  for(int i = 0; i< numberNodes; i++)
  {
	  positionAlloc[i] = CreateObject<MobiTradePositionAllocator>();
  }


  for(int i = 0; i< numberNodes; i++)
  {
	  positionAlloc[i]->SetAttribute("FileNumber", UintegerValue(i+1));
	  positionAlloc[i]->ParseFile();
	  mobility.SetPositionAllocator (positionAlloc[i]);
	  mobility.SetMobilityModel("ns3::MobiTradeMobilityModel", "PositionAllocator", PointerValue(positionAlloc[i]));
	  mobility.Install(c.Get(i));
  }

  // Connecting the Internet stack

  InternetStackHelper internet;
  MOBITRADEProtocolHelper mobitradeHelper;
  internet.SetRoutingHelper(mobitradeHelper);

  internet.Install (c);
  
  Ipv4AddressHelper ipv4;
  NS_LOG_UNCOND ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // Tracing
  //wifiPhy.EnablePcap ("wifi-dtn", devices);

  // Setting the length of the simulation
  Simulator::Stop (Seconds(86500.0));
  // Starting the simulation
  Simulator::Run ();
  // Free simulation resources
  Simulator::Destroy ();

  return 0;
}
