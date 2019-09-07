#pragma once
#include "Room.h"

namespace ChatServerLib
{
	class RoomManager
	{
	public:
		RoomManager() = default;
		~RoomManager() = default;

		void Init();

		UINT GetMaxRoomCount() { return MAX_ROOM_COUNT; }
		
		UINT GetCurrentRoomCount() { return m_CurrentRoomCount; }

		UINT16 EnterUser(int RoomIdx, User* pUser);
		
		UINT16 LeaveUser(int RoomIdx, User* pUser);

		Room* GetRoomByIndex(UINT32 index) { return m_RoomList[index]; } //TODO index가 잘못된 값일때 예외처리 해주기

		
		std::function<void(INT32, void*, INT16)> SendPacketFunc;
		

	private:
		std::vector<Room*> m_RoomList;
		UINT16 MAX_ROOM_COUNT = 10; //TODO 최흥배 값을 하드 코딩 안하기
		UINT16 m_CurrentRoomCount = 0;		
	};
}