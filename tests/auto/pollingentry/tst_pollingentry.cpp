#include <QSemaphore>
#include <QtTest>

#include "backend/computermanager.h"

class PausedPollThread : public QThread
{
public:
    QSemaphore started;
    QSemaphore release;

    void run() override
    {
        started.release();
        release.acquire();
    }
};

class PollingEntryTest : public QObject
{
    Q_OBJECT

private slots:
    void interruptedPollerRemainsInFlightUntilItExits();
};

void PollingEntryTest::interruptedPollerRemainsInFlightUntilItExits()
{
    ComputerPollingEntry entry;
    auto* thread = new PausedPollThread;
    entry.setActiveThread(thread);
    thread->start();
    QVERIFY(thread->started.tryAcquire(1, 1000));

    entry.interrupt();
    QVERIFY(!entry.isActive());
    QVERIFY(entry.hasRunningInactiveThreads());

    thread->release.release();
    QVERIFY(thread->wait(1000));
    QVERIFY(!entry.hasRunningInactiveThreads());
}

QTEST_APPLESS_MAIN(PollingEntryTest)

#include "tst_pollingentry.moc"
