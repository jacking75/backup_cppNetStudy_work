#pragma once

#include <vector>
#include <thread>
#include <mutex>

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include "CommonDefine.h"
#include "Performance.h"
#include "Connection.h"
#include "MessagePool.h"
#include "MiniDump.h"

// 코어수 이상의 iocp 객체를 가지고 각 객체는 특정 IOCP에서만 처리되도록 한다.
// 이렇게 해서 세션은 멀티스레드 세이프하게 동작하도록 한다.

namespace NetLib
{
	//TODO 모든 세션을 주기적으로 조사 기능 구현하기
	// - 연결이 끊어졌는데 재 사용하고 있는지 조사하기
	// - PostQueuedCompletionStatus가 실패 했을 때 뒤에서 관련 에러를 복구하는 기능이 필요
	
	//TODO 보내기도 PostQueuedCompletionStatus를 사용하여 해당 워크 스레드에서 처리하도록 한다.
	// 애플리케이션에서 Send를 호출하면 버퍼에 복사 후 PostQueuedCompletionStatus로 세션에 연결된 IOCP로 메시지를 보내고 여기서 WSASend를 호출하도록 한다.

	//TODO iocp 스레드는 무한 대기하지 않고 쓰기 간격만큼 깨어난다(?)
	// acceptThread는 특정 간격마다 종료된 세션을 비동기 accept 호출한다
	// workThread는 특정 간격마다 보내기 처리를 한다.
	// !! 아니다 해당 iocp 스레드에 postQueue를 보내서 처리하도록 한다. 즉 스레드가 메시지가 있을 때까지 깨어나지 않도록 한다. 앗 그러나 accept의 경우 특정 시간이 지난 후 처리하고 싶기 때문에 메시지 전달로 하기는 힘들듯
	

	
	struct OVERLAPPED_EX;
	class Connection;
	//class MessagePool;


	class IOCPServerNet
	{
	public:
		IOCPServerNet() {}
		~IOCPServerNet() {}

	public:
		virtual NetResult Start(NetConfig netConfig)
		{
			LogFuncPtr((int)LogLevel::Info, "Start Server Completion");
						
			m_NetConfig = netConfig;
			
			auto result = CreateListenSocket();
			if (result != NetResult::Success)
			{
				return result;
			}

			result = CreateHandleIOCP(netConfig.WorkThreadCount);
			if (result != NetResult::Success)
			{
				return result;
			}

			if (!CreateMessageManager())
			{
				return NetResult::Fail_Create_Message_Manager;
			}

			if (!LinkListenSocketIOCP())
			{
				return NetResult::Fail_Link_IOCP;
			}

			if (!CreateConnections())
			{
				return NetResult::Fail_Create_Connection;
			}

			if (!CreatePerformance())
			{
				return NetResult::Fail_Create_Performance;
			}

			if (!CreateWorkThread())
			{
				return NetResult::Fail_Create_WorkThread;
			}

			LogFuncPtr((int)LogLevel::Info, "Start Server Completion");
			return NetResult::Success;
		}

