#include "Frame.h"
CFrame::CFrame()
{
	m_eFrameType = eUnknowFrame;
	m_pData = 0;
	m_nWidth = 0;
	m_nHeight = 0;
	m_nLen = 0;
	m_ePixType = eUnknowPix;
	m_nAudioSampleRate = 0;
	m_nAudioChannel = 0;
	m_nTimesTamp = 0;
	m_nBitPerSample = 0;
	m_nShowTime = 0;
	m_nPixBits = 8;
}
CFrame::CFrame(CFrame&frame)
{
	m_eFrameType = frame.m_eFrameType;
	m_pData = 0;
	m_nWidth = frame.m_nWidth;
	m_nHeight = frame.m_nHeight;
	m_nLen = frame.m_nLen;
	m_ePixType = frame.m_ePixType;
	m_nAudioSampleRate = frame.m_nAudioSampleRate;
	m_nAudioChannel = frame.m_nAudioChannel;
	m_nTimesTamp = frame.m_nTimesTamp;
	m_nBitPerSample = frame.m_nBitPerSample;
	m_nShowTime = frame.m_nShowTime;
	m_nPixBits = frame.m_nPixBits;
}
CFrame::~CFrame()
{
	if (m_pData)
	{
		delete[]m_pData;
		m_pData = 0;
	}
}

bool CFrame::AllocMem(long nSize)
{
	if (m_pData)
	{
		FreeMem();
	}
	m_pData = new unsigned char[nSize];
	m_nLen = nSize;
	return true;
}
bool CFrame::FreeMem()
{
	if (m_pData)
	{
		delete[]m_pData;
		m_pData = 0;
	}
	return true;
}


CFrameSharePtrQueue::CFrameSharePtrQueue()
	:m_nTotalBytes(0)
{
}

CFrameSharePtrQueue::~CFrameSharePtrQueue()
{
	Clear();
}

bool CFrameSharePtrQueue::IsEmpty()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);
	return m_listFrameHolder.empty();
}

void CFrameSharePtrQueue::EnqueueFront(CFrameSharePtr& element)
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	m_listFrameHolder.push_front(element);
	m_nTotalBytes += element->m_nLen;
}

void CFrameSharePtrQueue::Enqueue(CFrameSharePtr& element)
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	m_listFrameHolder.push_back(element);
	m_nTotalBytes += element->m_nLen;
}

void CFrameSharePtrQueue::ForntQueue(CFrameSharePtr& element)
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	m_listFrameHolder.push_front(element);
	m_nTotalBytes += element->m_nLen;
}

CFrameSharePtr CFrameSharePtrQueue::Dequeue()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	if (m_listFrameHolder.empty())
	{
		return NULL;
	}

	CFrameSharePtr element = m_listFrameHolder.front();
	m_listFrameHolder.pop_front();

	m_nTotalBytes -= element->m_nLen;
	return element;
}

CFrameSharePtr CFrameSharePtrQueue::Front()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	if (m_listFrameHolder.empty())
	{
		return NULL;
	}

	CFrameSharePtr element = m_listFrameHolder.front();
	return element;
}

CFrameSharePtr CFrameSharePtrQueue::Back()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	if (m_listFrameHolder.empty())
	{
		return NULL;
	}

	CFrameSharePtr element = m_listFrameHolder.back();
	m_listFrameHolder.pop_back();

	m_nTotalBytes -= element->m_nLen;
	return element;
}

long long CFrameSharePtrQueue::GetBytes()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	return m_nTotalBytes;
}

int CFrameSharePtrQueue::Size()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	return m_listFrameHolder.size();
}

void CFrameSharePtrQueue::Clear()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);

	m_listFrameHolder.clear();
	m_nTotalBytes = 0;
}

long long CFrameSharePtrQueue::GetTimestampInterval()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);
	if (m_listFrameHolder.size() <= 1)
	{
		return 0;
	}

	long long timestamp_1 = m_listFrameHolder.back()->m_nTimesTamp;
	long long timestamp_2 = m_listFrameHolder.front()->m_nTimesTamp;

	long long timestamp = timestamp_1 - timestamp_2;
	return timestamp;
}

long long CFrameSharePtrQueue::FrontTimetemp()
{
	std::lock_guard< std::mutex> lock(m_stFrameListMutex);
	if (m_listFrameHolder.size() <= 0)
	{
		return 0;
	}
	CFrameSharePtr element = m_listFrameHolder.front();
	return element->m_nTimesTamp;
}
