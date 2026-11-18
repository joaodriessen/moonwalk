#pragma once
class WMUtils {
public:
    static bool isRunningDesktopEnvironment() { return true; }
    static bool isRunningWayland() { return false; }
};