		virtual void End()
		{
			LogFuncPtr((int)LogLevel::Info, "IOCPServer::EndServer - Start");

			m_IsRunWorkThread = false;
						
			if (m_hAcceptWorkIOCP != INVALID_HANDLE_VALUE)
			{
				CloseHandle(m_hAcceptWorkIOCP);

				if (m_AccetpThread.joinable())
				{
					m_AccetpThread.join();
				}				
			}


			auto index = 0;
			for (auto handel : m_WrokIOCPList)
			{
				CloseHandle(handel);

				if (m_WorkThreads[index].joinable())
				{
					m_WorkThreads[index].join();
				}

				++index;
			}
			m_WrokIOCPList.clear();
	

			if (m_hLogicIOCP != INVALID_HANDLE_VALUE)
			{
				//PostQueuedCompletionStatus(m_hLogicIOCP, 0, 0, nullptr);
				CloseHandle(m_hLogicIOCP);
			}

			if (m_ListenSocket != INVALID_SOCKET)
			{
				closesocket(m_ListenSocket);
				m_ListenSocket = INVALID_SOCKET;
			}

			WSACleanup();


			DestoryConnections();
						
			LogFuncPtr((int)LogLevel::Info, "IOCPServer::EndServer - Completion");
		}

		
		bool ProcessNetworkMessage(OUT INT8& msgOperationType, OUT INT32& connectionIndex, char* pBuf, OUT INT16& copySize, const INT32 waitTimeMillisec)
		{
			Message* pMsg = nullptr;
			Connection* pConnection = nullptr;
			DWORD ioSize = 0;
			auto waitTime = waitTimeMillisec;

			if (waitTime == 0)
			{
				waitTime = INFINITE;
			}
			
			if(auto result = GetQueuedCompletionStatus(
				m_hLogicIOCP,
				&ioSize,
				reinterpret_cast<PULONG_PTR>(&pConnection),
				reinterpret_cast<LPOVERLAPPED*>(&pMsg),
				waitTime); result == false)
			{
				return false;
			}

			switch (pMsg->Type)
			{
			case MessageType::Connection:
			{
				msgOperationType = (INT8)pMsg->Type;
				connectionIndex = pConnection->GetIndex();
			}
				break;
			case MessageType::Close:
			{
				msgOperationType = (INT8)pMsg->Type;
				connectionIndex = pConnection->GetIndex();

				pConnection->ResetConnection();
			}
				break;
			case MessageType::OnRecv:
			{
				ForwardingNetOnRecvMsgToAppLayer(pConnection, pMsg, msgOperationType, connectionIndex, pBuf, copySize, ioSize);
				m_pMsgPool->DeallocMsg(pMsg);
			}
				break;
			}
						
			return true;
		}

		void SendPacket(const INT32 connectionIndex, const void* pSendPacket, const INT16 packetSize)
		{
			auto pConnection = GetConnection(connectionIndex);
			if (pConnection == nullptr)
			{
				return;
			}
			
			char* pReservedSendBuf = nullptr;
			auto result = pConnection->ReservedSendPacketBuffer(&pReservedSendBuf, packetSize);
			if (result == NetResult::ReservedSendPacketBuffer_Not_Connected)
			{
				return;
			}
			else if (result == NetResult::ReservedSendPacketBuffer_Empty_Buffer)
			{
				pConnection->DisConnectAsync();
				return;
			}

			CopyMemory(pReservedSendBuf, pSendPacket, packetSize);

			if (pConnection->PostSend(packetSize) == false)
			{
				pConnection->DisConnectAsync();
			}
		}

		int GetMaxPacketSize() { return m_NetConfig.MaxPacketSize; }
		int GetMaxConnectionCount() { return m_NetConfig.MaxConnectionCount; }
		

