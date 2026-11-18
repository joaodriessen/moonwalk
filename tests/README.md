# Tests

Tests are built separately from the application. `moonlight-qt.pro` deliberately
does not include this directory in `SUBDIRS`, so production, embedded, and
packaging builds do not link Qt Test.

The current Linux-only qmake entry point is `tests/tests.pro`:

```sh
mkdir test-build
cd test-build
qmake6 ../tests/tests.pro
make -j"$(nproc)"
./auto/smoke/tst_smoke -txt
```

## Settings isolation

Any test that touches preferences must use an explicit `QTemporaryDir` and an
INI-format `QSettings` object rooted inside it. It must never construct default
`QSettings`, whose location can resolve to the user's real application settings.

```cpp
#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include <memory>

class PreferencesTest : public QObject {
    Q_OBJECT

private slots:
    void init() {
        QVERIFY(settingsDirectory.isValid());
        settings = std::make_unique<QSettings>(
            settingsDirectory.filePath("settings.ini"), QSettings::IniFormat);
    }

private:
    QTemporaryDir settingsDirectory;
    std::unique_ptr<QSettings> settings;
};
```

Production code under test must receive this explicit settings instance or an
equivalent test-only path/injection seam. `QStandardPaths::setTestModeEnabled()`
may be an additional safeguard, but it is not a substitute for the explicit
temporary path. Fixtures clean up with `QTemporaryDir`; tests must not read from
or write to the user's real settings.
