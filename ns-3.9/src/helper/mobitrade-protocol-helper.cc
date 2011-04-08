#include "mobitrade-protocol-helper.h"

#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

namespace ns3 {


NS_LOG_COMPONENT_DEFINE ("MOBITRADEProtocolHelper");

MOBITRADEProtocolHelper::MOBITRADEProtocolHelper ()
{

}

Ptr<Ipv4RoutingProtocol> 
MOBITRADEProtocolHelper::Create (Ptr<Node> node) const
{
  ObjectFactory m_agentFactory;
  m_agentFactory.SetTypeId ("ns3::MOBITRADEProtocol");
  Ptr<MOBITRADEProtocol> agent = m_agentFactory.Create<MOBITRADEProtocol> ();
  node->AggregateObject (agent);
  return agent;
}


} // namespace ns3