	private:		
		NetResult CreateListenSocket()
		{
			WSADATA wsaData;
			auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (result != 0)
			{
				return NetResult::fail_create_listensocket_startup;
			}

			m_ListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
			if (m_ListenSocket == INVALID_SOCKET)
			{
				return NetResult::fail_create_listensocket_socket;
			}

			SOCKADDR_IN	addr;
			ZeroMemory(&addr, sizeof(SOCKADDR_IN));

			addr.sin_family = AF_INET;
			addr.sin_port = htons(m_NetConfig.PortNumber);
			addr.sin_addr.s_addr = htonl(INADDR_ANY);

			if (::bind(m_ListenSocket, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR)
			{
				return NetResult::fail_create_listensocket_bind;
			}

			if (::listen(m_ListenSocket, SOMAXCONN) == SOCKET_ERROR)
			{
				return NetResult::fail_create_listensocket_listen;
			}

			return NetResult::Success;
		}

		NetResult CreateHandleIOCP(const int threadCount)
		{
			m_hAcceptWorkIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
			if (m_hAcceptWorkIOCP == INVALID_HANDLE_VALUE)
			{
				return NetResult::fail_handleiocp_accept;
			}

			for (int i = 0; i < threadCount; ++i)
			{
				auto handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
				if (handle == INVALID_HANDLE_VALUE)
				{
					return NetResult::fail_handleiocp_work;
				}

				m_WrokIOCPList.push_back(handle);
			}

			m_hLogicIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
			if (m_hLogicIOCP == INVALID_HANDLE_VALUE)
			{
				return NetResult::fail_handleiocp_logic;
			}

			return NetResult::Success;
		}

		bool CreateMessageManager()
		{
			m_pMsgPool = std::make_unique<MessagePool>(m_NetConfig.MaxMessagePoolCount, m_NetConfig.ExtraMessagePoolCount); 
			if (!m_pMsgPool->CheckCreate())
			{
				return false;
			}

			return true;
		}

		bool LinkListenSocketIOCP()
		{
			auto hIOCPHandle = CreateIoCompletionPort(
				reinterpret_cast<HANDLE>(m_ListenSocket),
				m_hAcceptWorkIOCP,
				0,
				0);

			if (hIOCPHandle == INVALID_HANDLE_VALUE || m_hAcceptWorkIOCP != hIOCPHandle)
			{
				return false;
			}

			return true;
		}


		bool CreateConnections()
		{
			ConnectionNetConfig config;
			config.MaxSendBufferSize = m_NetConfig.ConnectionMaxSendBufferSize;
			config.MaxRecvBufferSize = m_NetConfig.ConnectionMaxRecvBufferSize;
			config.MaxRecvOverlappedBufferSize = m_NetConfig.MaxRecvOverlappedBufferSize;
			config.MaxSendOverlappedBufferSize = m_NetConfig.MaxSendOverlappedBufferSize;

			for (int i = 0; i < m_NetConfig.MaxConnectionCount; ++i)
			{
				auto iocpIndex = ConnectionWorkIOCPIndex(i, m_NetConfig.WorkThreadCount, m_NetConfig.MaxConnectionCount);

				auto pConnection = new Connection();
				pConnection->Init(m_ListenSocket, m_WrokIOCPList[iocpIndex], i, config);
				m_Connections.push_back(pConnection);
			}

			return true;
		}

		void DestoryConnections()
		{
			for (int i = 0; i < m_NetConfig.MaxConnectionCount; ++i)
			{
				delete m_Connections[i];				
			}
		}

		Connection* GetConnection(const INT32 connectionIndex)
		{
			if (connectionIndex < 0 || connectionIndex >= m_NetConfig.MaxConnectionCount)
			{
				return nullptr;
			}

			return m_Connections[connectionIndex];
		}


		bool CreatePerformance()
		{
			if (m_NetConfig.PerformancePacketMillisecondsTime == INVALID_VALUE)
			{
				return false;
			}

			m_Performance = std::make_unique<Performance>();
			m_Performance->Start(m_NetConfig.PerformancePacketMillisecondsTime);

			return true;
		}

		bool CreateWorkThread()
		{
			if (m_NetConfig.WorkThreadCount == INVALID_VALUE)
			{
				return false;
			}

			m_AccetpThread = std::thread([&]() { AcceptThread(); });

			for (int i = 0; i < m_NetConfig.WorkThreadCount; ++i)
			{
				m_WorkThreads.emplace_back([&]() { WorkThread(i); });
			}

			return true;
		}

		void AcceptThread()
		{
			while (m_IsRunWorkThread)
			{
				DWORD ioSize = 0;
				OVERLAPPED_EX* pOverlappedEx = nullptr;
				Connection* pConnection = nullptr;

				//TODO: 모든 GetQueuedCompletionStatus를 GetQueuedCompletionStatusEx 버전을 사용하도록 변경. 
				auto result = GetQueuedCompletionStatus(
					m_hAcceptWorkIOCP,
					&ioSize,
					reinterpret_cast<PULONG_PTR>(&pConnection),
					reinterpret_cast<LPOVERLAPPED*>(&pOverlappedEx),
					INFINITE);

				if (pOverlappedEx == nullptr)
				{
					if (WSAGetLastError() != 0 && WSAGetLastError() != WSA_IO_PENDING)
					{
						char logmsg[128] = { 0, };
						sprintf_s(logmsg, "IOCPServer::WorkThread - GetQueuedCompletionStatus(). error:%d", WSAGetLastError());
						LogFuncPtr((int)LogLevel::Error, logmsg);
					}
					continue;
				}

				if (result == false)
				{
					ConnectionCloseComplete(pConnection, false);
					continue;
				}

				DoAccept(pConnection);
			}
		}

		void WorkThread(int iocpIndex)
		{
			while (m_IsRunWorkThread)
			{
				DWORD ioSize = 0;
				OVERLAPPED_EX* pOverlappedEx = nullptr;
				Connection* pConnection = nullptr;

				//TODO: 모든 GetQueuedCompletionStatus를 GetQueuedCompletionStatusEx 버전을 사용하도록 변경. 
				auto result = GetQueuedCompletionStatus(
					m_WrokIOCPList[iocpIndex],
					&ioSize,
					reinterpret_cast<PULONG_PTR>(&pConnection),
					reinterpret_cast<LPOVERLAPPED*>(&pOverlappedEx),
					INFINITE);
								
				if (pOverlappedEx == nullptr)
				{
					if (WSAGetLastError() != 0 && WSAGetLastError() != WSA_IO_PENDING)
					{
						char logmsg[128] = { 0, };
						sprintf_s(logmsg, "IOCPServer::WorkThread - GetQueuedCompletionStatus(). error:%d", WSAGetLastError());
						LogFuncPtr((int)LogLevel::Error, logmsg);
					}
					continue;
				}

				if (result == false || (0 == ioSize && OperationType::Recv == pOverlappedEx->OverlappedExOperationType))
				{
					ConnectionCloseComplete(pConnection, true);
					continue;
				}

				switch (pOverlappedEx->OverlappedExOperationType)
				{
				case OperationType::Recv:
					DoRecv(pOverlappedEx, ioSize);
					break;
				case OperationType::Send:
					DoSend(pOverlappedEx, ioSize);
					break;
				case OperationType::DisConnect:
					ConnectionCloseComplete(pConnection, true);
					break;
				}
			}
		}

		void ConnectionCloseComplete(Connection* pConnection, bool isForwardingToAppLayer)
		{
			pConnection->Close();

			if (isForwardingToAppLayer == false)
			{
				pConnection->ResetConnection();
				return;
			}

			if (PostNetMessage(pConnection, pConnection->GetCloseMsg()) != NetResult::Success)
			{
				//TODO 중요
				//아주아주 낮은 확률이지만 살패가 발생할 수도 있다. 
				//이런 경우는 애플리케이션 측에서 끊어진 세션에 대한 통보를 받지 못한 세션에 새로운 연결 메시지를 받으면 이전 세션을 끊어짐 처리를 하고, 새로운 연결도 일단 끊도록 한다
				// ResetConnection() 호출을 여기서 하면 위험해질 수 있다
				pConnection->ResetConnection();
			}
		}

		//TODO PostNetMessage 호출이 실패한 경우 어떻게 처리할지 이 함수를 호출한 곳에서 적적할게 구현해야 한다.
		// Recv 메시지에 대해서는 에러 처리 하지 않아도 될듯. 이 경우 아마 클라이언트가 접속을 짜를듯
		NetResult PostNetMessage(Connection* pConnection, Message* pMsg, const DWORD packetSize = 0)
		{
			if (m_hLogicIOCP == INVALID_HANDLE_VALUE || pMsg == nullptr)
			{
				return NetResult::fail_message_null;
			}

			// Boost.Asio의 경우 다른 식으로 오류에 대해 방어하고 있다. 참고하자
			//https://www.boost.org/doc/libs/1_67_0/boost/asio/detail/impl/win_iocp_io_context.ipp
			auto result = PostQueuedCompletionStatus(
				m_hLogicIOCP,
				packetSize,
				reinterpret_cast<ULONG_PTR>(pConnection),
				reinterpret_cast<LPOVERLAPPED>(pMsg));

			if (!result)
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "IOCPServer::PostLogicMsg - PostQueuedCompletionStatus(). error:%d, MsgType:%d", WSAGetLastError(), pMsg->Type);
				LogFuncPtr((int)LogLevel::Error, logmsg);
				return NetResult::fail_pqcs;
			}
			return NetResult::Success;
		}
				
