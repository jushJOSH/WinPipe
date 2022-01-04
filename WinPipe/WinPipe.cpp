#include "WinPipe.h"
#include <stdexcept>
#include <sstream>
#include <atomic>
#include <vector>
#include <thread>

/// <summary>
/// Splittes string by delimer and create vector of words
/// </summary>
/// <param name="msg"> - Text to tokenize</param>
/// <param name="delim"> - Delimer</param>
/// <returns>Vector of word, which was separated by delimer</returns>
std::vector<std::string> split(const std::string& msg, char delim)
{
	std::vector<std::string> result;
	std::stringstream msgStream(msg);
	std::string msgToken;

	while (std::getline(msgStream, msgToken, delim))
		result.push_back(msgToken);

	return result;
}

WinPipe::WinPipe(const std::string& PipeName, unsigned int Delay, unsigned int Retries)
	:	stopRequested(false), threadFinished(false), Delay(Delay), Retries(Retries)
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
	// Closing pipe and then create new
	this->PipeName = PipeName;
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

void WinPipe::subscribeTopic(const std::string& Topic,const std::function<void(const std::string&)> &Callback)
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
		// While reading from pipe 
		while (ReadFile(PipeHandle, buf, sizeof(buf) - 1, &dwRead, NULL) != FALSE)
		{
			buf[dwRead] = '\0';

			// Split message by topic and message. If less than 2 parts than false
			std::vector<std::string> splitted = split(buf, customDelimer);
			if (splitted.size() < 2) return;

			// Trying call callback for received topic with received message
			// on fail catching exception
			try
			{
				std::thread thr(Callbacks.at(splitted[0]), std::string(buf));
				thr.detach();
			}
			catch (std::exception& e)
			{
				printf("Topic %s doesnt exist. %s\n", splitted[0].c_str(), e.what());
			}

			// Sending feedback about received message;
			_itoa_s(strlen(buf), buf, 10);
			WriteFile(PipeHandle, buf, strlen(buf), NULL, NULL);
		}
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
