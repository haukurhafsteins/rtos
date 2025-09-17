#pragma once

template <class T>
struct ConfigObserver
{
    virtual ~ConfigObserver() = default;
    // Return false to veto (keeps old config).
    virtual bool on_config_apply(const T &newCfg) = 0;
};