		void DoAccept(Connection* pConnection)
		{
			if (pConnection->SetNetAddressInfo() == false)
			{
				char logmsg[128] = { 0, };
				sprintf_s(logmsg, "IOCPServer::DoAccept - GetAcceptExSockaddrs(). error:%d", WSAGetLastError());
				LogFuncPtr((int)LogLevel::Error, logmsg);

				ConnectionCloseComplete(pConnection, false);
				pConnection->ResetConnection();
				return;
			}
		
			if (pConnection->BindIOCP() == false)
			{
				ConnectionCloseComplete(pConnection, false);
				pConnection->ResetConnection();
				return;
			}


			pConnection->SetNetStateConnection();
			
			if (PostNetMessage(pConnection, pConnection->GetConnectionMsg()) != NetResult::Success)
			{
				ConnectionCloseComplete(pConnection, false);
				pConnection->ResetConnection();
				return;
			}

			auto result = pConnection->PostRecv(pConnection->RecvBufferBeginPos(), 0);
			if (result != NetResult::Success)
			{
				char logmsg[128] = { 0, };
				sprintf_s(logmsg, "IOCPServer::PostRecv. Call pConnection->PostRecv. error:%d", WSAGetLastError());
				LogFuncPtr((int)LogLevel::Error, logmsg);
				
				ConnectionCloseComplete(pConnection, true);
				return;
			}			
		}

