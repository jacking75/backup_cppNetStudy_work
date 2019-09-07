#pragma once
#include <unordered_map>

#include "User.h"



namespace ChatServerLib
{
	class UserManager
	{

	public:
		UserManager() = default;
		~UserManager() = default;

		void Init();

		int GetCurrentUserCnt() { return current_user_cnt; } //TODO 최흥배 코드가 보기 좋도록 함수 간에 간격을 준다. 다른 파일도 같이 적용하기

		int GetMaxUserCnt() { return MAX_USER_CNT; }
		
		void IncreaseUserCnt() { current_user_cnt++; }
		
		void DecreaseUserCnt() 
		{
			if (current_user_cnt > 0) 
			{
				current_user_cnt--;
			}
		}

		int AddUser(char* userID, int conn_idx);
		
		int FindUserByID(char* userID);
		
		void DeleteUserInfo(User* deleteUser);

		User* GetUserByConnIdx(INT32 conn_idx);


	private:
		const int MAX_USER_CNT = 1024; //TODO 최흥배 이 값은 하드코딩이 아닌 외부에서 값을 받도록 한다
		int current_user_cnt = 0; //TODO 최흥배 코딩 룰에 맞게 이름 바꾸기

		std::vector<User*> UserObjPool; //vector로
		std::unordered_map<std::string, int> UserDictionary;
	};
}