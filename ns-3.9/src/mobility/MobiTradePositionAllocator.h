#ifndef MOBITRADE_POSITION_ALLOCATOR_H
#define MOBITRADE_POSITION_ALLOCATOR_H

#include "position-allocator.h"
#include <string>
#include <vector>
#include "ns3/nstime.h"

#define USE_KAIST_TRACES false

namespace ns3
{
class MobiTradePositionAllocator : public PositionAllocator
{
public:
	static TypeId GetTypeId (void);
	MobiTradePositionAllocator ();
	virtual ~MobiTradePositionAllocator ();

	/**
	 * \param v the position to append at the end of the list of positions to return from GetNext.
	 */
	void Add (Vector v);
	void AddTime (Time t);
	virtual Vector GetNext (void) const;
	Time GetNextTime (void);
	void LoadInitialPosition(void);
	void ParseFile (void);
	std::string GetFileName(void)
	{
		return fileName;
	}

private:
	mutable std::vector<Vector>::const_iterator m_current;
	std::vector<Time>::const_iterator m_current_time;
	std::string fileName;
	uint32_t fileId;
	std::vector<Vector> m_positions;
	std::vector<Time> m_time;

};
}
#endif //MOBITRADE_POSITION_ALLOCATOR_H