#pragma once

#include <mutex>
#include <atomic>

#define WIN32_LEAN_AND_MEAN 
#include <WinSock2.h>
#include <mswsock.h>

#include "RingBuffer.h"
#include "NetDefine.h"


namespace NetLib
{	
	//TODO lock 방법 수정하기 
	//세션의 send,recv는 스레드 세이프 하다. 
	//IOCP 스레드가 아닌 곳에서 close 하는 경우는 스레드 세이프하도록 IOCP에 메시지를 보내서 접속을 끊도록 한다

	//void NetResult ResetConnection()
	//bool BindIOCP(const HANDLE hWorkIOCP)

	class Connection
	{
	public:	
		Connection() {}
		~Connection() {}

		Message* GetConnectionMsg() { return &m_ConnectionMsg; } 
		Message* GetCloseMsg() { return &m_CloseMsg; }
		
		SOCKET GetClientSocket() { return m_ClientSocket; }

		void SetConnectionIP(const char* szIP) { CopyMemory(m_szIP, szIP, MAX_IP_LENGTH); }
		INT32 GetIndex() { return m_Index; }

		bool IsConnect() { return m_IsConnect; }

		HANDLE GetIOCPHandle() { return m_hIOCP; }

		void SetNetStateConnection() { InterlockedExchange(reinterpret_cast<LPLONG>(&m_IsConnect), TRUE); }

		BOOL SetNetStateDisConnection() { return InterlockedCompareExchange(reinterpret_cast<LPLONG>(&m_IsConnect), FALSE, TRUE); }

		INT32 RecvBufferSize() { return m_RingRecvBuffer.GetBufferSize(); }

		char* RecvBufferBeginPos() { return m_RingRecvBuffer.GetBeginMark(); }

		void RecvBufferReadCompleted(const INT32 size) { m_RingRecvBuffer.ReleaseBuffer(size); }

		void Init(const SOCKET listenSocket, HANDLE iocpHandle, const INT32 index, const ConnectionNetConfig config)
		{
			m_Index = index;
			m_hIOCP = iocpHandle;
			m_ListenSocket = listenSocket;			
			m_RecvBufSize = config.MaxRecvOverlappedBufferSize;
			m_SendBufSize = config.MaxSendOverlappedBufferSize;

			Init();

			m_pRecvOverlappedEx = new OVERLAPPED_EX(index);
			m_pSendOverlappedEx = new OVERLAPPED_EX(index);
			m_pDisConnectOverlappedEx = new OVERLAPPED_EX(index);
			m_pDisConnectOverlappedEx->OverlappedExOperationType = OperationType::DisConnect;

			m_RingRecvBuffer.Create(config.MaxRecvBufferSize);
			m_RingSendBuffer.Create(config.MaxSendBufferSize);

			m_ConnectionMsg.Type = MessageType::Connection;
			m_ConnectionMsg.pContents = nullptr;
			m_CloseMsg.Type = MessageType::Close;
			m_CloseMsg.pContents = nullptr;

			BindAcceptExSocket();
		}
						
		//TODO 코드 수정 확인: IOCP 스레드에서 호출할 때만 가능하다.  반환 값은 void로 한다
		// 파라미터로 close 메시지 보낼지 말지 알려준다
		bool CloseComplete()
		{			
			if (m_ClientSocket != INVALID_SOCKET)
			{
				closesocket(m_ClientSocket);
				m_ClientSocket = INVALID_SOCKET;
			}

			//TODO 아래 함수 호출해야 한다
			/*if (PostNetMessage(pConnection, pConnection->GetCloseMsg()) != NetResult::Success)
			{
				pConnection->ResetConnection();
			}*/
			return false;
		}
		
