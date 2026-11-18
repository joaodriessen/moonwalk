#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

struct GnomeChromePlugin
{
    bool adwaitaKey = false;
    bool qAdwaitaKey = false;
    bool loadable = false;
};

struct GnomeChromeInputs
{
    bool explicitDecoration = false;
    bool waylandSession = false;
    QString activeSession;
    QVector<GnomeChromePlugin> plugins;
};

struct GnomeChromeDecision
{
    enum Kind {
        PreserveExplicit,
        QAdwaita,
        Adwaita,
        QtDefault,
    };

    Kind kind = QtDefault;
    QByteArray environmentValue;
};

class GnomeChromeSelector final
{
public:
    static GnomeChromeInputs inspectSystem();
    static GnomeChromeDecision select(const GnomeChromeInputs& inputs);
};

Q_DECLARE_METATYPE(GnomeChromeInputs)
Q_DECLARE_METATYPE(GnomeChromeDecision::Kind)
