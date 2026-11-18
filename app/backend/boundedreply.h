#pragma once

#include <QNetworkReply>

// Limits Qt's own reply buffer while a synchronous request runs. Checking a
// QByteArray after readAll() is too late: the reply has already buffered it.
class BoundedReply
{
public:
    BoundedReply(QNetworkReply *reply, qint64 maxBytes) : m_Reply(reply), m_MaxBytes(maxBytes)
    {
        Q_ASSERT(reply && maxBytes > 0);
        reply->setReadBufferSize(maxBytes + 1);
        m_ReadConnection = QObject::connect(reply, &QIODevice::readyRead, reply,
                                            [this]() { checkAvailable(); });
        m_HeaderConnection = QObject::connect(reply, &QNetworkReply::metaDataChanged, reply,
                                              [this]() {
            bool valid = false;
            qint64 length = m_Reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&valid);
            if (valid && length > m_MaxBytes) {
                reject();
            }
        });
    }

    ~BoundedReply()
    {
        QObject::disconnect(m_ReadConnection);
        QObject::disconnect(m_HeaderConnection);
    }

    BoundedReply(const BoundedReply &) = delete;
    BoundedReply &operator=(const BoundedReply &) = delete;

    bool exceeded() const { return m_Exceeded; }

private:
    void checkAvailable()
    {
        if (m_Reply->bytesAvailable() > m_MaxBytes) {
            reject();
        }
    }

    void reject()
    {
        if (!m_Exceeded) {
            m_Exceeded = true;
            m_Reply->abort();
        }
    }

    QNetworkReply *m_Reply;
    qint64 m_MaxBytes;
    bool m_Exceeded = false;
    QMetaObject::Connection m_ReadConnection;
    QMetaObject::Connection m_HeaderConnection;
};
