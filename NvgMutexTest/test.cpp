#include <windows.h>
#include "..\NvgMutexLib\NvgMutex.h"
#include <iostream>
#include <string>
#include <ctime>

std::string mutex_name;
int repetitions;
//*********************************************************************************************************************
DWORD WINAPI artist_thread(LPVOID param)
{
	try
	{
		auto character = reinterpret_cast<char*>(param);
		srand(static_cast<unsigned int>(time(nullptr)) + GetCurrentThreadId());

		NvgMutex m(mutex_name);

		int local_repetitions = 0;
		while (local_repetitions < repetitions)
		{
			auto char_no = rand() % 20;

			m.lock();

			for (int i = 0; i < char_no + 1; i++)
				std::cout << " ";
			std::cout << "]\r[";

			for (int i = 0; i < char_no; i++)
			{
				std::cout << *character;
				Sleep(rand() % 100);
			}

			std::cout << std::endl;

			m.unlock();
			local_repetitions++;
		}
	}
	catch (ConnectionError &e)
	{
		std::cout << "Connection error, goodbye server! What happened: " << e.what() << std::endl;
	}
	catch (FatalError &e)
	{
		std::cout << "This should never happen, inspect your code. What happened: " << e.what() << std::endl;
	}
	return 0;
}
//*********************************************************************************************************************
void artist()
{
	int thread_no;

	std::cout << "Mutex name: ";
	std::cin >> mutex_name;
	std::cout << "Number of threads: ";
	std::cin >> thread_no;
	std::cout << "Characters to write: ";
	auto characters = new char[thread_no];
	for (int i = 0; i < thread_no; i++)
		std::cin >> characters[i];
	std::cout << "Number of repetitions: ";
	std::cin >> repetitions;

	auto threads = new HANDLE[thread_no];
	for (int i = 0; i < thread_no; i++)
		threads[i] = CreateThread(nullptr, 0, artist_thread, &characters[i], 0, nullptr);

	WaitForMultipleObjects(thread_no, threads, true, INFINITE);
	for (int i = 0; i < thread_no; i++)
		CloseHandle(threads[i]);

	delete[] characters;
	delete[] threads;
}
//*********************************************************************************************************************
DWORD WINAPI mutex_reuse_second_thread(LPVOID param)
{
	NvgMutex m("xyz");
	m.lock();
	std::cout << "Second thread: mutex locked, success!\n";
	return 0;
}
//*********************************************************************************************************************
void mutex_reuse()
{
	{
		NvgMutex m("xyz");
		std::cout << "Mutex created\n";
		m.lock();
		std::cout << "Mutex locked\n";
	}
	std::cout << "Mutex destroyed\n";
	auto t = CreateThread(nullptr, 0, mutex_reuse_second_thread, nullptr, 0, nullptr);
	WaitForSingleObject(t, INFINITE);
	CloseHandle(t);
}
//*********************************************************************************************************************
DWORD WINAPI mutex_lock(LPVOID param)
{
	NvgMutex m("xyz");
	m.lock(1000);
	Sleep(5000);
	m.unlock();
	printf("Goodbye\n");
	return 0;
}
//*********************************************************************************************************************
void timeout()
{
	try
	{
		NvgMutex m("xyz");
		auto second = CreateThread(nullptr, 0, mutex_lock, nullptr, 0, nullptr);
		Sleep(1000);
		std::cout << "Trying to lock mutex with timeout = 2000 ms\n";
		auto locked = m.lock(2000);
		if (locked)
			std::cout << "Mutex locked successfully\n";
		else
			std::cout << "Unable to lock mutex, time expired!\n";
		WaitForSingleObject(second, INFINITE);
		CloseHandle(second);
	}
	catch (ConnectionError &e)
	{
		std::cout << "Connection error, goodbye server! What happened: " << e.what() << std::endl;
	}
	catch (FatalError &e)
	{
		std::cout << "This should never happen, inspect your code. What happened: " << e.what() << std::endl;
	}
}
//*********************************************************************************************************************
int main()
{
	try
	{
		std::cout << "Choose test:\n1. Artist\n2. Mutex reuse\n3. Timeout\n";
		int test;
		std::cin >> test;

		switch (test)
		{
		case 1:
			artist();
			break;
		case 2:
			mutex_reuse();
			break;
		case 3:
			timeout();
			break;
		}
		system("pause");
	}
	catch (...)
	{
		std::cout << "Something wrong happened in the client";
	}
}
//*********************************************************************************************************************