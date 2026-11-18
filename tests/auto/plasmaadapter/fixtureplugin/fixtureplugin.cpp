#include <QFile>
#include <QObject>

namespace {

class LibraryLifecycle final
{
public:
    LibraryLifecycle()
    {
        record("load\n");
    }

    ~LibraryLifecycle()
    {
        record("unload\n");
    }

private:
    static void record(const QByteArray& event)
    {
        const QString path = qEnvironmentVariable(
                    "STARLIGHT_TEST_PLASMA_PLUGIN_LIFECYCLE_FILE");
        if (path.isEmpty()) {
            return;
        }

        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            file.write(event);
        }
    }
};

LibraryLifecycle s_LibraryLifecycle;

} // namespace

class PlasmaStyleProbeFixturePlugin final : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "tests.starlight.PlasmaStyleProbeFixture")
};

#include "fixtureplugin.moc"
