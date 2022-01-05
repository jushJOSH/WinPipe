# WinPipe
## Ez pipe for windows. ~~Kinda cringy~~

Simple tool for IPC. Communicating trough messages with topics, similar to MQTT.

> Aye aint it super cool and thank you?
Sure dude youre welcome!


> Is there any problems with it?
I think only execution time is the only limit (4.5ms for init 2 pipes, subscribe topics, and send 6 messages. Like in main.cpp example)

> Any benefits?
Sure, multithreaded read mechanism, async read/write, buffer limitation (if needed), Publisher - Subscriber system, and many more!

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
  WinPipe pipe("Pipe"); // Pipe - channel name
  
  // Topic1 - topic, where you want to receive messages
  // callback - function, where message from topic passes
  pipe.subscribeTopic("Topic1", callback);
}
```

This allows to receive messages. To send messages to topic, try something like...
```
// Topic2 - where we want to send id
// Message - message itself
pipe.postMessage("Topic2", "Message");
```
If you need to reduce or extend buffer size (default 256 per received message), you can make this define after include:
```
#include "WinPipe.h"
#define MAX_ALLOWED_BUFFER 1024 // - Any number you want
```

**Note what every callback runs in _different_ thread**

Help appreciated
