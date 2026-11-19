#include "identitymanager.h"
#include "utils.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/rand.h>

#define SER_UNIQUEID "uniqueid"
#define SER_CERT "certificate"
#define SER_KEY "key"

IdentityManager* IdentityManager::s_Im = nullptr;

#ifdef Q_OS_LINUX
static bool protectSettingsFile(const QString& path)
{
    QFile file(path);
    if (!file.exists()) {
        if (!QDir().mkpath(QFileInfo(file).absolutePath())) return false;
        // Establish the mode before QSettings writes credentials. NewOnly
        // preserves the file if another instance created it in the meantime.
        if (file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) file.close();
    }
    return QFileInfo(file).isFile() &&
           file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}
#endif

IdentityManager*
IdentityManager::get()
{
    // This will always be called first on the main thread,
    // so it's safe to initialize without locks.
    if (s_Im == nullptr) {
        s_Im = new IdentityManager();
    }

    return s_Im;
}

void IdentityManager::createCredentials()
{
    X509* cert = X509_new();
    THROW_BAD_ALLOC_IF_NULL(cert);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_PKEY* pk = EVP_RSA_gen(2048);
    THROW_BAD_ALLOC_IF_NULL(pk);
#else
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    THROW_BAD_ALLOC_IF_NULL(ctx);

    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);

    // pk must be initialized on input
    EVP_PKEY* pk = NULL;
    EVP_PKEY_keygen(ctx, &pk);

    EVP_PKEY_CTX_free(ctx);
    THROW_BAD_ALLOC_IF_NULL(pk);
#endif

    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 0);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24 * 365 * 20); // 20 yrs
#else
    X509_gmtime_adj(X509_getm_notBefore(cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert), 60 * 60 * 24 * 365 * 20); // 20 yrs
#endif

    X509_set_pubkey(cert, pk);

    X509_NAME* name = X509_NAME_new();
    THROW_BAD_ALLOC_IF_NULL(name);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<unsigned char *>(const_cast<char*>("NVIDIA GameStream Client")),
                               -1, -1, 0);
    X509_set_subject_name(cert, name);
    X509_set_issuer_name(cert, name);
    X509_NAME_free(name);

    X509_sign(cert, pk, EVP_sha256());

    BIO* biokey = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(biokey);
    PEM_write_bio_PrivateKey(biokey, pk, NULL, NULL, 0, NULL, NULL);

    BIO* biocert = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(biocert);
    PEM_write_bio_X509(biocert, cert);

    BUF_MEM* mem;
    BIO_get_mem_ptr(biokey, &mem);
    m_CachedPrivateKey = QByteArray(mem->data, (int)mem->length);

    BIO_get_mem_ptr(biocert, &mem);
    m_CachedPemCert = QByteArray(mem->data, (int)mem->length);

    X509_free(cert);
    EVP_PKEY_free(pk);
    BIO_free(biokey);
    BIO_free(biocert);

    m_CachedSslCert.clear();
    m_CachedSslKey.clear();
    // Check that the new keypair is valid before persisting it
    if (getSslCertificate().isNull()) {
        qFatal("Newly generated certificate is unreadable");
    }
    if (getSslKey().isNull()) {
        qFatal("Newly generated private key is unreadable");
    }
}

IdentityManager::IdentityManager()
{
    QSettings settings;
    bool canPersist = true;
#ifdef Q_OS_LINUX
    canPersist = protectSettingsFile(settings.fileName());
    if (!canPersist) {
        qWarning() << "Could not make Moonwalk settings private. New pairing credentials will only last for this run.";
    }
#endif

    m_CachedPemCert = settings.value(SER_CERT).toByteArray();
    m_CachedPrivateKey = settings.value(SER_KEY).toByteArray();

    const bool haveCredentials = !m_CachedPemCert.isEmpty() && !m_CachedPrivateKey.isEmpty();
    const bool newCredentials = !haveCredentials || getSslCertificate().isNull() || getSslKey().isNull();
    if (newCredentials) {
        if (haveCredentials) qWarning() << "Saved pairing credentials could not be read.";
        createCredentials();
        if (canPersist) {
            settings.setValue(SER_CERT, m_CachedPemCert);
            settings.setValue(SER_KEY, m_CachedPrivateKey);
        }
    }

    // Load the unique ID from settings
    m_CachedUniqueId = settings.value(SER_UNIQUEID).toString();
    const bool newUniqueId = m_CachedUniqueId.isEmpty();
    if (newUniqueId) {
        // Generate a new unique ID in base 16
        uint64_t uid;
        RAND_bytes(reinterpret_cast<unsigned char*>(&uid), sizeof(uid));
        m_CachedUniqueId = QString::number(uid, 16);

        if (canPersist) settings.setValue(SER_UNIQUEID, m_CachedUniqueId);
    }
    if (canPersist && (newCredentials || newUniqueId)) {
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            qWarning() << "Could not save pairing credentials to Moonwalk settings.";
        }
    }
}

QSslCertificate
IdentityManager::getSslCertificate()
{
    if (m_CachedSslCert.isNull()) {
        m_CachedSslCert = QSslCertificate(m_CachedPemCert);
    }
    return m_CachedSslCert;
}

QSslKey
IdentityManager::getSslKey()
{
    if (m_CachedSslKey.isNull()) {
        // This seemingly useless const_cast is required for old OpenSSL headers
        // where BIO_new_mem_buf's parameter is not declared const like those on
        // the Steam Link hardware.
        BIO* bio = BIO_new_mem_buf(const_cast<char*>(m_CachedPrivateKey.constData()), -1);
        THROW_BAD_ALLOC_IF_NULL(bio);

        EVP_PKEY* pk = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (!pk) return {};

        bio = BIO_new(BIO_s_mem());
        THROW_BAD_ALLOC_IF_NULL(bio);

        // We must write out our PEM in the old PKCS1 format for SecureTransport
        // on macOS/iOS to be able to read it.
#ifdef Q_OS_DARWIN
        PEM_write_bio_PrivateKey_traditional(bio, pk, nullptr, nullptr, 0, nullptr, 0);
#else
        PEM_write_bio_PrivateKey(bio, pk, nullptr, nullptr, 0, nullptr, 0);
#endif

        BUF_MEM* mem;
        BIO_get_mem_ptr(bio, &mem);
        m_CachedSslKey = QSslKey(QByteArray(mem->data, (int)mem->length), QSsl::Rsa);

        BIO_free(bio);
        EVP_PKEY_free(pk);
    }
    return m_CachedSslKey;
}

QSslConfiguration
IdentityManager::getSslConfig()
{
    QSslConfiguration sslConfig(QSslConfiguration::defaultConfiguration());
    sslConfig.setLocalCertificate(getSslCertificate());
    sslConfig.setPrivateKey(getSslKey());
    // Preserve the paired peer identity on legacy hosts that do not restore
    // client authentication on a resumed TLS session. Existing authenticated
    // HTTP connections remain reusable, for polling and long-poll endpoints.
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionSharing,true);
    sslConfig.setSslOption(QSsl::SslOptionDisableSessionTickets,true);
    return sslConfig;
}

QString
IdentityManager::getUniqueId()
{
    return m_CachedUniqueId;
}

QByteArray
IdentityManager::getCertificate()
{
    return m_CachedPemCert;
}

QByteArray
IdentityManager::getPrivateKey()
{
    return m_CachedPrivateKey;
}
