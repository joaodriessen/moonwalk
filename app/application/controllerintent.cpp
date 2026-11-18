#include "controllerintent.h"

std::optional<ControllerIntent> ControllerIntent::fromButton(Uint8 button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return ControllerIntent { Action::NavigateUp };
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return ControllerIntent { Action::NavigateDown };
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return ControllerIntent { Action::NavigateLeft };
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return ControllerIntent { Action::NavigateRight };
    case SDL_CONTROLLER_BUTTON_A:
        return ControllerIntent { Action::Confirm };
    case SDL_CONTROLLER_BUTTON_B:
        return ControllerIntent { Action::Back };
    case SDL_CONTROLLER_BUTTON_X:
        return ControllerIntent { Action::OpenContextMenu };
    case SDL_CONTROLLER_BUTTON_Y:
    case SDL_CONTROLLER_BUTTON_START:
        return ControllerIntent { Action::OpenSettings };
    default:
        return std::nullopt;
    }
}

std::optional<ControllerIntent> ControllerIntent::fromAxis(
    Sint16 leftX,
    Sint16 leftY)
{
    if (leftY < -30000) {
        return ControllerIntent { Action::NavigateUp };
    }
    if (leftY > 30000) {
        return ControllerIntent { Action::NavigateDown };
    }
    if (leftX < -30000) {
        return ControllerIntent { Action::NavigateLeft };
    }
    if (leftX > 30000) {
        return ControllerIntent { Action::NavigateRight };
    }
    return std::nullopt;
}
