#pragma once

#include <cstring>
#include <string>
#include <exception>

const int buf_size = 128;	// in bytes
const DWORD read_timeout = 1000;	// in ms

enum class MessageType { ack, connect, create_mutex = 8, close_handle, wait, unlock, mutex_created = 24, handle_closed, wait_finished, mutex_unlocked };

extern wchar_t* mailslot_name;
//*********************************************************************************************************************
struct MailslotMessage
{
	MailslotMessage(int id, MessageType type, char* buf = "\0") : id(id), type(type)
	{
		memcpy(content, buf, buf_size);
	}
	MailslotMessage() { }

	MailslotMessage(MailslotMessage &other) : id(other.id), type(other.type)
	{
		memcpy(content, other.content, buf_size);
	}

	MailslotMessage& operator=(MailslotMessage &other)
	{
		id = other.id;
		type = other.type;
		memcpy(content, other.content, buf_size);
		return *this;
	}

	int id;
	MessageType type;
	char content[buf_size];
};
//*********************************************************************************************************************
class Exception : public std::exception
{
public:
	explicit Exception(const char* msg) : msg(msg) { }
	explicit Exception(const std::string& msg) : msg(msg) { }
	virtual ~Exception() noexcept { }

	virtual const char* what() const noexcept
	{
		return msg.c_str();
	}
protected:
	std::string msg;
};

class FatalError : public Exception
{
public:
	explicit FatalError(const char* msg) : Exception(msg) { }
	explicit FatalError(std::string& msg) : Exception(msg) { }
};

class ConnectionError : public Exception
{
public:
	explicit ConnectionError(const char* msg) : Exception(msg) { }
	explicit ConnectionError(std::string& msg) : Exception(msg) { }
};
//*********************************************************************************************************************