#include "ns3/random-variable.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "MobiTradePositionAllocator.h"

#include <cmath>
#include <iostream>
#include <fstream>
#include <cstring>
using namespace std;

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED (MobiTradePositionAllocator);

TypeId MobiTradePositionAllocator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MobiTradePositionAllocator")
    .SetParent<PositionAllocator> ()
    .AddConstructor<MobiTradePositionAllocator> ()
    .AddAttribute ("FileNumber",
                   "Number of the file",
		   UintegerValue (50),
		   MakeUintegerAccessor (&MobiTradePositionAllocator::fileId),
		   MakeUintegerChecker <uint32_t> ())
    ;
  return tid;
}


MobiTradePositionAllocator::MobiTradePositionAllocator()
{

}

MobiTradePositionAllocator::~MobiTradePositionAllocator()
{
	m_positions.clear();
	m_time.clear();
}

void MobiTradePositionAllocator::Add (Vector v)
{
	m_positions.push_back (v);
}

void MobiTradePositionAllocator::LoadInitialPosition(void)
{
	try
	{
		char* tmpFileName = new char[256];
		if(USE_KAIST_TRACES)
			sprintf (tmpFileName, "/home/amir/KAIST/Converted/KAIST_30sec_%d.txt", fileId);
		else sprintf (tmpFileName, "/home/amir/home/KAIST/HCMM/node_%d", fileId);

		cout << "Reading initial position from file:" << tmpFileName << "\n";

		ifstream file;
		file.open(tmpFileName);

		string line;
		file >> line;
		double tmpTime = 0;
		double tmpX = 0;
		double tmpY = 0;
		sscanf(line.c_str(), "%lf\t%lf\t%lf\n", &tmpTime, &tmpX, &tmpY);

		m_time.push_back((Time) Seconds(tmpTime));

		Vector v;
		v.x = tmpX;
		v.y = tmpY;
		v.z = 0;

		m_positions.push_back(v);

		file.close();
		delete [] tmpFileName;
	}
	catch (exception &e)
	{
		cerr<<"Exception ocuured @ MobiTradePositionAllocator::LoadInitialPosition: "<<e.what()<<endl;
		exit(1);
	}
}

Vector MobiTradePositionAllocator::GetNext (void) const
{
	Vector v = *m_current;
	m_current ++;
	if(m_current == m_positions.end())
	{
		m_current = m_positions.begin();
	}
	return v;
}


void MobiTradePositionAllocator::AddTime (Time t)
{
	m_time.push_back (t);
}


Time MobiTradePositionAllocator::GetNextTime (void)
{
	Time t = *m_current_time;
	m_current_time++;
	if (m_current_time == m_time.end ())
	{
		m_current_time = m_time.begin ();
	}
	return t;
}

void MobiTradePositionAllocator::ParseFile (void)
{

try
{
  char* tmpFileName = new char[256];
  if(USE_KAIST_TRACES)
	  sprintf (tmpFileName, "/home/amir/KAIST/Converted/KAIST_30sec_%d.txt", fileId);
  else   sprintf (tmpFileName, "/home/amir/KAIST/HCMM/node_%d", fileId);

  cout << "file name is " << tmpFileName << "\n";
  fileName.assign(tmpFileName);
  ifstream file;
  file.open(tmpFileName);
  char line[512];
  memset(line, 512, '\0');
  while (!file.getline(line, 512, '\n').eof())
  {
	  double tmpTime = 0;
	  double tmpX = 0;
	  double tmpY = 0;
	  sscanf(line, "%lf\t%lf\t%lf\n", &tmpTime, &tmpX, &tmpY);
	  // Adding time stamps
	  m_time.push_back((Time) Seconds(tmpTime));
	  // Adding positions
	  Vector v;
	  v.x = tmpX;
	  v.y = tmpY;
	  v.z = 0;
	  m_positions.push_back(v);
	  // Clearing the tmp line
	  memset(line, 512, '\0');
  }

  file.close();

  // Setting the current time and position
  m_current_time = m_time.begin ();
  m_current = m_positions.begin ();
  delete[] tmpFileName;
}

catch(exception &e)
{
	cerr<<"Exception occurred @ MobiTradePositionAllocator::ParseFile: "<<e.what()<<endl;
	exit(1);
}

}
}
