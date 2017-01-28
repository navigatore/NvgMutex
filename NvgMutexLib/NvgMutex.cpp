#include "NvgMutex.h"

thread_local HANDLE NvgMutex::server_handle = nullptr;
thread_local HANDLE NvgMutex::my_handle = nullptr;
thread_local int NvgMutex::msg_ctr = 0;
thread_local int NvgMutex::existing_objects = 0;
//*********************************************************************************************************************
NvgMutex::NvgMutex(std::string name)
{
	if (existing_objects == 0)	// Connection with server not established yet
	{
		establish_connection();
	}
	send_message(MessageType::create_mutex, (void*)name.c_str(), strlen(name.c_str()) + 1);
	auto response = receive_message(read_timeout);
	ack(response);
	msg_ctr++;
	if (response.type != MessageType::mutex_created)
		throw ConnectionError("Got invalid response from the server");
	mhandle = *(reinterpret_cast<HANDLE*>(response.content));
	existing_objects++;
}
//*********************************************************************************************************************
NvgMutex::~NvgMutex()
{
	send_message(MessageType::close_handle, &mhandle, sizeof(mhandle));
	auto msg = receive_message(read_timeout);
	ack(msg);
	msg_ctr++;
	existing_objects--;
	if (existing_objects == 0)
	{
		disconnect();
	}
	if (msg.type != MessageType::handle_closed)
		ExitThread(1);	// Not a very good solution
}
//*********************************************************************************************************************
bool NvgMutex::lock(int wait_ms)
{
	int msg_content[2];
	msg_content[0] = reinterpret_cast<int>(mhandle);
	msg_content[1] = wait_ms;
	send_message(MessageType::wait, msg_content, sizeof(msg_content));
	MailslotMessage msg;
	if (wait_ms)
		msg = receive_message(wait_ms + read_timeout);
	else
		msg = receive_message();
	ack(msg);
	msg_ctr++;
	if (msg.type != MessageType::wait_finished)
	{
		disconnect();
		throw ConnectionError("Got invalid response from the server");
	}
	if (*reinterpret_cast<int*>(msg.content))
		return true;
	return false;
}
//*********************************************************************************************************************
void NvgMutex::unlock()
{
	send_message(MessageType::unlock, &mhandle, sizeof(mhandle));
	auto msg = receive_message(read_timeout);
	ack(msg);
	msg_ctr++;
	if (msg.type != MessageType::mutex_unlocked)
	{
		disconnect();
		throw ConnectionError("Got invalid response from the server");
	}
}
//*********************************************************************************************************************
void NvgMutex::send_message(MessageType type, void* content, int clen)
{
	if (clen > buf_size || clen < 0)
		throw FatalError("Improper content length given");
	MailslotMessage msg;
	msg.id = msg_ctr;
	msg.type = type;
	memcpy(msg.content, content, clen);

	DWORD bytes_written;

	if (!WriteFile(server_handle, &msg, sizeof(msg), &bytes_written, nullptr) || bytes_written != sizeof(msg))
	{
		disconnect();
		throw ConnectionError("Writing to server mailslot failed");
	}
	get_ack();
	msg_ctr++;
}
//*********************************************************************************************************************
void NvgMutex::ack(MailslotMessage msg)
{
	msg.type = MessageType::ack;
	DWORD bytes_written;
	WriteFile(server_handle, &msg, sizeof(MailslotMessage), &bytes_written, nullptr);
	if (bytes_written != sizeof(MailslotMessage))
	{
		disconnect();
		throw ConnectionError("Writing to server mailslot failed");
	}
}
//*********************************************************************************************************************
void NvgMutex::get_ack()
{
	auto recv = receive_message(read_timeout);
	if (recv.id != msg_ctr || recv.type != MessageType::ack)
	{
		disconnect();
		throw ConnectionError("Got invalid response from the server");
	}
}
//*********************************************************************************************************************
MailslotMessage NvgMutex::receive_message(DWORD timeout)
{
	MailslotMessage msg;
	DWORD bytes_read;
	SetMailslotInfo(my_handle, timeout);
	if (!ReadFile(my_handle, &msg, sizeof(MailslotMessage), &bytes_read, nullptr) || bytes_read != sizeof(msg))
	{
		disconnect();
		throw ConnectionError("Error while reading");
	}
	if (msg.id != msg_ctr)
	{
		disconnect();
		throw ConnectionError("Message counter detected an error");
	}
	return msg;
}
//*********************************************************************************************************************
void NvgMutex::establish_connection()
{
	std::wstring wtmp;
	std::string tmp;
	auto tid = GetCurrentThreadId();

	std::string name = "\\\\.\\mailslot\\nvg_mutex_" + std::to_string(tid);
	wtmp = std::wstring(name.begin(), name.end());
	my_handle = CreateMailslot(wtmp.c_str(), 0, MAILSLOT_WAIT_FOREVER, nullptr);
	if (my_handle == INVALID_HANDLE_VALUE)
		throw FatalError("Creating mailslot failed");

	server_handle = CreateFile(mailslot_name, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (server_handle == INVALID_HANDLE_VALUE)
		throw FatalError("Obtaining server mailslot handle failed");

	send_message(MessageType::connect, (void*)&tid, sizeof(tid));

	CloseHandle(server_handle);

	auto response = receive_message();
	std::string server_name = response.content;
	wtmp = std::wstring(server_name.begin(), server_name.end());
	server_handle = CreateFile(wtmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (server_handle == INVALID_HANDLE_VALUE)
		throw FatalError("Obtaning server mailslot handle failed");

	ack(response);
	msg_ctr++;
}
//*********************************************************************************************************************
void NvgMutex::disconnect()
{
	CloseHandle(server_handle);
	server_handle = nullptr;
	CloseHandle(my_handle);
	my_handle = nullptr;
	msg_ctr = 0;
}
//*********************************************************************************************************************