#include <iostream>
#include "WinPipe.h"
#include <chrono>

using namespace std;
bool mutex = false;
void Pipe1Callback(const std::string& Message)
{
	while (mutex);
	mutex = true;
	cout << "Callback 1 " << Message << '\n';
	mutex = false;
}

void Pipe2Callback(const std::string& Message)
{
	while (mutex);
	mutex = true;
	cout << "Callback 2 " << Message << '\n';
	mutex = false;
}

void HelloWorld(const std::string& Message)
{
	while (mutex);
	mutex = true;
	cout << "Hello World! " << Message << '\n';
	mutex = false;
}

int main()
{
	// Initializing pipes
	WinPipe Pipe1("TestChannel");
	WinPipe Pipe2("TestChannel");

	// Subscribing topics to receive messages
	Pipe1.subscribeTopic("Topic1", Pipe1Callback);
	Pipe2.subscribeTopic("Topic2", Pipe2Callback);
	Pipe1.subscribeTopic("Hello World!", HelloWorld);
	Pipe2.subscribeTopic("Hello World!", HelloWorld);

	// Posting messages
	Pipe1.postMessage("Topic2", "Message1");
	Pipe2.postMessage("Topic1", "Message2");
	Pipe1.postMessage("Hello World!", "11111111111");
	Pipe2.postMessage("Hello World!", "22222222222");
}