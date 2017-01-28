#pragma once
#ifndef MAILSLOTMESSAGE_H
#define MAILSLOTMESSAGE_H

#include <cstring>

const int buf_size = 128, ack_wait_time = 500;

enum class MessageType { ack, connect, error, create_mutex=8, close_handle, wait, unlock, mutex_created=24, handle_closed, wait_finished, mutex_unlocked};

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

const LPTSTR mailslot_name = TEXT("\\\\.\\mailslot\\nvg_mutex_main");

#endif // MAILSLOTMESSAGE_H