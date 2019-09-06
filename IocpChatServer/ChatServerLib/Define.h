#pragma once


#include "../../IocpNetLib/NetDefine.h"


namespace ChatServerLib
{
	struct ChatServerConfig : NetLib::NetConfig
	{
		int StartRoomNummber;
		int MaxRoomCount;

		NetLib::NetConfig GetNetConfig()
		{
			NetLib::NetConfig netConfig;

			netConfig.m_PortNumber = m_PortNumber; 
			netConfig.m_WorkThreadCount = m_WorkThreadCount;
			netConfig.m_MaxRecvOverlappedBufferSize = m_MaxRecvOverlappedBufferSize;
			netConfig.m_MaxSendOverlappedBufferSize = m_MaxSendOverlappedBufferSize;
			netConfig.m_MaxRecvConnectionBufferCount = m_MaxRecvConnectionBufferCount;
			netConfig.m_MaxSendConnectionBufferCount = m_MaxSendConnectionBufferCount;
			netConfig.m_MaxPacketSize = m_MaxPacketSize;
			netConfig.m_MaxConnectionCount = m_MaxConnectionCount;
			netConfig.m_MaxMessagePoolCount = m_MaxMessagePoolCount;
			netConfig.m_ExtraMessagePoolCount = m_ExtraMessagePoolCount;
			netConfig.m_PerformancePacketMillisecondsTime = m_PerformancePacketMillisecondsTime;
			netConfig.m_PostMessagesThreadsCount = m_PostMessagesThreadsCount;
			
			return netConfig;
		}






	};

}