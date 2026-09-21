#include "joystick_manager_glfw.h"

#include <cstring>
#ifdef __APPLE__
#import <GameController/GameController.h>
#include <cmath>
#include <cstdio>
static bool nativeJoyLogged = false;
static bool nativeKeys[18]{};
static double nativeMouseRemainderX = 0, nativeMouseRemainderY = 0;
static bool nativeWasGameplay = false;
static bool nativeMouse[2]{};
static bool nativeShoulders[2]{};
static auto nativeLastPoll = std::chrono::steady_clock::now();
static GLFWGameWindow* nativeJoyWindow = nullptr;

#endif
#include <fstream>
#include "window_glfw.h"
#include "joystick_manager.h"
#include "game_window_manager.h"

std::unordered_set<GLFWGameWindow*> GLFWJoystickManager::windows;
GLFWGameWindow* GLFWJoystickManager::focusedWindow;
std::unordered_map<int, GLFWJoystickManager::JoystickInfo> GLFWJoystickManager::connectedJoysticks;
std::unordered_set<int> GLFWJoystickManager::userIds;

void GLFWJoystickManager::init() {
    glfwSetJoystickCallback(_glfwJoystickCallback);
    loadMappingsFromFile("gamecontrollerdb.txt");
}

void GLFWJoystickManager::loadMappingsFromFile(std::string const& path) {
    std::ifstream fs(path);
    if (!fs)
        return;
    std::string line;
    while (std::getline(fs, line)) {
        if (!line.empty())
            loadMappings(line);
    }
}

void GLFWJoystickManager::loadMappings(const std::string &content) {
    glfwUpdateGamepadMappings(content.c_str());
}

int GLFWJoystickManager::nextUnassignedUserId() {
    for (int i = 0; ; i++) {
        if (userIds.count(i) <= 0)
            return i;
    }
}

void GLFWJoystickManager::update(GLFWGameWindow* window) {
    if (!window) return;
#ifdef __APPLE__
    updateNativeJoyCon(window);
#endif
    if (focusedWindow != window) return;

    for (auto& j : connectedJoysticks) {
        GLFWgamepadstate state;
        glfwGetGamepadState(j.first, &state);
        for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
            if (state.buttons[i] != j.second.oldButtonStates[i]) {
                window->onGamepadButton(j.second.userId, mapButtonId(i), state.buttons[i] != 0);
            }
        }
        for (int i = 0; i <= GLFW_GAMEPAD_AXIS_LAST; i++) {
            float value = state.axes[i];
            switch(i) {
            case GLFW_GAMEPAD_AXIS_LEFT_TRIGGER:
            case GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER:
                value = value / 2.0f + 0.5f;
            break;
            }
            window->onGamepadAxis(j.second.userId, mapAxisId(i), value);
        }

        memcpy(j.second.oldButtonStates, state.buttons, GLFW_GAMEPAD_BUTTON_LAST + 1);
    }
}

void GLFWJoystickManager::update() {
    update(focusedWindow);
}

void GLFWJoystickManager::addWindow(GLFWGameWindow* window) {
    windows.insert(window);
    if (windows.size() == 1) {
        // First window created poll all joysticks for valid mappings etc.
        // Doing this earlier can cause unintenional errors if muliple mapping are added before the first window is created
        for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
            if (glfwJoystickPresent(i)) {
                _glfwJoystickCallback(i, GLFW_CONNECTED);
            }
        }
    } else {
        // Only newly added window gets the events
        for (auto& joystick : connectedJoysticks)
            window->onGamepadState(joystick.second.userId, true);
    }
}

void GLFWJoystickManager::removeWindow(GLFWGameWindow* window) {
#ifdef __APPLE__
    if(nativeJoyWindow == window) {
        nativeJoyWindow = nullptr;
        memset(nativeKeys, 0, sizeof(nativeKeys));
        memset(nativeMouse, 0, sizeof(nativeMouse));
        memset(nativeShoulders, 0, sizeof(nativeShoulders));
    }
#endif
    windows.erase(window);
}

void GLFWJoystickManager::onWindowFocused(GLFWGameWindow* window, bool focused) {
    if (focused)
        focusedWindow = window;
    else if (focusedWindow == window /* && !focused */)
        focusedWindow = nullptr;
}

