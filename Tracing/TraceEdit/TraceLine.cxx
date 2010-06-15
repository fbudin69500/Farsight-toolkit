/*=========================================================================
Copyright 2009 Rensselaer Polytechnic Institute
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 
=========================================================================*/

#include <iostream>
#include <math.h>
#include <vtksys/hash_map.hxx>
#include "tinyxml/tinyxml.h"

#define VTK_CREATE(type, var) \
  vtkSmartPointer<type> var = vtkSmartPointer<type>::New()

#define MY_ENCODING "ISO-8859-1"

#include "TraceBit.h"
#include "TraceLine.h"

///////////////////////////////////////////////////////////////////////////////
TraceLine::TraceLine()
{
  this->m_parent = NULL;
  this->root= -1;
  this->level = 0;
  this->m_id = -(1<<30);
  this->m_branches.clear();
  this->EuclidianD = -1;
}

///////////////////////////////////////////////////////////////////////////////
TraceLine::~TraceLine()
{
}

///////////////////////////////////////////////////////////////////////////////
TraceLine* TraceLine::GetParent()
{
  return this->m_parent;
}
int TraceLine::GetParentID()
{
	if (this->m_parent)
	{
		return this->m_parent->GetId();
	}
	else 
	{
		return -1;
	}
}

///////////////////////////////////////////////////////////////////////////////
void TraceLine::SetParent(TraceLine* p)
{
  this->m_parent = p;
}
int TraceLine::GetRootID()
{
	return this->root;
}
int TraceLine::GetLevel()
{
	return this->level;
}
bool TraceLine::isLeaf()
{
	if (this->m_branches.size() ==0)
	{
		return true;
	}
	else
	{
		return false;
	}
}
bool TraceLine::isRoot()
{
	if (!this->m_parent)
	{
		return true;
	}
	else
	{
		return false;
	}
}
bool TraceLine::isFree()
{
	if (this->isRoot() && this->isLeaf())
	{
		return true;
	}
	else
	{
		return false;
	}
}
void TraceLine::setRoot(int RootID, int traceLevel, double parentPath)
{
	this->root = RootID;
	this->level = traceLevel;
	this->PathLength = parentPath + this->GetLength();
}
void TraceLine::calculateVol()
{
	double dist = 0, r = 0;
	TraceBit pre, cur;
	TraceBitsType::iterator it = this->m_trace_bits.begin();
	pre = *it; 
	it++;
	r = pre.r;
	for (; it != this->m_trace_bits.end(); it++)
	{
		cur = *it;
		r += cur.r;
		dist += Euclidian(pre, cur);
		pre = cur;
	}
	this->length = dist;
	this->radii = r / this->m_trace_bits.size(); //ave radii
	this->volume = pow((this->radii),2)*this->length;
}
void TraceLine::setTraceBitIntensities(vtkSmartPointer<vtkImageData> imageData)
{
	TraceBit curBit;
	TraceBitsType::iterator it = this->m_trace_bits.begin();
	for (; it != this->m_trace_bits.end(); it++)
	{
		int lx = 0, ly = 0, lz = 0; 
		curBit = *it;
		lx = (int) floor(curBit.x + 0.5);
		ly = (int) floor(curBit.y + 0.5);
		lz = (int) floor(curBit.z + 0.5);
		(*it).I = imageData->GetScalarComponentAsDouble(lx,ly,lz,0);
		std::cout<< "\nid:\t"<< (*it).id << "\tI:\t" << (*it).I;
	}
	//should have ended
}
double TraceLine::GetEuclidianLength()
{
	if (this->m_trace_bits.size() <2)
	{
		this->EuclidianD =0;
	}
	else
	{
		TraceBit front = this->m_trace_bits.front();
		TraceBit back  = this->m_trace_bits.back();
		this->EuclidianD = this->Euclidian(front, back);
	}
	return this->EuclidianD;
}
double TraceLine::GetBitDensity()
{
	this->BitDensity = this->GetSize() / this->GetLength();
	return this->BitDensity;
}
double TraceLine::GetDistToParent()
{
	if (this->m_parent)
	{
		this->DistToParent = this->Euclidian(this->m_trace_bits.front(), 
			this->m_parent->m_trace_bits.back());
		if (this->m_trace_bits.size()>1)
		{
			double Leading = 0, dist =0;
			TraceBit pre, cur;
			TraceBitsType::iterator it = this->m_trace_bits.begin();
			pre = *it; 
			it++;
			cur = *it;
			Leading = Euclidian(pre, cur);
			if (Leading > 2*this->DistToParent)
			{
				this->DistToParent = Leading;
			}
		}
		return this->DistToParent;
	}
	else
	{
		this->DistToParent = 0;
		return -1;
	}
}
double TraceLine::GetFragmentationSmoothness()
{
	if (!(this->EuclidianD > -1))
	{
		this->GetEuclidianLength();
	}
	double t = -1;
	if ( this->m_trace_bits.size() > 1)
	{
		t = this->length/this->EuclidianD;
	}
	return t;
}
///////////////////////////////////////////////////////////////////////////////
void TraceLine::AddBranch(TraceLine* b)
{ 
  this->m_branches.push_back(b);
}


