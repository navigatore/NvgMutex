#include <windows.h>
#include "..\NvgMutexLib\common.h"
#include <cstdio>
#include <string>
#include <map>

HANDLE main_mailslot_handle = CreateMailslot(mailslot_name, sizeof(MailslotMessage), MAILSLOT_WAIT_FOREVER, NULL);

thread_local HANDLE client_handle;	
thread_local HANDLE watches[2];	// [0] -> client thread, [1] -> my mailslot handle

thread_local int msg_ctr = 0;
//*********************************************************************************************************************
void connect_new_client(MailslotMessage &msg);
void ack(MailslotMessage msg);
DWORD WINAPI ServerThread(LPVOID param);

void send_message(MessageType type, void* content, int clen);
void get_ack();
MailslotMessage receive_message(DWORD timeout = INFINITE);
MailslotMessage receive_message_with_timeout(DWORD timeout);
MailslotMessage receive_message_without_timeout();
//*********************************************************************************************************************
int main()
{
	printf("Main thread TID: %d\n", GetCurrentThreadId());

	MailslotMessage request;
	DWORD bytes_read;
	while (true)
	{
		ReadFile(main_mailslot_handle, &request, sizeof(MailslotMessage), &bytes_read, nullptr);

		if (bytes_read == sizeof(MailslotMessage) && request.type == MessageType::connect)
			connect_new_client(request);
	}
}
//*********************************************************************************************************************
void ack(MailslotMessage msg)
{
	msg.type = MessageType::ack;
	DWORD bytes_written;
	WriteFile(client_handle, &msg, sizeof(MailslotMessage), &bytes_written, NULL);
	if (bytes_written != sizeof(MailslotMessage))
		throw ConnectionError("Error while writing to client mailslot");
}
//*********************************************************************************************************************
void connect_new_client(MailslotMessage &msg)
{
	MailslotMessage* msgptr = reinterpret_cast<MailslotMessage*>(GlobalAlloc(GMEM_FIXED, sizeof(msg)));
	*msgptr = msg;
	HANDLE t = CreateThread(NULL, 0, ServerThread, msgptr, 0, NULL);
	CloseHandle(t);
}
//*********************************************************************************************************************
DWORD WINAPI ServerThread(LPVOID param)
{
	std::map<HANDLE, bool> mutexes;

	printf("Server Thread start, TID: %d\n", GetCurrentThreadId());

	MailslotMessage* msg = reinterpret_cast<MailslotMessage*>(param);

	auto client_tid = *(reinterpret_cast<DWORD*>(msg->content));
	watches[0] = OpenThread(SYNCHRONIZE, false, client_tid);

	std::string client_name = "\\\\.\\mailslot\\nvg_mutex_" + std::to_string(client_tid);
	std::wstring tmp = std::wstring(client_name.begin(), client_name.end());
	client_handle = CreateFile(tmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	ack(*msg);
	msg_ctr++;

	std::string name = "\\\\.\\mailslot\\nvg_mutex_" + std::to_string(GetCurrentThreadId());
	tmp = std::wstring(name.begin(), name.end());
	watches[1] = CreateMailslot(tmp.c_str(), sizeof(MailslotMessage), MAILSLOT_WAIT_FOREVER, nullptr);
	SetMailslotInfo(watches[1], read_timeout);

	GlobalFree(param);

	try
	{
		send_message(MessageType::connect, (void*)name.c_str(), strlen(name.c_str()) + 1);

		while (true)
		{
			MailslotMessage order = receive_message();
			ack(order);
			msg_ctr++;
			if (order.type == MessageType::create_mutex)
			{
				std::string name(reinterpret_cast<char*>(order.content));
				name = "nvg_mutex_" + name;
				std::wstring wname(name.begin(), name.end());
				auto handle = CreateMutex(nullptr, false, wname.c_str());
				mutexes[handle] = true;

				if (handle == nullptr)
					throw FatalError("Mutex creation failed");
				send_message(MessageType::mutex_created, &handle, sizeof(handle));
			}
			else if (order.type == MessageType::wait)
			{
				int True = 1, False = 0;
				HANDLE handle = *(reinterpret_cast<HANDLE*>(order.content));
				int timeout = *(reinterpret_cast<int*>(order.content) + 1);
				if (!timeout)
				{
					WaitForSingleObject(handle, INFINITE);
					send_message(MessageType::wait_finished, (void*)&True, sizeof(bool));
				}
				else
				{
					auto wait = WaitForSingleObject(handle, timeout);
					if (wait == WAIT_OBJECT_0)
						send_message(MessageType::wait_finished, (void*)&True, sizeof(int));
					else if (wait == WAIT_TIMEOUT)
						send_message(MessageType::wait_finished, (void*)&False, sizeof(int));
					else
						throw FatalError("Wait finished, but the finish reason was not expected");
				}

			}
			else if (order.type == MessageType::unlock)
			{
				HANDLE handle = *(reinterpret_cast<HANDLE*>(order.content));
				ReleaseMutex(handle);
				send_message(MessageType::mutex_unlocked, "\0", 1);
			}
			else if (order.type == MessageType::close_handle)
			{
				HANDLE handle = *(reinterpret_cast<HANDLE*>(order.content));
				CloseHandle(handle);
				mutexes.erase(handle);
				send_message(MessageType::handle_closed, "\0", 1);
			}
		}
	}
	catch (ConnectionError &e)
	{
		printf("Connection error, goodbye client! What happened: %s\n", e.what());
	}
	catch (FatalError &e)
	{
		printf("This should never happen, inspect your code. What happened: %s\n", e.what());
	}
	for (auto it = mutexes.cbegin(); it != mutexes.cend(); it++)
	{
		if (!CloseHandle(it->first))
			printf("Something wrong happened\n");
	}
	CloseHandle(client_handle);
	CloseHandle(watches[0]);
	CloseHandle(watches[1]);
	return 0;
}
//*********************************************************************************************************************
void send_message(MessageType type, void* content, int clen)
{
	if (clen > buf_size || clen < 0)
		throw FatalError("Improper content length given");
	MailslotMessage msg;
	msg.id = msg_ctr;
	msg.type = type;
	memcpy(msg.content, content, clen);

	DWORD bytes_written;

	if (!WriteFile(client_handle, &msg, sizeof(MailslotMessage), &bytes_written, nullptr) || bytes_written != sizeof(msg))
	{
		throw ConnectionError("Error while writing to client mailslot");
	}
	get_ack();
	msg_ctr++;
}
//*********************************************************************************************************************
void get_ack()
{
	auto recv = receive_message(read_timeout);
	if (recv.id != msg_ctr || recv.type != MessageType::ack)
		throw ConnectionError("Message counter detected error");
}
//*********************************************************************************************************************
MailslotMessage receive_message(DWORD timeout)
{
	if (timeout == INFINITE)
		return receive_message_without_timeout();
	else
		return receive_message_with_timeout(timeout);
}
//*********************************************************************************************************************
MailslotMessage receive_message_without_timeout()
{
	SetMailslotInfo(watches[1], INFINITE);
	MailslotMessage msg;
	DWORD bytes_read;
	while (true)
	{
		auto signaled = WaitForMultipleObjects(2, watches, false, INFINITE);
		if (signaled == WAIT_OBJECT_0)	// client down!
		{
			throw ConnectionError("Client thread closed");
		}
		else if (signaled != WAIT_OBJECT_0 + 1)
		{
			throw FatalError("Wait finished, but the finish reason was not expected");
		}
		DWORD how_many;
		GetMailslotInfo(watches[1], nullptr, nullptr, &how_many, nullptr);
		if (how_many)
		{
			ReadFile(watches[1], &msg, sizeof(msg), &bytes_read, nullptr);
			break;
		}
	}
	return msg;
}
//*********************************************************************************************************************
MailslotMessage receive_message_with_timeout(DWORD timeout)
{
	MailslotMessage msg;
	DWORD bytes_read;
	SetMailslotInfo(watches[1], timeout);
	if (!ReadFile(watches[1], &msg, sizeof(MailslotMessage), &bytes_read, nullptr) || msg.id != msg_ctr)
		throw ConnectionError("Error while reading from server thread mailslot");
	return msg;
}
//*********************************************************************************************************************