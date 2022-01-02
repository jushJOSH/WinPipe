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

WinPipe::WinPipe(const std::string& PipeName, SyncModel Model, unsigned int Delay)
	: PipeName(PipeName), stopRequested(false), Model(Model), Delay(Delay)
{
	if (!tryConnectPipe(PipeName))
		if (!tryCreatePipe(PipeName))
			throw std::runtime_error("Error on handling pipe");

	std::thread thr(&WinPipe::Loop, this);
	thr.detach();
}

WinPipe::~WinPipe()
{
	stopRequested = true;
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

void WinPipe::postMessage(const std::string& Topic, const std::string& Message)
{
	Sleep(Delay);
	std::string fullMessage = Topic + customDelimer + Message;
	DWORD dwWritten;

	WriteFile(PipeHandle, fullMessage.c_str(), fullMessage.size() + 1, &dwWritten, NULL);
}

void WinPipe::subscribeTopic(const std::string& Topic, std::function<void(const std::string&)> Callback)
{
	Callbacks[Topic] = Callback;
}

void WinPipe::Loop() const
{
	char buf[256];
	DWORD dwRead;

	ConnectNamedPipe(PipeHandle, NULL);
	while (!this->isStopRequested())
	{
		while (ReadFile(PipeHandle, buf, sizeof(buf) - 1, &dwRead, NULL) != FALSE)
		{
			buf[dwRead - 1] = '\0';
			std::vector<std::string> splitted = split(std::string(buf), customDelimer);

			try
			{
				std::thread thread(Callbacks.at(splitted[0]), splitted[1]);
				thread.detach();
			}
			catch (std::exception& e)
			{
				printf("Topic doesnt exist. %s\n", e.what());
			}
		}
	}
	DisconnectNamedPipe(PipeHandle);
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
	char buf[256];
	sprintf_s<256>(buf, "\\\\.\\pipe\\%s", PipeName.c_str());

	PipeHandle = CreateNamedPipeA(
		buf,
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
	char buf[256];
	sprintf_s<256>(buf, "\\\\.\\pipe\\%s", PipeName.c_str());

	PipeHandle = CreateFileA(
		buf,			// pipe name 
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
