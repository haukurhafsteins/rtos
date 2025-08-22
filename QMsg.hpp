
#pragma once
#include <cstdint>
#include <cstring>

struct NoData {};

template <typename C, typename D = NoData>
class QMsg
{
public:
    C cmd;
    D data;

    QMsg() : cmd(), data() {}
    QMsg(C c, const D& d) : cmd(c), data(d) {}
    QMsg(C c, const D* d) : cmd(c), data(*d) {}
    QMsg(C c) : cmd(c), data() {}

    size_t size() const { return sizeof(C) + sizeof(D); }
    D& getData() { return data; }
};