///////////////////////////////////////////////////////////////////////////////
TraceLine* TraceLine::GetBranch1()
{
  return this->m_branches[0];
}

///////////////////////////////////////////////////////////////////////////////
void TraceLine::SetBranch1(TraceLine* b0)
{
  this->m_branches[0] = b0;
}

///////////////////////////////////////////////////////////////////////////////
TraceLine* TraceLine::GetBranch2()
{
  return this->m_branches[1];
}

///////////////////////////////////////////////////////////////////////////////
void TraceLine::SetBranch2(TraceLine* b1)
{
  this->m_branches[1] = b1;
}

///////////////////////////////////////////////////////////////////////////////
unsigned char TraceLine::GetType()
{
  return this->m_type;
}

///////////////////////////////////////////////////////////////////////////////
void TraceLine::SetType(unsigned char t)
{
  this->m_type = t;
}

///////////////////////////////////////////////////////////////////////////////
void TraceLine::AddTraceBit(TraceBit tbit)
{
  this->m_trace_bits.push_back(tbit);
}
void TraceLine::ExtendTrace(TraceBit tbit)
{
	bool front = false;
	if (this->m_trace_bits.size()>1)
	{
		if (!this->Orient(tbit))
		{
			this->m_trace_bits.push_front(tbit);
			front = true;
		}
	}
	if (!front)
	{
		this->m_trace_bits.push_back(tbit);
	}
}
TraceBit TraceLine::removeLastBit()
{
	TraceBit lastBit = this->m_trace_bits.back();
	if (this->m_trace_bits.size() > 1)
	{
		this->m_trace_bits.pop_back();
	}
	return lastBit;
}
TraceBit TraceLine::removeFirstBit()
{
	TraceBit firstBit = this->m_trace_bits.front();
	if (this->m_trace_bits.size() > 1)
	{
		this->m_trace_bits.pop_front();
	}
	return firstBit;
}

