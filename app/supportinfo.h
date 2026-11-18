#pragma once

#include <QObject>

#ifndef STARLIGHT_BUILD_REVISION
#define STARLIGHT_BUILD_REVISION "unknown"
#endif

// Local, user-initiated support information. Never reads preferences or credentials.
class SupportInfo : public QObject
{
    Q_OBJECT
public:
    explicit SupportInfo(QString logPath, QObject* parent = nullptr);
    Q_INVOKABLE QString summary() const;
    Q_INVOKABLE bool copySummary() const;
    static QString redact(QString text);
    static void recordLog(const QString& text);

private:
    QString m_LogPath;
};
