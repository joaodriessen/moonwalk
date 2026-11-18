#pragma once

#include <QString>

class HostIdentitySource
{
public:
    virtual ~HostIdentitySource() = default;

    virtual QString uuidForRow(int row) const = 0;
};