//This is an error fix
bool TraceLine::removeLeadingBit()
{
	if (this->m_trace_bits.size() < 3)
	{
		return false;
	}else
	{		
		//std::vector<double> dist;// = 0, r = 0;
		double Leading = 0, dist =0;
		TraceBit pre, cur;
		TraceBitsType::iterator it = this->m_trace_bits.begin();
		pre = *it; 
		it++;
		cur = *it;
		Leading = Euclidian(pre, cur);
		it++;
		for (; it != this->m_trace_bits.end(); it++)
		{
			cur = *it;
			dist += this->Euclidian(pre, cur);
			pre = cur;
		}//end for
		if(Leading > (dist/ (this->m_trace_bits.size() -1)))
		{
			this->m_trace_bits.pop_front();
			return true;
		}//end dist
		return false;
	}//end else size
}
///////////////////////////////////////////////////////////////////////////////
TraceLine::TraceBitsType::iterator TraceLine::GetTraceBitIteratorBegin()
{
  return this->m_trace_bits.begin();
}
TraceBit TraceLine::GetBitXFromBegin(int x)
{
	TraceBit curBit;
	TraceBitsType::iterator iter = this->m_trace_bits.begin();
	TraceBitsType::iterator iterend = this->m_trace_bits.end();
	if (3 > (int)this->m_trace_bits.size())
	{
		--iterend;
		curBit = * iterend;
		//return  curBit;
	}
	else
	{
		int i = 0;
		while(( i< (int)(this->m_trace_bits.size()-2))&&( i < x ))
		{
			++iter;
			i++;
		}
		curBit = *iter;
	}
	return curBit;
}
///////////////////////////////////////////////////////////////////////////////
TraceLine::TraceBitsType::iterator TraceLine::GetTraceBitIteratorEnd()
{
  return this->m_trace_bits.end();
}

TraceBit TraceLine::GetBitXFromEnd(int x)
{
	TraceBit curBit;
	TraceBitsType::iterator iter = this->m_trace_bits.begin();
	TraceBitsType::iterator iterend = this->m_trace_bits.end();
	if (3 >(int)this->m_trace_bits.size())
	{
		curBit = * iter;
		
	}
	else
	{
		int i = 0;
		while(( i<(int)(this->m_trace_bits.size()-2))&&( i < x ))
		{
			--iterend;
			i++;
		}
		curBit = *iterend;
	}
	return curBit;
}
///////////////////////////////////////////////////////////////////////////////
TraceLine::TraceBitsType* TraceLine::GetTraceBitsPointer()
{
  return &m_trace_bits;
}

///////////////////////////////////////////////////////////////////////////////
void TraceLine::SetId(int lid){ m_id = lid;}

///////////////////////////////////////////////////////////////////////////////
int TraceLine::GetId()
{ 
  return m_id;
};

///////////////////////////////////////////////////////////////////////////////
int TraceLine::GetSize()
{
  return (int)m_trace_bits.size();
};

///////////////////////////////////////////////////////////////////////////////
void TraceLine::Print(std::ostream &c,int indent)
{
  for(int counter=0; counter<indent; counter++)
	  c<<" ";
  c<<"TraceLine: "<<std::endl;
  for(int counter=0; counter<indent; counter++)
	  c<<" ";
  c<<"Size: "<<m_trace_bits.size()<<std::endl;
  for(unsigned int counter=0; counter< m_branches.size(); counter++)
  {
	  m_branches[counter]->Print(std::cout,indent+4);
  }
}

///////////////////////////////////////////////////////////////////////////////
std::vector<unsigned int> * TraceLine::GetMarkers()
{
  return &(this->m_markers);
}

///////////////////////////////////////////////////////////////////////////////
std::vector<TraceLine*> * TraceLine::GetBranchPointer()
{
  return &(this->m_branches);
}

///////////////////////////////////////////////////////////////////////////////
void TraceLine::setTraceColor(double newColor)
{
  this->traceColor = newColor;
};

///////////////////////////////////////////////////////////////////////////////
double TraceLine::getTraceColor()
{
  return this->traceColor;
};

///////////////////////////////////////////////////////////////////////////////
 TraceLine::TraceLine(const TraceLine &t)
{
  this->traceColor = t.traceColor;
  this->m_id = t.m_id;
  this->m_markers = t.m_markers;
  this->m_type = t.m_type;
  this->m_parent = NULL;
  for(unsigned int counter=0; counter< t.m_branches.size(); counter++)
  {
    TraceLine *temp = new TraceLine();
    temp->SetParent(this);
    this->AddBranch(temp);
    *temp = *t.m_branches[counter];
  }
  this->m_trace_bits = t.m_trace_bits;
}

