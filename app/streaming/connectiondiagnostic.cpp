#include "connectiondiagnostic.h"

#include <QHostInfo>
#include <QTcpSocket>
#include <QUdpSocket>

ConnectionDiagnostic::ConnectionDiagnostic(QObject* parent) : QObject(parent)
{
    m_Deadline.setSingleShot(true);
    connect(&m_Deadline, &QTimer::timeout, this, [this] {
        finish(m_ReferenceConnected ? int(m_Failed) : -1);
    });
}

ConnectionDiagnostic::~ConnectionDiagnostic()
{
    if (m_Lookup != -1) QHostInfo::abortHostLookup(m_Lookup);
}

void ConnectionDiagnostic::start(const QString& host, quint16 referencePort,
                                 const QVector<Port>& ports, int timeoutMs)
{
    if (m_Started || m_Finished) return;
    m_Started = true;
    m_Ports = ports;
    m_ReferencePort = referencePort;
    for (const auto& port : ports) m_Pending |= port.flag;
    m_Failed = m_Pending;
    if (!m_Pending) { finish(0); return; }
    m_Deadline.start(qMax(1, timeoutMs));

    QHostAddress literal;
    if (literal.setAddress(host)) {
        m_Addresses.append(literal);
        tryReference();
    }
    else {
        m_Lookup = QHostInfo::lookupHost(host, this, [this](const QHostInfo& info) {
            m_Lookup = -1;
            if (m_Finished) return;
            for (const auto& address : info.addresses()) m_Addresses.append(address);
            tryReference();
        });
    }
}

void ConnectionDiagnostic::cancel()
{
    finish(-1);
}

void ConnectionDiagnostic::tryReference()
{
    if (m_Finished) return;
    if (m_Addresses.isEmpty()) { finish(-1); return; }
    const auto address = m_Addresses.takeFirst();
    auto socket = new QTcpSocket(this);
    m_Reference = socket;
    connect(socket, &QTcpSocket::connected, this, [this, socket, address] {
        if (m_Finished || socket != m_Reference) return;
        m_ReferenceConnected = true;
        probePorts(address);
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket] {
        if (m_Finished || m_ReferenceConnected || socket != m_Reference) return;
        socket->deleteLater();
        m_Reference = nullptr;
        tryReference();
    });
    socket->connectToHost(address, m_ReferencePort);
}

void ConnectionDiagnostic::probePorts(const QHostAddress& address)
{
    for (const auto& port : m_Ports) {
        if (port.udp) {
            auto socket = new QUdpSocket(this);
            connect(socket, &QUdpSocket::readyRead, this, [this, socket, port] {
                if (m_Finished || !(m_Pending & port.flag)) return;
                char data[1040];
                // Same echo payload/size contract as LiTestClientConnectivity.
                if (socket->readDatagram(data, sizeof(data)) != sizeof(data)) finish(-1);
                else completePort(port.flag, true);
            });
            connect(socket, &QUdpSocket::errorOccurred, this, [this, port] {
                completePort(port.flag, false);
            });
            socket->connectToHost(address, port.number);
            QByteArray packet(1040, '\0');
            packet.replace(0, 15, "moonlight-ctest");
            // Three paced datagrams, as in the existing connectivity tester.
            for (int i = 0; i < 3; ++i) {
                QTimer::singleShot(i * 50, this, [this, socket, port, packet] {
                    if (!m_Finished && (m_Pending & port.flag) && socket->write(packet) < 0)
                        completePort(port.flag, false);
                });
            }
        }
        else {
            auto socket = new QTcpSocket(this);
            connect(socket, &QTcpSocket::connected, this, [this, port] {
                completePort(port.flag, true);
            });
            connect(socket, &QTcpSocket::errorOccurred, this, [this, port] {
                completePort(port.flag, false);
            });
            socket->connectToHost(address, port.number);
        }
    }
}

void ConnectionDiagnostic::completePort(unsigned int flag, bool reachable)
{
    if (m_Finished || !(m_Pending & flag)) return;
    m_Pending &= ~flag;
    if (reachable) m_Failed &= ~flag;
    if (!m_Pending) finish(int(m_Failed));
}

void ConnectionDiagnostic::finish(int result)
{
    if (m_Finished) return;
    m_Finished = true;
    m_Deadline.stop();
    if (m_Lookup != -1) {
        QHostInfo::abortHostLookup(m_Lookup);
        m_Lookup = -1;
    }
    for (auto socket : findChildren<QAbstractSocket*>(QString(), Qt::FindDirectChildrenOnly))
        socket->abort();
    emit finished(result);
}
