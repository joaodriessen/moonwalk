#include "boxartmanager.h"
#include "../path.h"

#include <QImageReader>
#include <QImageWriter>

BoxArtManager::BoxArtManager(QObject *parent) :
    QObject(parent),
    m_BoxArtDir(Path::getBoxArtCacheDir()),
    m_ThreadPool(this)
{
    // 4 is a good balance between fast loading for large
    // app grids and not crushing GFE with tons of requests
    // and causing UI jank from constantly stalling to decode
    // new images.
    m_ThreadPool.setMaxThreadCount(4);
    if (!m_BoxArtDir.exists()) {
        m_BoxArtDir.mkpath(".");
    }
}

QString
BoxArtManager::getFilePathForBoxArt(NvComputer* computer, int appId)
{
    QDir dir = m_BoxArtDir;

    // Create the cache directory if it did not already exist
    if (!dir.exists(computer->uuid)) {
        dir.mkdir(computer->uuid);
    }

    // Change to this computer's box art cache folder
    dir.cd(computer->uuid);

    // Try to open the cached file
    return dir.filePath(QString::number(appId) + ".png");
}

class NetworkBoxArtLoadTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    NetworkBoxArtLoadTask(BoxArtManager* boxArtManager, NvComputer* computer, NvApp& app)
        : m_Bam(boxArtManager),
          m_ComputerOwner(computer->retain()),
          m_Computer(computer),
          m_App(app)
    {
    }

private:
    void run()
    {
        QUrl image = m_Bam->loadBoxArtFromNetwork(m_Computer, m_App.id);
        if (image.isEmpty() && !m_Computer->isRemoved()) {
            // Give it another shot if it fails once
            image = m_Bam->loadBoxArtFromNetwork(m_Computer, m_App.id);
        }
        auto owner = m_ComputerOwner;
        auto app = m_App;
        QMetaObject::invokeMethod(m_Bam, [owner, app, image, bam = m_Bam]() {
            if (owner && !owner->isRemoved()) {
                bam->handleBoxArtLoadComplete(owner.data(), app, image);
            }
        }, Qt::QueuedConnection);
    }

    BoxArtManager* m_Bam;
    QSharedPointer<NvComputer> m_ComputerOwner;
    NvComputer* m_Computer;
    NvApp m_App;
};

QUrl BoxArtManager::loadBoxArt(NvComputer* computer, NvApp& app)
{
    // Try to open the cached file if it exists and contains data
    QFile cacheFile(getFilePathForBoxArt(computer, app.id));
    if (cacheFile.exists() && cacheFile.size() > 0) {
        return QUrl::fromLocalFile(cacheFile.fileName());
    }

    // If we get here, we need to fetch asynchronously.
    // Kick off a worker on our thread pool to do just that.
    NetworkBoxArtLoadTask* netLoadTask = new NetworkBoxArtLoadTask(this, computer, app);
    m_ThreadPool.start(netLoadTask);

    // Return the placeholder then we can notify the caller
    // later when the real image is ready.
    return QUrl("qrc:/res/no_app_image.png");
}

void BoxArtManager::deleteBoxArt(NvComputer* computer)
{
    QDir dir(Path::getBoxArtCacheDir());

    // Delete everything in this computer's box art directory
    if (dir.cd(computer->uuid)) {
        dir.removeRecursively();
    }
}

void BoxArtManager::handleBoxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image)
{
    if (!image.isEmpty()) {
        emit boxArtLoadComplete(computer, app, image);
    }
}

QUrl BoxArtManager::loadBoxArtFromNetwork(NvComputer* computer, int appId)
{
    NvHTTP http(computer);

    QString cachePath = getFilePathForBoxArt(computer, appId);
    QImage image;
    try {
        image = http.getBoxArt(appId);
    } catch (...) {}

    // Cache the box art on disk if it loaded
    if (!image.isNull() && !computer->isRemoved()) {
        if (image.save(cachePath)) {
            if (computer->isRemoved()) {
                QFile(cachePath).remove();
                return QUrl();
            }
            return QUrl::fromLocalFile(cachePath);
        }
        else {
            // A failed save() may leave a zero byte file. Make sure that's removed.
            QFile(cachePath).remove();
        }
    }

    return QUrl();
}

#include "boxartmanager.moc"
