# Tests

Use `./dev check` for the usual checks or `./dev test NAME` for a single suite.
The [development guide](../docs/development.md) covers isolation and test
selection. Tests build separately, so production and packaging builds
don't link Qt Test.

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
