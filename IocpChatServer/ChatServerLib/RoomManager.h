#pragma once
#include "Room.h"

namespace ChatServerLib
{
	class RoomManager
	{
	public:
		RoomManager() = default;
		~RoomManager() = default;

		void Init(const INT32 beginRoomNumber, const INT32 maxRoomCount)
		{
			m_BeginRoomNumber = beginRoomNumber;
			m_MaxRoomCount = maxRoomCount;
			m_EndRoomNumber = beginRoomNumber + maxRoomCount;

			m_RoomList = std::vector<Room*>(maxRoomCount);

			for (auto i = 0; i < maxRoomCount; i++)
			{
				m_RoomList[i] = new Room();
				m_RoomList[i]->SendPacketFunc = SendPacketFunc;
				m_RoomList[i]->Init((i+ beginRoomNumber));
			}
		}

		UINT GetMaxRoomCount() { return m_MaxRoomCount; }
		
		UINT16 EnterUser(INT32 roomNumber, User* pUser)
		{
			auto pRoom = GetRoomByNumber(roomNumber);
			if (pRoom == nullptr)
			{
				return (UINT16)Common::ERROR_CODE::ROOM_INVALID_INDEX;
			}


			return pRoom->EnterUser(pUser);
		}
		
		UINT16 LeaveUser(INT32 roomNumber, User* pUser)
		{
			auto pRoom = GetRoomByNumber(roomNumber);
			if (pRoom == nullptr)
			{
				return (UINT16)Common::ERROR_CODE::ROOM_INVALID_INDEX;
			}
			
			return pRoom->LeaveUser(pUser);
		}

		Room* GetRoomByNumber(INT32 number) 
		{ 
			if (number < m_BeginRoomNumber || number >= m_EndRoomNumber)
			{
				return nullptr;
			}

			auto index = (number - m_BeginRoomNumber);
			return m_RoomList[index]; 
		} 

		
		std::function<void(INT32, void*, INT16)> SendPacketFunc;
		

	private:
		std::vector<Room*> m_RoomList;
		INT32 m_BeginRoomNumber = 0;
		INT32 m_EndRoomNumber = 0;
		INT32 m_MaxRoomCount = 0;
	};
}