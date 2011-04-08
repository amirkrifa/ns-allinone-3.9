#ifndef MOBITRADE_MOBILITY_MODEL_H
#define MOBITRADE_MOBILITY_MODEL_H

#include "mobility-model.h"
#include "constant-velocity-helper.h"
#include "position-allocator.h"
#include "ns3/ptr.h"
#include "ns3/event-id.h"

namespace ns3 {

class MobiTradeMobilityModel : public MobilityModel
{
public:

  static TypeId GetTypeId (void);
  /**
   * Create a position located at coordinates (0,0,0)
   */

  MobiTradeMobilityModel();
  virtual ~MobiTradeMobilityModel();

private:

  virtual Vector DoGetPosition (void) const;
  virtual void DoSetPosition (const Vector &position);
  virtual Vector DoGetVelocity (void) const;

  void BeginWalk (void);

  ConstantVelocityHelper m_helper;

  Ptr<PositionAllocator> m_position;

  double m_speed;
  Time m_pause;
  Time m_prevTime;
  EventId m_event;
  bool started;
};

};

#endif /* MOBITRADE_MOBILITY_MODEL_H */