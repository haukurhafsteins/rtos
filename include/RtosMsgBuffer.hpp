#pragma once

class RtosMsgBuffer {
public:
    RtosMsgBuffer(size_t bufferSize);
    ~RtosMsgBuffer();
    bool send(const void* data, size_t size, TickType_t timeout = portMAX_DELAY);
    size_t receive(void* msgBuf, size_t msgBufSize, TickType_t timeout = portMAX_DELAY);
    size_t size() const { return _bufferSize; }

private:
    MessageBufferHandle_t _handle;
    size_t _bufferSize;
};
