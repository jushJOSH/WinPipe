# WinPipe
Ez pipe for windows. ~~Kinda cringy~~

Simple tool for IPC. Communicating trough messages with topics, similar to MQTT.


> Aye aint it super cool and thank you?

Sure dude youre welcome!


> Is there any problems with it?

[x]Lots of. Starting with 1ms delay for posting messages. Communication with messages, NOWAIT, etc... FIXED


> Any benefits?

Ofc, multithreaded read mechanism, callback system, topics, at least better than original Named Pipe windows mechanism. Also made with STL.

# Easy start
Start with include 

```
#include "WinPipe.h"
```

And here you go:
init object, set name and subscribe topics

```
void callback(const std::string &Message)
{
  // Do sth with caught message...
}

int main()
{
  WinPipe pipe("Pipe");
  pipe.subscribeTopic("Topic1", callback); // where callback - function
}
```
It allows to receive messages. To send messages to topic, try something like...
```
pipe.postMessage("Topic2", "Message");
```

**Note what every callback runs in _different_ thread**

Help appreciated
