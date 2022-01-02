#pragma once
#include <string>
#include <Windows.h>
#include <functional>
#include <map>
#include <atomic>

/// <summary>
/// Class to easy managing pipes on win32 systems
/// </summary>
class WinPipe
{
public:
	enum class SyncModel
	{
		Sync, Async
	};

	/// <summary>
	/// Constructor of pipe
	/// </summary>
	/// <param name="PipeName"> - Channel name</param>
	/// <param name="model"> - Sync or Async model. I thing it doesnt work</param>
	/// <param name="Delay"> - For waiting messages. Cant make without it</param>
	WinPipe(const std::string& PipeName, SyncModel model = SyncModel::Async, unsigned int Delay = 1);
	~WinPipe();

	/// <summary>
	/// Sets custom delimer for your messages
	/// </summary>
	/// <param name="delim"> - Your delimer</param>
	void setCustomDelimer(char delim);

	/// <summary>
	/// Returnes current delimer for messages
	/// </summary>
	char getCustomDelimer() const;


	/// <summary>
	/// Setting pipe name
	/// </summary>
	/// <param name="PipeName"> - New pipe name</param>
	void setPipeName(const std::string& PipeName);

	/// <summary>
	/// Get name of pipe
	/// </summary>
	/// <returns>Pipe name</returns>
	std::string getPipeName() const;


	/// <summary>
	/// Posting message with topic to pipeline
	/// </summary>
	/// <param name="Topic"> - Theme of message, like id</param>
	/// <param name="Message"> - Message to be sent</param>
	void postMessage(const std::string& Topic, const std::string& Message);

	/// <summary>
	/// Subscribes to topic, and when message with this topic arrives, callbacks
	/// </summary>
	/// <param name="Topic"> - Name of topic to subscribe, like id</param>
	/// <param name="Callback"> - Callback</param>
	void subscribeTopic(const std::string& Topic, std::function<void(const std::string&)> Callback);

	/// <summary>
	/// Whenever the stop requested
	/// </summary>
	/// <returns>True if requested, else false</returns>
	const std::atomic_bool& isStopRequested() const;

	/// <summary>
	/// Requesting a stop in loop method
	/// </summary>
	void requestStop();

private:
	std::map<std::string, std::function<void(const std::string&)>> Callbacks;

	std::string PipeName;
	HANDLE PipeHandle;

	char customDelimer = ':';
	std::atomic<bool> stopRequested;

	SyncModel Model;
	unsigned int Delay;

	void Loop() const;
	bool tryCreatePipe(const std::string& PipeName);
	bool tryConnectPipe(const std::string& PipeName);
};