		void DoRecv(OVERLAPPED_EX* pOverlappedEx, const DWORD ioSize)
		{
			Connection* pConnection = GetConnection(pOverlappedEx->ConnectionIndex);
			if (pConnection == nullptr)
			{
				return;
			}
						
			pOverlappedEx->OverlappedExWsaBuf.buf = pOverlappedEx->pOverlappedExSocketMessage;
			pOverlappedEx->OverlappedExRemainByte += ioSize;
												
			auto remainByte = pOverlappedEx->OverlappedExRemainByte;
			auto pNext = pOverlappedEx->OverlappedExWsaBuf.buf;
			
			RequestPacketForwardingLoop(pConnection, remainByte, pNext);

			if (pConnection->PostRecv(pNext, remainByte) != NetResult::Success)
			{
				pConnection->DisConnectAsync();
			}
		}

		void RequestPacketForwardingLoop(Connection* pConnection, DWORD& remainByte, char* pBuffer)
		{
			//TODO 패킷 분해 부분을 가상 함수로 만들기 

			const int PACKET_HEADER_LENGTH = 5;
			const int PACKET_SIZE_LENGTH = 2;
			const int PACKET_TYPE_LENGTH = 2;
			short packetSize = 0;

			while (true)
			{
				if (remainByte < PACKET_HEADER_LENGTH)
				{
					break;
				}

				CopyMemory(&packetSize, pBuffer, PACKET_SIZE_LENGTH);
				auto currentSize = packetSize;

				if (0 >= packetSize || packetSize > pConnection->RecvBufferSize())
				{
					char logmsg[128] = { 0, }; sprintf_s(logmsg, "IOCPServer::DoRecv. Arrived Wrong Packet.");
					LogFuncPtr((int)LogLevel::Error, logmsg);

					pConnection->DisConnectAsync();
					return;
				}

				if (remainByte >= (DWORD)currentSize)
				{
					auto pMsg = m_pMsgPool->AllocMsg();
					if (pMsg == nullptr)
					{
						return;
					}

					pMsg->SetMessage(MessageType::OnRecv, pBuffer);
					if (PostNetMessage(pConnection, pMsg, currentSize) != NetResult::Success)
					{
						m_pMsgPool->DeallocMsg(pMsg);
						return;
					}

					remainByte -= currentSize;
					pBuffer += currentSize;
				}
				else
				{
					break;
				}
			}
		}

