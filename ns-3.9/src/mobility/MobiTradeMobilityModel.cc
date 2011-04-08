#include "MobiTradeMobilityModel.h"
#include "MobiTradePositionAllocator.h"
#include "ns3/simulator.h"
#include "ns3/random-variable.h"
#include "ns3/pointer.h"
#include "MobiTradePositionAllocator.h"
#include "ns3/double.h"

#include <cmath>
#include <iostream>
using namespace std;

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (MobiTradeMobilityModel);

TypeId MobiTradeMobilityModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MobiTradeMobilityModel")
	.SetParent<MobilityModel> ()
	.SetGroupName ("Mobility")
	.AddConstructor<MobiTradeMobilityModel> ()
	.AddAttribute ("Speed",
				   "A random variable used to pick the speed of a random waypoint model.",
				   RandomVariableValue (UniformVariable (0.3, 0.7)),
				   MakeDoubleAccessor (&MobiTradeMobilityModel::m_speed),
				   MakeDoubleChecker<double> ())
	.AddAttribute ("Pause",
				   "A random variable used to pick the pause of a random waypoint model.",
				   RandomVariableValue (ConstantVariable (2.0)),
				   MakeTimeAccessor (&MobiTradeMobilityModel::m_pause),
				   MakeTimeChecker ())
	.AddAttribute ("PositionAllocator",
				   "The position model used to pick a destination point.",
				   PointerValue (),
				   MakePointerAccessor (&MobiTradeMobilityModel::m_position),
				   MakePointerChecker<PositionAllocator> ());
  return tid;
}

MobiTradeMobilityModel::MobiTradeMobilityModel ()
{
	m_speed = 0;
	started = false;
}

MobiTradeMobilityModel::~MobiTradeMobilityModel ()
{

}

void MobiTradeMobilityModel::BeginWalk (void)
{
	if(!started)
	{
		Ptr<MobiTradePositionAllocator> temp = StaticCast<MobiTradePositionAllocator> (m_position);
		m_helper.SetPosition( m_position->GetNext());
		m_prevTime = temp->GetNextTime ();
		started = false;
	}


	m_helper.Update ();
	Vector m_current = m_helper.GetCurrentPosition ();
	Ptr<MobiTradePositionAllocator> temp = StaticCast<MobiTradePositionAllocator> (m_position);
	Time time = temp->GetNextTime ();

	Time diff = time - m_prevTime;

	m_prevTime = time;
	Vector destination = m_position->GetNext();

	double dx = (destination.x - m_current.x);
	double dy = (destination.y - m_current.y);
	double dz = (destination.z - m_current.z);

	if (dx == 0 && dy == 0)
	{
		m_helper.Update ();
		m_helper.Pause ();
		if (diff.IsPositive ())
			m_event = Simulator::Schedule (diff, &MobiTradeMobilityModel::BeginWalk, this);
	}
	else
	{
		double output;

		if (diff.GetSeconds () == 0)
			output = 0;
		else
			output = CalculateDistance (destination, m_current) / diff.GetSeconds ();

		m_speed = abs(output);

		double k = m_speed / std::sqrt (dx*dx + dy*dy + dz*dz);

		m_helper.SetVelocity (Vector (k*abs(dx), k*abs(dy), k*dz));
		/*cout << "FileName: "<<temp->GetFileName()<<endl;
		cout << "BeginWalk, m_current.x = " << m_current.x << ", m_current.y = " << m_current.y << "\n";
		cout << "BeginWalk, destination.x = " << destination.x << ", destination.y = " << destination.y << "\n";
		cout << "TimeDiff: "<<diff.GetSeconds()<<endl;
		cout << "dx = " << dx << ", dy = " << dy << "\n";
		cout << "Speed = " << m_speed << ", Velocity = " << Vector (k*abs(dx), k*abs(dy), k*dz) << "\n";
		cout<<endl<<endl;
	*/
		m_helper.Unpause ();

		if (diff.IsPositive ())
			m_event = Simulator::Schedule (diff, &MobiTradeMobilityModel::BeginWalk, this);
	}

	NotifyCourseChange ();
}

Vector MobiTradeMobilityModel::DoGetPosition (void) const
{
	m_helper.Update ();
	return m_helper.GetCurrentPosition ();
}

void MobiTradeMobilityModel::DoSetPosition (const Vector &position)
{
	 m_helper.SetPosition (position);
	 Simulator::Remove (m_event);
	 m_event = Simulator::ScheduleNow (&MobiTradeMobilityModel::BeginWalk, this);
}

Vector MobiTradeMobilityModel::DoGetVelocity (void) const
{
	 return m_helper.GetVelocity ();
}

}; // namespace ns3