void GLFWJoystickManager::_glfwJoystickCallback(int joystick, int action) {
    auto js = connectedJoysticks.find(joystick);
    int userId;
    if (action == GLFW_CONNECTED) {
        if (!glfwJoystickIsGamepad(joystick)) {
            if (windows.empty()) {
                // No Warning before first window is created
                return;
            }
            int axis, hats, buttons;
            if (!glfwGetJoystickAxes(joystick, &axis)) {
                axis = 0;
            }
            if (!glfwGetJoystickHats(joystick, &hats)) {
                hats = 0;
            }
            if (!glfwGetJoystickButtons(joystick, &buttons)) {
                buttons = 0;
            }
            if(!JoystickManager::handleMissingGamePadMapping(glfwGetJoystickName(joystick), glfwGetJoystickGUID(joystick), axis, buttons, hats, [&](std::string mapping) {
                GameWindowManager::getManager()->addGamePadMapping(mapping);
                return glfwJoystickIsGamepad(joystick);
            })) {
                // Default mapping failed
                return;
            }
        }

        if (js != connectedJoysticks.end())
            return;
        userId = nextUnassignedUserId();
        userIds.insert(userId);
        connectedJoysticks.insert({joystick, JoystickInfo(userId)});
    } else if (action == GLFW_DISCONNECTED) {
        if (js == connectedJoysticks.end())
            return;
        userId = js->second.userId;
        userIds.erase(userId);
        connectedJoysticks.erase(joystick);
    }

    for (GLFWGameWindow* window : windows)
        window->onGamepadState(userId, action == GLFW_CONNECTED);
}

GamepadButtonId GLFWJoystickManager::mapButtonId(int id) {
    switch (id) {
        case GLFW_GAMEPAD_BUTTON_A: return GamepadButtonId::A;
        case GLFW_GAMEPAD_BUTTON_B: return GamepadButtonId::B;
        case GLFW_GAMEPAD_BUTTON_X: return GamepadButtonId::X;
        case GLFW_GAMEPAD_BUTTON_Y: return GamepadButtonId::Y;
        case GLFW_GAMEPAD_BUTTON_BACK: return GamepadButtonId::BACK;
        case GLFW_GAMEPAD_BUTTON_START: return GamepadButtonId::START;
        case GLFW_GAMEPAD_BUTTON_GUIDE: return GamepadButtonId::GUIDE;
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return GamepadButtonId::LB;
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return GamepadButtonId::RB;
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return GamepadButtonId::LEFT_STICK;
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return GamepadButtonId::RIGHT_STICK;
        case GLFW_GAMEPAD_BUTTON_DPAD_UP: return GamepadButtonId::DPAD_UP;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return GamepadButtonId::DPAD_RIGHT;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return GamepadButtonId::DPAD_DOWN;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return GamepadButtonId::DPAD_LEFT;
        default: return GamepadButtonId::UNKNOWN;
    }
}

