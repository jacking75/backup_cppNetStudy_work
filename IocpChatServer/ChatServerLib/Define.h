#pragma once


#include "../../IocpNetLib/NetDefine.h"


namespace ChatServerLib
{
	struct ChatServerConfig : NetLib::NetConfig
	{
		int StartRoomNummber = 0;
		int MaxRoomCount = 0;
		int MaxRoomUserCount = 0;

		NetLib::NetConfig GetNetConfig()
		{
			NetLib::NetConfig netConfig;

			netConfig.PortNumber = PortNumber; 
			netConfig.WorkThreadCount = WorkThreadCount;
			netConfig.MaxRecvOverlappedBufferSize = MaxRecvOverlappedBufferSize;
			netConfig.MaxSendOverlappedBufferSize = MaxSendOverlappedBufferSize;
			netConfig.MaxRecvConnectionBufferCount = MaxRecvConnectionBufferCount;
			netConfig.MaxSendConnectionBufferCount = MaxSendConnectionBufferCount;
			netConfig.MaxPacketSize = MaxPacketSize;
			netConfig.MaxConnectionCount = MaxConnectionCount;
			netConfig.MaxMessagePoolCount = MaxMessagePoolCount;
			netConfig.ExtraMessagePoolCount = ExtraMessagePoolCount;
			netConfig.PerformancePacketMillisecondsTime = PerformancePacketMillisecondsTime;
			netConfig.PostMessagesThreadsCount = PostMessagesThreadsCount;
			
			return netConfig;
		}






	};

}