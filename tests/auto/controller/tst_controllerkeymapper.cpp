#include <QtTest>

#include "../../../app/gui/sdlgamepadkeynavigation.h"

class ControllerKeyMapperTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsButtonsToNavigationKeys_data();
    void mapsButtonsToNavigationKeys();
    void mapsAxesWithTheSameNavigationMode();
    void pollingRequiresBothEnablementAndFocus();
    void firstPollRequestsAQueueFlush();
    void axisNavigationRepeatsAt150Milliseconds();
};

void ControllerKeyMapperTest::mapsButtonsToNavigationKeys_data()
{
    QTest::addColumn<int>("button");
    QTest::addColumn<bool>("uiNavigationMode");
    QTest::addColumn<bool>("expectedMapping");
    QTest::addColumn<int>("expectedKey");
    QTest::addColumn<int>("expectedModifiers");

    QTest::newRow("normal dpad up remains an up arrow")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_UP) << false << true
            << static_cast<int>(Qt::Key_Up) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("UI dpad up becomes back-tab")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_UP) << true << true
            << static_cast<int>(Qt::Key_Tab) << static_cast<int>(Qt::ShiftModifier);
    QTest::newRow("normal dpad down remains a down arrow")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_DOWN) << false << true
            << static_cast<int>(Qt::Key_Down) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("UI dpad down becomes tab")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_DOWN) << true << true
            << static_cast<int>(Qt::Key_Tab) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("left remains an arrow in UI mode")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_LEFT) << true << true
            << static_cast<int>(Qt::Key_Left) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("right remains an arrow in UI mode")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) << true << true
            << static_cast<int>(Qt::Key_Right) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("normal confirm is return")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_A) << false << true
            << static_cast<int>(Qt::Key_Return) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("UI confirm is space")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_A) << true << true
            << static_cast<int>(Qt::Key_Space) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("cancel is escape")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_B) << false << true
            << static_cast<int>(Qt::Key_Escape) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("context is menu")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_X) << false << true
            << static_cast<int>(Qt::Key_Menu) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("Y signals the settings hangup shortcut")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_Y) << false << true
            << static_cast<int>(Qt::Key_Hangup) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("start signals the settings hangup shortcut")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_START) << true << true
            << static_cast<int>(Qt::Key_Hangup) << static_cast<int>(Qt::NoModifier);
    QTest::newRow("unmapped shoulder button has no synthetic key")
            << static_cast<int>(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) << false << false
            << static_cast<int>(Qt::Key_unknown) << static_cast<int>(Qt::NoModifier);
}

void ControllerKeyMapperTest::mapsButtonsToNavigationKeys()
{
    QFETCH(int, button);
    QFETCH(bool, uiNavigationMode);
    QFETCH(bool, expectedMapping);
    QFETCH(int, expectedKey);
    QFETCH(int, expectedModifiers);

    const auto mapping = ControllerKeyMapper::mapButton(static_cast<Uint8>(button), uiNavigationMode);

    QCOMPARE(mapping.has_value(), expectedMapping);
    if (expectedMapping) {
        QCOMPARE(static_cast<int>(mapping->key), expectedKey);
        QCOMPARE(static_cast<int>(mapping->modifiers), expectedModifiers);
    }
}

void ControllerKeyMapperTest::mapsAxesWithTheSameNavigationMode()
{
    const auto normalUp = ControllerKeyMapper::mapAxis(0, -30001, false);
    QVERIFY(normalUp.has_value());
    QCOMPARE(normalUp->key, Qt::Key_Up);
    QCOMPARE(normalUp->modifiers, Qt::NoModifier);

    const auto uiUp = ControllerKeyMapper::mapAxis(0, -30001, true);
    QVERIFY(uiUp.has_value());
    QCOMPARE(uiUp->key, Qt::Key_Tab);
    QCOMPARE(uiUp->modifiers, Qt::ShiftModifier);

    const auto diagonal = ControllerKeyMapper::mapAxis(32000, -32000, false);
    QVERIFY(diagonal.has_value());
    QCOMPARE(diagonal->key, Qt::Key_Up);

    const auto neutral = ControllerKeyMapper::mapAxis(30000, 30000, false);
    QVERIFY(!neutral.has_value());
}

void ControllerKeyMapperTest::pollingRequiresBothEnablementAndFocus()
{
    QVERIFY(ControllerKeyMapper::shouldPoll(true, true));
    QVERIFY(!ControllerKeyMapper::shouldPoll(true, false));
    QVERIFY(!ControllerKeyMapper::shouldPoll(false, true));
    QVERIFY(!ControllerKeyMapper::shouldPoll(false, false));
}

void ControllerKeyMapperTest::firstPollRequestsAQueueFlush()
{
    QVERIFY(ControllerKeyMapper::shouldFlushFirstPoll(true));
    QVERIFY(!ControllerKeyMapper::shouldFlushFirstPoll(false));
}

void ControllerKeyMapperTest::axisNavigationRepeatsAt150Milliseconds()
{
    QVERIFY(!ControllerKeyMapper::canRepeatAxisNavigation(149, 0));
    QVERIFY(ControllerKeyMapper::canRepeatAxisNavigation(150, 0));
    QVERIFY(ControllerKeyMapper::canRepeatAxisNavigation(25, UINT32_MAX - 130));
}

QTEST_GUILESS_MAIN(ControllerKeyMapperTest)

#include "tst_controllerkeymapper.moc"
