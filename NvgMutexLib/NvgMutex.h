#pragma once

#include <windows.h>
#include <string>
#include "common.h"

class NvgMutex
{
public:
	NvgMutex(std::string name);

	NvgMutex(const NvgMutex& other) = delete; // non construction-copyable
	NvgMutex& operator=(const NvgMutex& other) = delete; // non copyable

	~NvgMutex();

	bool lock(int wait_ms = 0);		// 0 means waiting forever
	void unlock();

private:
	void send_message(MessageType type, void* content, int clen);
	MailslotMessage receive_message(DWORD timeout = INFINITE);
	void ack(MailslotMessage msg);
	void get_ack();
	void establish_connection();
	void disconnect();

	HANDLE mhandle;
	static thread_local int msg_ctr;
	static thread_local HANDLE server_handle;
	static thread_local HANDLE my_handle;
	static thread_local int existing_objects;
};