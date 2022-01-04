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

WinPipe::WinPipe(const std::string& PipeName, SyncModel Model, unsigned int Delay, unsigned int Retries)
	:	stopRequested(false), threadFinished(false), Model(Model), Delay(Delay), Retries(Retries)
{
	char buf[256];
	sprintf_s<256>(buf, "\\\\.\\pipe\\%s", PipeName.c_str());
	this->PipeName = std::string(buf);

	if (!tryConnectPipe(this->PipeName))
		if (!tryCreatePipe(this->PipeName))
			throw std::runtime_error("Error on handling pipe");

	thread = std::thread(&WinPipe::Loop, this);
	thread.detach();
}

WinPipe::~WinPipe()
{
	requestStop();

	while (Messages);
	while (!stopRequested);
	while (!threadFinished);

	printf("Destroyed\n");

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
	std::string writeBuf = Topic + customDelimer + Message;
	char readBuf[256];
	DWORD dwRead;

	unsigned int currentRetries = 0;
	++Messages;

	do
	{
		TransactNamedPipe(PipeHandle, (LPVOID)writeBuf.c_str(), writeBuf.size(), readBuf, 256, &dwRead, NULL);
	} while ([&]() -> bool
		{
			readBuf[dwRead] = '\0';
			if (dwRead == 0 && currentRetries++ != Retries && std::string(readBuf) == writeBuf)
			{
				Sleep(Delay);
				return true;
			}
			else return false;
		}());

	--Messages;
	return dwRead != 0;
}

void WinPipe::subscribeTopic(const std::string& Topic, std::function<void(const std::string&)> Callback)
{
	Callbacks[Topic] = Callback;
}

void WinPipe::Loop()
{
	char buf[256];
	DWORD dwRead;

	while (!this->isStopRequested() && PipeHandle != INVALID_HANDLE_VALUE)
	{
		while (ReadFile(PipeHandle, buf, sizeof(buf) - 1, &dwRead, NULL) != FALSE)
		{
			buf[dwRead] = '\0';

			std::thread thr([&](const std::string& readMsg)
				{
					std::vector<std::string> splitted = split(readMsg, customDelimer);
					if (splitted.size() < 2) return;

					try
					{
						Callbacks.at(splitted[0])(splitted[1]);
					}
					catch (std::exception& e)
					{
						printf("Topic doesnt exist. %s\n", e.what());
					}
				}, buf);
			thr.detach();

			WriteFile(PipeHandle, buf, strlen(buf), NULL, NULL);
		}
	}
	
	printf("Finished\n");

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

/// <summary>
/// Trying to create pipe with given pipe name
/// </summary>
/// <param name="PipeName"> - Name of pipe</param>
/// <returns>
/// Returns true on success, else false
/// </returns>
bool WinPipe::tryCreatePipe(const std::string& PipeName)
{
	PipeHandle = CreateNamedPipeA(
		PipeName.c_str(),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | (int)this->Model,   // FILE_FLAG_FIRST_PIPE_INSTANCE is not needed but forces CreateNamedPipe(..) to fail if the pipe already exists...
		1,
		256 * 16,
		256 * 16,
		0,
		NULL);

	return PipeHandle != INVALID_HANDLE_VALUE;
}

/// <summary>
/// Trying to connect with given pipe name
/// </summary>
/// <param name="PipeName"> - Name of pipe</param>
/// <returns>
/// Returns true on success, else false
/// </returns>
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

	DWORD dwMode = PIPE_READMODE_MESSAGE | (int)this->Model;
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