///////////////////////////////////////////////////////////////////////////////
void TraceLine::Getstats()
{
  double XF, XB, YF, YB, ZF, ZB;
  XF = m_trace_bits.front().x;
  XB = m_trace_bits.back().x;
  YF= m_trace_bits.front().y;
  YB= m_trace_bits.back().y;
  ZF= m_trace_bits.front().z;
  ZB= m_trace_bits.back().z;
  printf("Trace # %d \t Trace Size: \t %d ", m_id, (int)m_trace_bits.size());
  printf("First bit x: %4.2f y: %4.2f z: %4.2f \t", XF, YF, ZF); 
  printf("Endt bit x: %4.2f y: %4.2f z: %4.2f \n", XB, YB, ZB); 
}


///////////////////////////////////////////////////////////////////////////////
double TraceLine::Euclidian(TraceBit bit1, TraceBit bit2)
{
	double distance, x, y, z;
	x = pow((bit1.x -bit2.x),2);
	y = pow((bit1.y -bit2.y),2);
	z = pow((bit1.z -bit2.z),2);
	distance = sqrt(x +y +z);
	return distance;

}
double TraceLine::Angle(TraceBit bit1f, TraceBit bit1b, TraceBit bit2f, TraceBit bit2b)
{
	double delX1, delX2, delY1, delY2, delZ1, delZ2,  norm1, norm2, angle;
	//delta x,y,z 
	angle = 0;
	delX1= fabs (bit1f.x-bit1b.x);	delX2= fabs (bit2f.x-bit2b.x);
	delY1= fabs (bit1f.y-bit1b.y);	delY2= fabs (bit2f.y-bit2b.y);
	delZ1= fabs (bit1f.z-bit1b.z);	delZ2= fabs (bit2f.z-bit2b.z);
	norm1=sqrt(pow((delX1),2)+ pow((delY1),2)+ pow((delZ1),2));
	norm2=sqrt(pow((delX2),2)+ pow((delY2),2)+ pow((delZ2),2));
	angle = acos(((delX1 * delX2) + (delY1 *delY2) + (delZ1 * delZ2)) /
          (norm2 * norm1));
	if (!(angle >= 0))
	{//prevent nan rounding errors
		angle = 0;
	}
	return angle;
}
bool TraceLine::EndPtDist(TraceLine *Trace2, int &dir1, int &dir2, double &dist,
                          double &maxdist, double &angle) 
{
  //int center1=this->GetSize()/2, center2=Trace2->GetSize()/2;
  double min, distances[4];

  int Xbits = 10; //small step from end to determine 
  //compute the endpt distances
  distances[0]=Euclidian(m_trace_bits.front(), Trace2->m_trace_bits.front());//0 F-F
  distances[1]=Euclidian(m_trace_bits.front(), Trace2->m_trace_bits.back());//1 F-B
  distances[2]=Euclidian(m_trace_bits.back(), Trace2->m_trace_bits.front());//2 B-F
  distances[3]=Euclidian(m_trace_bits.back(), Trace2->m_trace_bits.back());//3 B-B

  //determine minimum spacing
  min = distances[0];
  int i, mark=0;
  for (i = 1; i<4; i++)
    {
    if (min > distances[i])
      {
      min= distances[i];
      mark=i;
      }
    } 
// std::cout << mark << "=mark\n";
  // from min determine orientation and return distance
  if (mark ==0)
    {//F-F
		//std::cout <<"FF\n";
	if (this->m_branches.size() > 0 && Trace2->m_branches.size()> 0)
		{return false;}
	if(this->GetParentID()!=-1 || Trace2->GetParentID()!=-1)
	{return false;}
    dist = distances[0];
    dir1= m_trace_bits.front().marker;
    dir2= Trace2->m_trace_bits.front().marker; 
	angle = Angle(m_trace_bits.front(),this->GetBitXFromBegin(Xbits), 
		Trace2->m_trace_bits.front(),Trace2->GetBitXFromBegin(Xbits));
    //angle=PI-angle;
    maxdist= distances[3];
    }
  else if (mark ==1)
    {//1 F-B
		//std::cout <<"FB\n";
	if ((this->GetParentID() != -1)||!Trace2->isLeaf())
		{return false;}
    dist = distances[1];
    dir1= m_trace_bits.front().marker;
    dir2= Trace2->m_trace_bits.back().marker; 
	angle = Angle(m_trace_bits.front(),this->GetBitXFromBegin(Xbits), 
		Trace2->GetBitXFromEnd(Xbits),Trace2->m_trace_bits.back());
    maxdist= distances[2];
    }
  else if (mark ==2)
    {//2 B-F
		//std::cout <<"BF\n";
		if (!this->isLeaf() || (Trace2->GetParentID()!=-1))
		{return false;}
    dist = distances[2];
    dir1= m_trace_bits.back().marker;
    dir2= Trace2->m_trace_bits.front().marker; 
	angle = Angle(this->GetBitXFromEnd(Xbits),m_trace_bits.back(), 
		Trace2->m_trace_bits.front(),Trace2->GetBitXFromBegin(Xbits));
    maxdist= distances[1];
    }
  else
    {//3 B-B
		//std::cout <<"BB\n";
	if (!this->isLeaf() || !Trace2->isLeaf()) 
		{return false;}
    dist = distances[3];
    dir1= m_trace_bits.back().marker;
    dir2= Trace2->m_trace_bits.back().marker;
	angle = Angle(this->GetBitXFromEnd(Xbits),m_trace_bits.back(), 
		Trace2->GetBitXFromEnd(Xbits),Trace2->m_trace_bits.back());
    //angle=PI-angle;
    maxdist= distances[0];
    }
  return true;
}
bool TraceLine::Orient(TraceLine * Trunk)
{
	double distances[2];
	//compute the endpt distances
	distances[0]= Euclidian(m_trace_bits.front(),	Trunk->m_trace_bits.back());// F-B
	distances[1]= Euclidian(m_trace_bits.back(),	Trunk->m_trace_bits.back());// B-B
	if(distances[0] < distances[1])
	{
		return true;	//oriented correctly
	}
	return false;		//needs to be flipped
}
bool TraceLine::Orient(TraceBit bit)
{
	double distances[2];
	//compute the endpt distances
	distances[0]= Euclidian(m_trace_bits.front(),	bit);// F-Bit
	distances[1]= Euclidian(m_trace_bits.back(),	bit);// B-Bit
	if(distances[0] > distances[1])
	{
		return true;	//oriented correctly
	}
	return false;		//needs to be flipped
}
///////////////////////////////////////////////////////////////////////////////
std::string TraceLine::stats()
{
	std::stringstream thisStats;
	thisStats << this->GetId();
	thisStats << "\t";
	thisStats << (int)this->GetType();
	thisStats << "\t" ;
	thisStats << this->GetSize();
	thisStats << "\t";
	thisStats << this->GetLength();
	thisStats << "\t" ;
	thisStats << this->GetEuclidianLength();
	thisStats << "\t" ;
	thisStats << this->GetRadii();
	thisStats << "\t";
	thisStats << this->GetFragmentationSmoothness() ;
	thisStats << "\t";
	thisStats << this->GetParentID();
	return thisStats.str();
}
std::string TraceLine::statHeaders()
{
	std::stringstream thisStatsHeaders; 
	thisStatsHeaders <<"ID"
		<<"\tType"
		<<"\tSize"
		<<"\tLength"
		<<"\tEuclidian L"
		<<"\tRadii"
		<<"\tContraction"
		<<"\tParent ID";
	return thisStatsHeaders.str();
}
