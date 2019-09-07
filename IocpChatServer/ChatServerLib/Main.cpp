#include "Main.h"


namespace ChatServerLib 
{
	int Main::Init(ChatServerConfig serverConfig) //Config를 외부에서 받도록 이 함수의 인
	{
		m_pIOCPServer = std::make_unique<NetLib::IOCPServerNet>();
		
		m_Config = serverConfig;
		auto netConfig = serverConfig.GetNetConfig();

		auto ServerStartResult = m_pIOCPServer->Start(netConfig);
		if (ServerStartResult != NetLib::NetResult::Success) {
			printf("ServerStartError! ErrorCode: %d\n", (int)ServerStartResult);
			return -1;
		}		

		m_pPacketManager = std::make_unique<PacketManager> ();
		m_pUserManager = std::make_unique<UserManager> ();
		m_pRoomManager = std::make_unique<RoomManager> ();

		auto sendPacketFunc = [&](INT32 connectionIndex, void* pSendPacket, INT16 packetSize)
		{
			m_pIOCPServer->SendPacket(connectionIndex, pSendPacket, packetSize);
		};

		m_pPacketManager->SendPacketFunc = sendPacketFunc;
		m_pPacketManager->Init(m_pUserManager.get(), m_pRoomManager.get());
		
		m_pUserManager->Init();

		m_pRoomManager->SendPacketFunc = sendPacketFunc;
		m_pRoomManager->Init();

				
		//서버정보를 초기화하고 UserManager
		
		return 0;
	}
		
	void Main::Run() 
	{
		m_IsRun = true;

		auto MaxPacketSize = m_Config.MaxPacketSize + 1;
		auto pBuf = new char[MaxPacketSize];
		ZeroMemory(pBuf, sizeof(char) * MaxPacketSize);
		INT32 waitTimeMillisec = 1;

		while (m_IsRun)
		{
			INT8 operationType = 0;
			INT32 connectionIndex = 0;
			INT16 copySize = 0;

			//여기 함수 개선하기(위의 동적할당 계속 되어서)

			//WorkThread의 함수들을 불러와서 처리한다.

			if (m_pIOCPServer->ProcessNetworkMessage(operationType, connectionIndex, pBuf, copySize, waitTimeMillisec) == false)
			{
				continue;
			}
			
			auto msgType = (NetLib::MessageType)operationType;

			switch (msgType)
			{
			case NetLib::MessageType::Connection:
				printf("On Connect %d\n",connectionIndex);
				break;
			case NetLib::MessageType::Close:
				m_pPacketManager->ClearConnectionInfo(connectionIndex);
				break;
			case NetLib::MessageType::OnRecv:
				m_pPacketManager->ProcessRecvPacket(connectionIndex, pBuf, copySize);
				break;
			}
		}

	}

	void Main::Stop()
	{
		m_IsRun = false;

		m_pIOCPServer->End();

	}

	
	
}