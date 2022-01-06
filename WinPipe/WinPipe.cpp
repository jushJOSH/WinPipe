#include "WinPipe.h"
#include <stdexcept>
#include <sstream>
#include <atomic>
#include <vector>
#include <thread>

WinPipe::WinPipe(const std::string& PipeName, unsigned int Delay, unsigned int Retries)
	: stopRequested(false), threadFinished(false), Delay(Delay), Retries(Retries)
{
	// PipeName init
	char buf[MAX_ALLOWED_BUFFER];
	sprintf_s<MAX_ALLOWED_BUFFER>(buf, "\\\\.\\pipe\\%s", PipeName.c_str());
	this->PipeName = std::string(buf);

	// PipeHandle init
	if (!tryConnectPipe(this->PipeName))
		if (!tryCreatePipe(this->PipeName))
			throw std::runtime_error("Error on handling pipe");

	// Thread init
	thread = std::thread(&WinPipe::Loop, this);
	thread.detach();
}

WinPipe::~WinPipe()
{
	// Asking to stop and then waiting
	requestStop();
	while (!threadFinished);

	// Closing pipes
	DisconnectNamedPipe(PipeHandle);
	CloseHandle(PipeHandle);
}

void WinPipe::setCustomDelimer(char delim)
{
	customDelimer = delim;
}

char WinPipe::getCustomDelimer() const
{
	return customDelimer;
}

void WinPipe::setPipeName(const std::string& PipeName)
{
	// PipeName init
	char buf[MAX_ALLOWED_BUFFER];
	sprintf_s<MAX_ALLOWED_BUFFER>(buf, "\\\\.\\pipe\\%s", PipeName.c_str());
	this->PipeName = std::string(buf);

	// Closing pipe and then create new
	CloseHandle(PipeHandle);
	if (!tryConnectPipe(PipeName))
		if (!tryCreatePipe(PipeName))
			throw std::runtime_error("Error on handling pipe");
}

std::string WinPipe::getPipeName() const
{
	return PipeName;
}

bool WinPipe::postMessage(const std::string& Topic, const std::string& Message)
{
	// Creating message
	std::string writeBuf = Topic + customDelimer + Message;

	// For reading response
	char readBuf[MAX_ALLOWED_BUFFER];
	DWORD dwRead;

	// For attempts
	Retries = 0;
	unsigned int currentRetries = 0;

	// Sending message to pipe and awaiting response
	do
	{
		TransactNamedPipe(PipeHandle, (LPVOID)writeBuf.c_str(), writeBuf.size(), readBuf, MAX_ALLOWED_BUFFER, &dwRead, NULL);
	} while ([&]() -> bool
		{
			// If received lenght equal to sent, then everything ok
			// Else sending message again
			if (dwRead < MAX_ALLOWED_BUFFER)
				readBuf[dwRead] = '\0';

			if (dwRead == 0 && currentRetries++ != Retries || atoi(readBuf) != writeBuf.size())
			{
				Sleep(Delay);
				return true;
			}
			else return false;
		}());

	// Return success
	return dwRead != 0;
}

void WinPipe::subscribeTopic(const std::string& Topic, const std::function<void(const std::string&)>& Callback)
{
	Callbacks[Topic] = Callback;
}

void WinPipe::Loop()
{
	char buf[MAX_ALLOWED_BUFFER];
	DWORD dwRead;

	// If stop requested or pipe broken we stop
	while (!this->isStopRequested() && PipeHandle != INVALID_HANDLE_VALUE)
	{
		std::string recvMessage;
		// While reading from pipe 
		while (ReadFile(PipeHandle, buf, sizeof(buf) - 1, &dwRead, NULL) != FALSE || GetLastError() == ERROR_MORE_DATA)
		{
			buf[dwRead] = '\0';
			recvMessage += buf;
		}

		if (recvMessage.empty())
			continue;

		// Split message by topic and message.
		size_t delimer = recvMessage.find_first_of(customDelimer);
		std::string
			topic = recvMessage.substr(0, delimer),
			message = recvMessage.substr(delimer + 1, recvMessage.size() - delimer);

		// Trying call callback for received topic with received message
		// on fail catching exception
		try
		{
			std::thread thr(Callbacks.at(topic), message);
			thr.detach();
		}
		catch (std::exception& e)
		{
			printf("Topic %s doesnt exist. %s\n", topic.c_str(), e.what());
		}

		// Sending feedback about received message;
		_itoa_s(strlen(buf), buf, 10);
		WriteFile(PipeHandle, buf, strlen(buf), NULL, NULL);
	}

	threadFinished = true;
}

const std::atomic_bool& WinPipe::isStopRequested() const
{
	return stopRequested;
}

void WinPipe::requestStop()
{
	stopRequested = true;
}

bool WinPipe::tryCreatePipe(const std::string& PipeName)
{
	PipeHandle = CreateNamedPipeA(
		PipeName.c_str(),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT,   // FILE_FLAG_FIRST_PIPE_INSTANCE is not needed but forces CreateNamedPipe(..) to fail if the pipe already exists...
		1,
		MAX_ALLOWED_BUFFER,
		MAX_ALLOWED_BUFFER,
		0,
		NULL);

	return PipeHandle != INVALID_HANDLE_VALUE;
}

bool WinPipe::tryConnectPipe(const std::string& PipeName)
{
	do
	{
		PipeHandle = CreateFileA(
			PipeName.c_str(),			// pipe name 
			GENERIC_READ |  // read and write access 
			GENERIC_WRITE,
			0,              // no sharing 
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe 
			0,              // default attributes 
			NULL);          // no template file 

		// return if pipe invalid
		if (PipeHandle == INVALID_HANDLE_VALUE)
			return false;

	} while (GetLastError() == ERROR_PIPE_BUSY);

	// The pipe connected; change to message-read mode. 

	DWORD dwMode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
	BOOL fSuccess = SetNamedPipeHandleState(
		PipeHandle,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 

	if (!fSuccess)
	{
		printf("SetNamedPipeHandleState failed. GLE=%d\n", GetLastError());
		return false;
	}

	return true;
}
