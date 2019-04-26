#include "Filter.h"

CSSFilter::CSSFilter(std::string &name)
{
	m_strFilterName = name;
	m_listConnectNext.clear();
	m_listConnectFront.clear();
}

CSSFilter::~CSSFilter()
{
	m_listConnectNext.clear();
	m_listConnectFront.clear();
}

int CSSFilter::AddFrontFilter(CSSFilter *dst)
{
	if (dst)
	{
		std::lock_guard< std::mutex> lock(m_MutexConnectFront);
		std::deque<CSSFilter*>::iterator iter = find(m_listConnectFront.begin(), m_listConnectFront.end(), dst);
		if (iter != m_listConnectFront.end())
		{
			return -1;
		}
		else
		{
			m_listConnectFront.push_back(dst);
			return 0;
		}
	}
	else
	{
		return -1;
	}
}

int CSSFilter::ConnectFilter(CSSFilter*dst)
{
	if (!dst)
	{
		return -1;
	}
	std::lock_guard< std::mutex> lock(m_MutexConnectNext);
	std::deque<CSSFilter*>::iterator iter = find(m_listConnectNext.begin(), m_listConnectNext.end(), dst);
	if (iter != m_listConnectNext.end())
	{
		return -1;
	}
	else
	{
		dst->AddFrontFilter(this);
		m_listConnectNext.push_back(dst);
		
		return 0;
	}
}

int CSSFilter::DisConncetFilter(CSSFilter *filter)
{
	if (!filter)
	{
		return -1;
	}
}

void CSSFilter::DeliverData(CFrameSharePtr &frame)
{
	std::lock_guard< std::mutex> lock(m_MutexConnectNext);
	std::deque<CSSFilter*>::iterator iter = m_listConnectNext.begin();
	for (; iter != m_listConnectNext.end(); iter++)
	{
		(*iter)->InputData(frame);
	}
	
}