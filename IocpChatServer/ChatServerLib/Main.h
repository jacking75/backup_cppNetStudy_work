#pragma once

#include <memory>

#include <concurrent_queue.h>

#include "../../IocpNetLib/IOCPServerNet.h"
#include "../ChatServerLib/Define.h"
#include "PacketManager.h"

namespace ChatServerLib 
{
	class Main
	{
	public:
		Main();
		~Main();

		int Init(ChatServerConfig serverConfig);
		
		void Run();
		
		void Stop();
		
		NetLib::IOCPServerNet* GetIOCPServer() { return  m_pIOCPServer.get(); }


	private:
		std::unique_ptr<NetLib::IOCPServerNet> m_pIOCPServer;
		std::unique_ptr<UserManager> m_pUserManager;
		std::unique_ptr<PacketManager> m_pPacketManager;
		std::unique_ptr<RoomManager> m_pRoomManager;
		
		Concurrency::concurrent_queue<char*> m_MqDataQueue;

		int max_packet_size;
		int max_connection_count;
		int post_message_thread_cnt;
		int MaxRoomCnt;


		bool m_IsRun = false;
	};


}