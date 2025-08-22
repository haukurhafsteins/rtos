# RTOS Abstraction Layer

The goal of this system is to abstract RTOS.

**Supported systems**
- FreeRTOS
- Zephyr

In your makefile define one of the following for the OS to use:
* `USE_FREERTOS`:
* `USE_ZEPHYR`:

## Tasks
### The Task Class
#### Singleton
```c++
// Header file
#include <RtosMsgBufferTask.hpp>
#include <SingletonTask.hpp>
#include <QMsg.hpp>

// List of commands the task can receive
enum class MyCmd
{
    Msg1,
    Msg2
};

// Message payload definitions. Msg1 has no payload
class MyMsg2 {
public:
    int var1;
};

// Provide the maximum possible message size
class MyTask : public RtosMsgBufferTask<sizeof(QMsg<myCmd, MyMsg2>)>, public SingletonTask<MyTask>
{
public:
    MyTask(const char *name, size_t stackSize, int priority, size_t qByteSize);

    void sendMsg1();
    void sendMsg2(MyMsg2 &msg2);

protected:
    void handleMessage(const void *data, size_t len) override;
    void handleTimeout() override;
};
```

```c++
// Source File

//---------------------------------------------------------------------------
// Class initialization
//---------------------------------------------------------------------------
MyTask::MyTask(const char *name, size_t stackSize, int priority, size_t qByteSize)
    : RtosMsgBufferTask(name, stackSize, priority, qByteSize)
{
    ...
}

//---------------------------------------------------------------------------
// Message reception
//---------------------------------------------------------------------------
void MyTask::handleMessage(const void *data, size_t len)
{
    QMsg<MyCmd, uint8_t> *msg = (QMsg<MyCmd, uint8_t> *)data;
    switch (msg->cmd)
    {
    case MyCmd::Msg1:
        ...
        break;
    case MyCmd::Msg2:
    {
        QMsg<MyCmd, MyMsg2> *msg = (QMsg<MyCmd, MyMsg2> *)data;
        auto val = msg->getData();
        ...
        break;
    }
}

void MyTask::handleTimeout()
{
    ...
}

//---------------------------------------------------------------------------
// Message sending
//---------------------------------------------------------------------------
void MyTask::sendMsg1() 
{
    QMsg msg(MyCmd::Msg1);
    send(&msg, msg.size());
}
void MyTask::sendMsg2(MyMsg2 &msg2)
{
    QMsg msg(MyCmd::Msg2, msg2);
    send(&msg, msg.size());
}
```

### Start a Task
#### Multiple Instances
```c++
// name, stack size, priority, queue byte size
TheTask mytask1("task1", 4096, 3, 256);
TheTask mytask2("task2", 4096, 3, 256);
```
#### Singleton
```c++
// Define globally the _instance variable for the singleton
template <> MyTask *SingletonTask<GestureTask>::_instance = nullptr;
...
void main(void)
{
    ...
    // Create the task
    static MyTask mytask("myTaskName", 4096, 3, 256); // Create
    MyTask::bind(mytask);   // Bind to _instance
    ...

    // Access the instance from anywhere
    MyTask::get().somePublicFunction(...);
}
```

## TODO
- [ ] Add I2C abstraction
- [ ] Add Gattserver abstraction
