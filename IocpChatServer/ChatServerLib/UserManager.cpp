#include <utility>

#include "UserManager.h"


namespace ChatServerLib
{
	void UserManager::Init() 
	{

		UserObjPool = std::vector<User*>(MAX_USER_CNT);

		for (int i = 0; i < MAX_USER_CNT; i++) 
		{
			UserObjPool[i] = new User();
			UserObjPool[i]->Init(i);
		}


	}

	int UserManager::AddUser(char* userID, int conn_idx)	
	{
		//TODO 최흥배 유저 중복 조사하기

		int user_idx = conn_idx;

		UserObjPool[user_idx]->SetLogin(conn_idx, userID);
		UserDictionary.insert(std::pair< char*, int>(userID, conn_idx));

		return 0;
	}

	int UserManager::FindUserByID(char* userID) 
	{

		std::unordered_map<std::string, int>::iterator res;
		res = UserDictionary.find(userID);
		if (res == UserDictionary.end()) 
		{
			return -1;
		}
		else 
		{
			return (*res).second;
		}
	}

	void UserManager::DeleteUserInfo(User* deleteUser) 
	{
		UserDictionary.erase(deleteUser->GetUserId());
		deleteUser->Clear();

	}

	User* UserManager::GetUserByConnIdx(INT32 conn_idx)
	{
		return UserObjPool[conn_idx];
	}
}