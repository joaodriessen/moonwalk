#pragma once
#include <QSharedPointer>
class NvComputer {
public:
    QSharedPointer<NvComputer> retain() const { return { }; }
};
