#pragma once

#include <memory>

#include "../../IocpNetLib/IOCPServerNet.h"
#include "Define.h"
#include "PacketManager.h"

namespace ChatServerLib 
{
	class Main
	{
	public:
		Main() = default;
		~Main() = default;

		int Init(ChatServerConfig serverConfig);
		
		void Run();
		
		void Stop();
		
		NetLib::IOCPServerNet* GetIOCPServer() { return  m_pIOCPServer.get(); }


	private:
		std::unique_ptr<NetLib::IOCPServerNet> m_pIOCPServer;
		std::unique_ptr<UserManager> m_pUserManager;
		std::unique_ptr<PacketManager> m_pPacketManager;
		std::unique_ptr<RoomManager> m_pRoomManager;
				
		ChatServerConfig m_Config;
				
		bool m_IsRun = false;
	};


}