GamepadAxisId GLFWJoystickManager::mapAxisId(int id) {
    switch (id) {
        case GLFW_GAMEPAD_AXIS_LEFT_X: return GamepadAxisId::LEFT_X;
        case GLFW_GAMEPAD_AXIS_LEFT_Y: return GamepadAxisId::LEFT_Y;
        case GLFW_GAMEPAD_AXIS_RIGHT_X: return GamepadAxisId::RIGHT_X;
        case GLFW_GAMEPAD_AXIS_RIGHT_Y: return GamepadAxisId::RIGHT_Y;
        case GLFW_GAMEPAD_AXIS_LEFT_TRIGGER: return GamepadAxisId::LEFT_TRIGGER;
        case GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER: return GamepadAxisId::RIGHT_TRIGGER;
        default: return GamepadAxisId::UNKNOWN;
    }
}
#ifdef __APPLE__
// Read native Joy-Con state, then use the existing keyboard/mouse callbacks.
// Do not register an Android gamepad: that path crashes on world load here.
void GLFWJoystickManager::updateNativeJoyCon(GLFWGameWindow* window) {
    @autoreleasepool {
        GCController* joy = nil;
        if (focusedWindow == window) {
            for (GCController* controller in GCController.controllers) {
                if ([controller.productCategory containsString:@"Joy-Con (L/R)"] && controller.extendedGamepad) {
                    joy = controller;
                    break;
                }
            }
        }
        if (nativeJoyWindow && nativeJoyWindow != window) return;
        if (!joy && !nativeJoyWindow) return;
        nativeJoyWindow = window;
        if (joy && !nativeJoyLogged) {
            fprintf(stderr, "[NativeJoyCon] Keyboard/mouse v2 input enabled for Joy-Con (L/R)\n");
            nativeJoyLogged = true;
        }
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - nativeLastPoll).count();
        nativeLastPoll = now;
        dt = std::min(0.05, std::max(0.0, dt));
        GCExtendedGamepad* p = joy.extendedGamepad;
        const bool gameplay = window->getCursorDisabled();
        const int keys[] = {GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
            GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_E, GLFW_KEY_Q,
            GLFW_KEY_ESCAPE, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_TAB,
            GLFW_KEY_F5, GLFW_KEY_B, GLFW_KEY_T, GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT};
        bool pressed[] = {
            gameplay && p.leftThumbstick.yAxis.value > 0.25f,
            gameplay && p.leftThumbstick.xAxis.value < -0.25f,
            gameplay && p.leftThumbstick.yAxis.value < -0.25f,
            gameplay && p.leftThumbstick.xAxis.value > 0.25f,
            gameplay && p.buttonA.pressed, gameplay && p.buttonB.pressed,
            p.buttonX.pressed, gameplay && (p.buttonY.pressed || p.dpad.down.pressed),
            p.buttonMenu.pressed || (!gameplay && p.buttonB.pressed),
            gameplay && p.leftThumbstickButton.pressed, p.buttonOptions.pressed,
            gameplay && p.dpad.up.pressed, gameplay && p.dpad.left.pressed, gameplay && p.dpad.right.pressed,
            !gameplay && p.dpad.up.pressed, !gameplay && p.dpad.down.pressed,
            !gameplay && p.dpad.left.pressed, !gameplay && p.dpad.right.pressed
        };
        for (int i = 0; i < 18; ++i) {
            if (pressed[i] != nativeKeys[i]) {
                GLFWGameWindow::_glfwKeyCallback(window->window, keys[i], 0,
                    pressed[i] ? GLFW_PRESS : GLFW_RELEASE, 0);
                nativeKeys[i] = pressed[i];
            }
        }
        auto axis = [](float value) {
            return std::fabs(value) < 0.15f ? 0.0f : std::copysign((std::fabs(value)-0.15f)/0.85f, value);
        };
        const double dx = axis(p.rightThumbstick.xAxis.value);
        const double dy = -axis(p.rightThumbstick.yAxis.value);
        if (!joy || gameplay != nativeWasGameplay) {
            nativeMouseRemainderX = nativeMouseRemainderY = 0;
        }
        nativeWasGameplay = gameplay;
        if (gameplay) {
            // Minecraft's direct mouse API truncates to integer pixels. Preserve
            // subpixel input across frames so camera speed is frame-rate independent.
            nativeMouseRemainderX += dx * dt * 1300.0;
            nativeMouseRemainderY += dy * dt * 1300.0;
            const double moveX = std::trunc(nativeMouseRemainderX);
            const double moveY = std::trunc(nativeMouseRemainderY);
            nativeMouseRemainderX -= moveX;
            nativeMouseRemainderY -= moveY;
            if (moveX || moveY) window->onMouseRelativePosition(moveX, moveY);
        } else if (joy) {
            const double mx = dx + axis(p.leftThumbstick.xAxis.value);
            const double my = dy - axis(p.leftThumbstick.yAxis.value);
            if (mx || my) {
                double x, y;
                int width, height;
                glfwGetCursorPos(window->window, &x, &y);
                glfwGetWindowSize(window->window, &width, &height);
                x = std::max(0.0, std::min(double(width-1), x + mx * dt * 650.0));
                y = std::max(0.0, std::min(double(height-1), y + my * dt * 650.0));
                glfwSetCursorPos(window->window, x, y);
            }
        }
        bool mouse[] = {p.rightTrigger.value > 0.5f || (!gameplay && p.buttonA.pressed),
                        p.leftTrigger.value > 0.5f};
        for (int i = 0; i < 2; ++i) {
            if (mouse[i] != nativeMouse[i]) {
                GLFWGameWindow::_glfwMouseButtonCallback(window->window,
                    i == 0 ? GLFW_MOUSE_BUTTON_LEFT : GLFW_MOUSE_BUTTON_RIGHT,
                    mouse[i] ? GLFW_PRESS : GLFW_RELEASE, 0);
                nativeMouse[i] = mouse[i];
            }
        }
        bool shoulders[] = {p.leftShoulder.pressed, p.rightShoulder.pressed};
        for (int i = 0; i < 2; ++i) {
            if (shoulders[i] && !nativeShoulders[i])
                GLFWGameWindow::_glfwScrollCallback(window->window, 0.0, i == 0 ? 1.0 : -1.0);
            nativeShoulders[i] = shoulders[i];
        }
        if (!joy) nativeJoyWindow = nullptr;
    }
}
#endif