		void DoSend(OVERLAPPED_EX* pOverlappedEx, const DWORD ioSize)
		{
			auto pConnection = GetConnection(pOverlappedEx->ConnectionIndex);
			if (pConnection == nullptr)
			{
				return;
			}
									
			pOverlappedEx->OverlappedExRemainByte += ioSize;

			//모든 메세지 전송하지 못한 상황
			if (static_cast<DWORD>(pOverlappedEx->OverlappedExTotalByte) > pOverlappedEx->OverlappedExRemainByte)
			{
				pOverlappedEx->OverlappedExWsaBuf.buf += ioSize;
				pOverlappedEx->OverlappedExWsaBuf.len -= ioSize;

				ZeroMemory(&pOverlappedEx->Overlapped, sizeof(OVERLAPPED));

				DWORD flag = 0;
				DWORD sendByte = 0;
				auto result = WSASend(
					pConnection->GetClientSocket(),
					&(pOverlappedEx->OverlappedExWsaBuf),
					1,
					&sendByte,
					flag,
					&(pOverlappedEx->Overlapped),
					NULL);

				if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
				{
					char logmsg[128] = { 0, };
					sprintf_s(logmsg, "IOCPServer::DoSend. WSASend. error:%d", WSAGetLastError());
					LogFuncPtr((int)LogLevel::Error, logmsg);

					pConnection->DisConnectAsync();
					return;
				}
			}
			//모든 메세지 전송한 상황
			else
			{
				pConnection->SendBufferSendCompleted(pOverlappedEx->OverlappedExTotalByte);
				pConnection->SetEnableSend();
				
				if (pConnection->PostSend(0) == false)
				{
					pConnection->DisConnectAsync();
				}
			}
		}
							
		void ForwardingNetOnRecvMsgToAppLayer(Connection* pConnection, const Message* pMsg, OUT INT8& msgOperationType, OUT INT32& connectionIndex, char* pBuf, OUT INT16& copySize, const DWORD ioSize)
		{
			if (pMsg->pContents == nullptr)
			{
				return;
			}

			msgOperationType = (INT8)pMsg->Type;
			connectionIndex = pConnection->GetIndex();
			CopyMemory(pBuf, pMsg->pContents, ioSize);

			copySize = static_cast<INT16>(ioSize);

			pConnection->RecvBufferReadCompleted(ioSize);

			m_Performance.get()->IncrementPacketProcessCount();
		}

		int ConnectionWorkIOCPIndex(const int connectionIndex, const int ioThreadCount, const int connectionCount)
		{
			auto ConnectionsPerThread = connectionCount / ioThreadCount;
			auto iocpIndex = connectionIndex / ConnectionsPerThread;

			if (iocpIndex >= ioThreadCount)
			{
				--iocpIndex;
			}

			return iocpIndex;
		}

	private:
		NetConfig m_NetConfig;

		SOCKET m_ListenSocket = INVALID_SOCKET;

		std::vector<Connection*> m_Connections;
			
		std::vector<HANDLE> m_WrokIOCPList;
		HANDLE m_hAcceptWorkIOCP = INVALID_HANDLE_VALUE;
		
		HANDLE m_hLogicIOCP = INVALID_HANDLE_VALUE;

		bool m_IsRunWorkThread = true;
		std::thread m_AccetpThread;
		std::vector<std::thread> m_WorkThreads;

		std::unique_ptr<MessagePool> m_pMsgPool;

		std::unique_ptr<Performance> m_Performance;
	};
}