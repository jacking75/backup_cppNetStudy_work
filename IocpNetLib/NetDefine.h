#pragma once

#define WIN32_LEAN_AND_MEAN 
#include <WinSock2.h>

#include "CommonDefine.h"


namespace NetLib
{
	struct NetConfig
	{
		UINT16 PortNumber = 0;
		INT32 WorkThreadCount = INVALID_VALUE;
		INT32 MaxRecvOverlappedBufferSize = INVALID_VALUE;
		INT32 MaxSendOverlappedBufferSize = INVALID_VALUE;
		INT32 MaxRecvConnectionBufferCount = INVALID_VALUE;
		INT32 MaxSendConnectionBufferCount = INVALID_VALUE;
		INT32 MaxPacketSize = INVALID_VALUE;
		INT32 MaxConnectionCount = INVALID_VALUE;
		INT32 MaxMessagePoolCount = INVALID_VALUE;
		INT32 ExtraMessagePoolCount = INVALID_VALUE;
		INT32 PerformancePacketMillisecondsTime = INVALID_VALUE;
		INT32 PostMessagesThreadsCount = INVALID_VALUE;
	};

	struct ConnectionNetConfig
	{
		INT32 MaxRecvConnectionBufferCount;
		INT32 MaxSendConnectionBufferCount;
		INT32 MaxRecvOverlappedBufferSize;
		INT32 MaxSendOverlappedBufferSize;
	};



	enum class MessageType : INT8
	{
		None = 0,
		Connection,
		Close,
		OnRecv
	};

	enum class OperationType : INT8
	{
		None = 0,

		Send,
		Recv,
		Accept,
	};

	struct OVERLAPPED_EX
	{
		OVERLAPPED Overlapped;
		WSABUF OverlappedExWsaBuf;

		OperationType OverlappedExOperationType;

		int	OverlappedExTotalByte;
		DWORD OverlappedExRemainByte;
		char* pOverlappedExSocketMessage;

		INT32 ConnectionIndex = 0;
		//TODO ConnectionUnique 추가하자

		OVERLAPPED_EX(INT32 connectionIndex)
		{
			ZeroMemory(this, sizeof(OVERLAPPED_EX));
			ConnectionIndex = connectionIndex;
		}
	};


	struct Message
	{
		MessageType Type = MessageType::None;
		char* pContents = nullptr;

		void Clear()
		{
			Type = MessageType::None;
			pContents = nullptr;
		}

		void SetMessage(MessageType msgType, char* pSetContents)
		{
			Type = msgType;
			pContents = pSetContents;
		}
	};

	const int MAX_IP_LENGTH = 20;
	const int MAX_ADDR_LENGTH = 64;
}