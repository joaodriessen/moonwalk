#pragma once

#include "identitymanager.h"
#include "nvhttp.h"
#include <QCryptographicHash>

#include <openssl/x509.h>
#include <openssl/evp.h>

class NvPairingManager
{
public:
    enum PairState
    {
        PAIRED,
        PIN_WRONG,
        FAILED,
        ALREADY_IN_PROGRESS
    };

    explicit NvPairingManager(NvComputer* computer, NvHTTP::CancelToken* cancelToken = nullptr);

    ~NvPairingManager();

    PairState
    pair(QString appVersion, QString pin, QSslCertificate& serverCert);

private:
    friend class NvPairingManagerTest;

    static QByteArray deriveAesKey(const QByteArray& saltedPin, QCryptographicHash::Algorithm hashAlgo);

    QByteArray
    generateRandomBytes(int length);

    QByteArray
    saltPin(const QByteArray& salt, QString pin);

    static QByteArray
    encrypt(const QByteArray& plaintext, const QByteArray& key);

    static QByteArray
    decrypt(const QByteArray& ciphertext, const QByteArray& key);

    QByteArray
    getSignatureFromCert(X509* cert);

    QByteArray
    getSignatureFromPemCert(const QByteArray& certificate);

    bool
    verifySignature(const QByteArray& data, const QByteArray& signature, const QByteArray& serverCertificate);

    QByteArray
    signMessage(const QByteArray& message);

    NvHTTP m_Http;
    X509* m_Cert;
    EVP_PKEY* m_PrivateKey;
};
