#pragma once

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

//#define SAFE_DELETE(x) if( x != nullptr ) { delete x; x = nullptr; } 
//#define SAFE_DELETE_ARR(x) if( x != nullptr ) { delete [] x; x = nullptr;}

namespace NetLib
{
	const INT32 INVALID_VALUE = -1;
	
	enum class LogLevel
	{
		Trace = 0,
		Debug,
		Info,
		Warn,
		Error,
		Fetal
	};


	enum class NetResult : INT16
	{
		Success = 0,

		Fail_Make_Directories_Log = 30,
		Fail_Make_Directories_Dump = 31,
		Fail_WorkThread_Info = 32,
		Fail_Server_Info_Port = 33,
		fail_server_info_max_recv_ovelapped_buffer_size = 34,
		fail_server_info_max_send_ovelapped_buffer_size = 35,
		fail_server_info_max_recv_connection_buffer_size = 36,
		fail_server_info_max_send_connection_buffer_size = 37,
		fail_server_info_max_packet_size = 38,
		fail_server_info_max_connection_count = 39,
		fail_server_info_max_message_pool_count = 40,
		fail_server_info_extra_message_pool_count = 41,
		fail_server_info_performance_packet_milliseconds_time = 42,
		fail_server_info_post_messages_threads_count = 43,
		fail_create_listensocket_startup = 50,
		fail_create_listensocket_socket = 51,
		fail_create_listensocket_bind = 52,
		fail_create_listensocket_listen = 53,
		fail_handleiocp_work = 61,
		fail_handleiocp_logic = 62,
		fail_handleiocp_accept = 63,
		Fail_Create_Message_Manager = 66,
		Fail_Link_IOCP = 68,
		Fail_Create_Connection = 71,
		Fail_Create_Performance = 72,
		Fail_Create_WorkThread = 73,

		BindAcceptExSocket_fail_WSASocket = 76,
		BindAcceptExSocket_fail_AcceptEx = 77,

		fail_message_null = 79,
		fail_pqcs = 80,

		PostRecv_Null_Obj = 81,
		PostRecv_Null_WSABUF = 82,
		PostRecv_Null_Socket_Error = 83,

		ReservedSendPacketBuffer_Not_Connected = 86,
		ReservedSendPacketBuffer_Empty_Buffer = 87,

		FUNCTION_RESULT_END
	};
		
	


	class Helper
	{
		Helper() = default;
		~Helper() = default;

		INT32 SystemCoreCount()
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			return (INT32)systemInfo.dwNumberOfProcessors;
		}
	};

	inline void (*LogFuncPtr) (const int, const char* pszLogMsg);

}