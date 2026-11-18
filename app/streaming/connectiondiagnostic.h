#pragma once

#include <QObject>
#include <QHostAddress>
#include <QTimer>
#include <QVector>

class QTcpSocket;

// Optional failure diagnostics. All DNS/socket delivery is asynchronous, owned
// by this request and bounded by one deadline (not a fresh timeout per port).
class ConnectionDiagnostic : public QObject
{
    Q_OBJECT
public:
    struct Port { unsigned int flag; quint16 number; bool udp; };
    explicit ConnectionDiagnostic(QObject* parent = nullptr);
    ~ConnectionDiagnostic() override;
    void start(const QString& host, quint16 referencePort, const QVector<Port>& ports,
               int timeoutMs = 4000);
    void cancel();

signals:
    void finished(int result); // 0: reachable, -1: inconclusive, otherwise failed flags

private:
    void tryReference();
    void probePorts(const QHostAddress& address);
    void completePort(unsigned int flag, bool reachable);
    void finish(int result);

    QTimer m_Deadline;
    QVector<QHostAddress> m_Addresses;
    QVector<Port> m_Ports;
    quint16 m_ReferencePort = 0;
    QTcpSocket* m_Reference = nullptr;
    int m_Lookup = -1;
    unsigned int m_Pending = 0;
    unsigned int m_Failed = 0;
    bool m_Started = false;
    bool m_Finished = false;
    bool m_ReferenceConnected = false;
};
