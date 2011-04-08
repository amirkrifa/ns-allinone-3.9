#ifndef MOBITRADE_PROTOCOL_HELPER
#define MOBITRADE_PROTOCOL_HELPER

#include "ns3/mobitrade_protocol.h"
#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "node-container.h"
#include "ipv4-routing-helper.h"

namespace ns3
{

class MOBITRADEProtocolHelper : public Ipv4RoutingHelper
{
public:
  MOBITRADEProtocolHelper ();
  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   *
   * This method will be called by ns3::InternetStackHelper::Install
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;
  /**
   * \brief virtual constructor
   * \returns pointer to clone of this Ipv4RoutingHelper 
   * 
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  virtual Ipv4RoutingHelper* Copy (void) const
  {
    return new MOBITRADEProtocolHelper();
  }


private:
  
};

}// namespace ns3
#endif
