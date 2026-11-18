#pragma once

#include <optional>

#include "SDL_compat.h"

struct ControllerIntent
{
    enum class Action
    {
        NavigateUp,
        NavigateDown,
        NavigateLeft,
        NavigateRight,
        Confirm,
        Back,
        OpenContextMenu,
        OpenSettings,
    };

    Action action;

    static std::optional<ControllerIntent> fromButton(Uint8 button);
    static std::optional<ControllerIntent> fromAxis(Sint16 leftX, Sint16 leftY);
};