		void DisConnectAsync()
		{
			if (SetNetStateDisConnection() == FALSE)
			{
				return;
			}

			auto result = PostQueuedCompletionStatus(
				m_hIOCP,
				0,
				reinterpret_cast<ULONG_PTR>(this),
				reinterpret_cast<LPOVERLAPPED>(m_pDisConnectOverlappedEx));

			if (!result)
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "Connection::DisConnectAsync - PostQueuedCompletionStatus(). error:%d", WSAGetLastError());
				LogFuncPtr((int)LogLevel::Error, logmsg);
			}
		}
					
		NetResult ResetConnection()
		{
			std::lock_guard<std::mutex> Lock(m_MUTEX);

			m_pRecvOverlappedEx->OverlappedExRemainByte = 0;
			m_pRecvOverlappedEx->OverlappedExTotalByte = 0;
			m_pSendOverlappedEx->OverlappedExRemainByte = 0;
			m_pSendOverlappedEx->OverlappedExTotalByte = 0;
			
			Init();
			return BindAcceptExSocket();
		}
		
		bool BindIOCP()
		{
			std::lock_guard<std::mutex> Lock(m_MUTEX);

			//즉시 접속 종료하기 위한 소켓 옵션 추가
			linger li = { 0, 0 };
			li.l_onoff = 1;
			setsockopt(m_ClientSocket, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&li), sizeof(li));

			auto hIOCPHandle = CreateIoCompletionPort(
				reinterpret_cast<HANDLE>(m_ClientSocket),
				m_hIOCP,
				reinterpret_cast<ULONG_PTR>(this),
				0);

			if (hIOCPHandle == INVALID_HANDLE_VALUE)
			{
				return false;
			}

			return true;
		}
		
		NetResult PostRecv(const char* pNextBuf, const DWORD remainByte)
		{
			if (m_IsConnect == FALSE || m_pRecvOverlappedEx == nullptr)
			{
				return NetResult::PostRecv_Null_Obj;
			}

			m_pRecvOverlappedEx->OverlappedExOperationType = OperationType::Recv;
			m_pRecvOverlappedEx->OverlappedExRemainByte = remainByte;

			auto moveMark = static_cast<int>(remainByte - (m_RingRecvBuffer.GetCurMark() - pNextBuf));
			m_pRecvOverlappedEx->OverlappedExWsaBuf.len = m_RecvBufSize;
			m_pRecvOverlappedEx->OverlappedExWsaBuf.buf = m_RingRecvBuffer.ForwardMark(moveMark, m_RecvBufSize, remainByte);

			if (m_pRecvOverlappedEx->OverlappedExWsaBuf.buf == nullptr)
			{
				return NetResult::PostRecv_Null_WSABUF;
			}

			m_pRecvOverlappedEx->pOverlappedExSocketMessage = m_pRecvOverlappedEx->OverlappedExWsaBuf.buf - remainByte;

			ZeroMemory(&m_pRecvOverlappedEx->Overlapped, sizeof(OVERLAPPED));

			IncrementRecvIORefCount();

			DWORD flag = 0;
			DWORD recvByte = 0;
			auto result = WSARecv(
				m_ClientSocket,
				&m_pRecvOverlappedEx->OverlappedExWsaBuf,
				1,
				&recvByte,
				&flag,
				&m_pRecvOverlappedEx->Overlapped,
				NULL);

			if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
			{
				DecrementRecvIORefCount();
				return NetResult::PostRecv_Null_Socket_Error;
			}

			return NetResult::Success;
		}
		
		bool PostSend(const INT32 sendSize)
		{
			//남은 패킷이 존재하는지 확인하기 위한 과정
			if (sendSize > 0)
			{
				m_RingSendBuffer.SetUsedBufferSize(sendSize);
			}

			if (InterlockedCompareExchange(reinterpret_cast<LPLONG>(&m_IsSendable), FALSE, TRUE))
			{
				auto reservedSize = 0;
				char* pBuf = m_RingSendBuffer.GetBuffer(m_SendBufSize, reservedSize);
				if (pBuf == nullptr)
				{
					InterlockedExchange(reinterpret_cast<LPLONG>(&m_IsSendable), TRUE);
					return true;
				}

				ZeroMemory(&m_pSendOverlappedEx->Overlapped, sizeof(OVERLAPPED));

				m_pSendOverlappedEx->OverlappedExWsaBuf.len = reservedSize;
				m_pSendOverlappedEx->OverlappedExWsaBuf.buf = pBuf;
				m_pSendOverlappedEx->ConnectionIndex = GetIndex();

				m_pSendOverlappedEx->OverlappedExRemainByte = 0;
				m_pSendOverlappedEx->OverlappedExTotalByte = reservedSize;
				m_pSendOverlappedEx->OverlappedExOperationType = OperationType::Send;

				IncrementSendIORefCount();

				DWORD flag = 0;
				DWORD sendByte = 0;
				auto result = WSASend(
					m_ClientSocket,
					&m_pSendOverlappedEx->OverlappedExWsaBuf,
					1,
					&sendByte,
					flag,
					&m_pSendOverlappedEx->Overlapped,
					NULL);

				if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
				{
					DecrementSendIORefCount();
					return false;					
				}
			}
			return true;
		}
		
		NetResult ReservedSendPacketBuffer(OUT char** ppBuf, const int sendSize)
		{
			if (!m_IsConnect)
			{
				*ppBuf = nullptr;
				return NetResult::ReservedSendPacketBuffer_Not_Connected;
			}

			*ppBuf = m_RingSendBuffer.ForwardMark(sendSize);
			if (*ppBuf == nullptr)
			{
				return NetResult::ReservedSendPacketBuffer_Empty_Buffer;
			}

			return NetResult::Success;
		}
		
		void IncrementRecvIORefCount() { InterlockedIncrement(reinterpret_cast<LPLONG>(&m_RecvIORefCount)); }
		void IncrementSendIORefCount() { InterlockedIncrement(reinterpret_cast<LPLONG>(&m_SendIORefCount)); }
		void IncrementAcceptIORefCount() { ++m_AcceptIORefCount; }
		void DecrementRecvIORefCount() { InterlockedDecrement(reinterpret_cast<LPLONG>(&m_RecvIORefCount)); }
		void DecrementSendIORefCount() { InterlockedDecrement(reinterpret_cast<LPLONG>(&m_SendIORefCount)); }
		void DecrementAcceptIORefCount() { --m_AcceptIORefCount; }
				
		bool SetNetAddressInfo()
		{
			SOCKADDR* pLocalSockAddr = nullptr;
			SOCKADDR* pRemoteSockAddr = nullptr;

			int	localSockaddrLen = 0;
			int	remoteSockaddrLen = 0;

			GetAcceptExSockaddrs(m_AddrBuf, 0,
				sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
				&pLocalSockAddr, &localSockaddrLen,
				&pRemoteSockAddr, &remoteSockaddrLen);

			if (remoteSockaddrLen != 0)
			{
				SOCKADDR_IN* pRemoteSockAddrIn = reinterpret_cast<SOCKADDR_IN*>(pRemoteSockAddr);
				if (pRemoteSockAddrIn != nullptr)
				{
					char szIP[MAX_IP_LENGTH] = { 0, };
					inet_ntop(AF_INET, &pRemoteSockAddrIn->sin_addr, szIP, sizeof(szIP));

					SetConnectionIP(szIP);
				}

				return true;
			}
			
			return false;
		}

		void SendBufferSendCompleted(const INT32 sendSize)
		{
			m_RingSendBuffer.ReleaseBuffer(sendSize);
		}

		void SetEnableSend()
		{
			InterlockedExchange(reinterpret_cast<LPLONG>(&m_IsSendable), TRUE);
		}


	private:
		void Init()
		{
			ZeroMemory(m_szIP, MAX_IP_LENGTH);

			m_RingRecvBuffer.Init();
			m_RingSendBuffer.Init();

			m_IsConnect = FALSE;
			m_IsClosed = FALSE;
			m_IsSendable = TRUE;

			m_SendIORefCount = 0;
			m_RecvIORefCount = 0;
			m_AcceptIORefCount = 0;
		}
		
		NetResult BindAcceptExSocket()
		{
			ZeroMemory(&m_pRecvOverlappedEx->Overlapped, sizeof(OVERLAPPED));

			m_pRecvOverlappedEx->OverlappedExWsaBuf.buf = m_AddrBuf;
			m_pRecvOverlappedEx->pOverlappedExSocketMessage = m_pRecvOverlappedEx->OverlappedExWsaBuf.buf;
			m_pRecvOverlappedEx->OverlappedExWsaBuf.len = m_RecvBufSize;
			m_pRecvOverlappedEx->OverlappedExOperationType = OperationType::Accept;
			m_pRecvOverlappedEx->ConnectionIndex = GetIndex();

			m_ClientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
			if (m_ClientSocket == INVALID_SOCKET)
			{
				return NetResult::BindAcceptExSocket_fail_WSASocket;
			}

			IncrementAcceptIORefCount();

			DWORD acceptByte = 0;
			auto result = AcceptEx(
				m_ListenSocket,
				m_ClientSocket,
				m_pRecvOverlappedEx->OverlappedExWsaBuf.buf,
				0,
				sizeof(SOCKADDR_IN) + 16,
				sizeof(SOCKADDR_IN) + 16,
				&acceptByte,
				reinterpret_cast<LPOVERLAPPED>(m_pRecvOverlappedEx));

			if (!result && WSAGetLastError() != WSA_IO_PENDING)
			{
				DecrementAcceptIORefCount();

				return NetResult::BindAcceptExSocket_fail_AcceptEx;
			}

			return NetResult::Success;
		}


	private:
		INT32 m_Index = INVALID_VALUE;		
		UINT64 m_UniqueId = 0; //TODO 만약 사용 안하면 삭제하기
				
		HANDLE m_hIOCP = INVALID_HANDLE_VALUE;

		SOCKET m_ClientSocket = INVALID_SOCKET;
		SOCKET m_ListenSocket = INVALID_SOCKET;

		std::mutex m_MUTEX;

		//TODO 정적 할당으로 바꾼다
		OVERLAPPED_EX* m_pRecvOverlappedEx = nullptr;
		OVERLAPPED_EX* m_pSendOverlappedEx = nullptr;
		OVERLAPPED_EX* m_pDisConnectOverlappedEx = nullptr;

		RingBuffer m_RingRecvBuffer;
		RingBuffer m_RingSendBuffer;

		char m_AddrBuf[MAX_ADDR_LENGTH] = { 0, };

		BOOL m_IsClosed = FALSE;
		BOOL m_IsConnect = FALSE;
		BOOL m_IsSendable = TRUE;

		INT32	m_RecvBufSize = INVALID_VALUE;
		INT32	m_SendBufSize = INVALID_VALUE;
				
		char m_szIP[MAX_IP_LENGTH] = { 0, };

		//TODO 아래 함수를 제거하여 IO 에러가 발생해서 접속을 짤라야하면 바로 짜를 수 있도록 하자
		DWORD m_SendIORefCount = 0; 
		DWORD m_RecvIORefCount = 0; 
		std::atomic<short> m_AcceptIORefCount = 0;
		
		Message m_ConnectionMsg;
		Message m_CloseMsg;
	};
}