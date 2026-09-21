# macOS Joy-Con keyboard/mouse bridge

This fork preserves the keyboard/mouse v2 implementation tested on an Apple Silicon Mac with Minecraft Bedrock 1.26.51.1 (arm64-v8a). The GLFW backend compiles the controller source as Objective-C++ with ARC and links GameController.framework. Other backends are unchanged.

Apple's fused `Joy-Con (L/R)` state is read during the regular window poll. It uses existing in-game keyboard/mouse callbacks, without registering an Android gamepad or injecting a dynamic library. Losing focus or disconnecting releases held inputs.

| Input | Action |
| --- | --- |
| Left stick | WASD, digital movement threshold 0.25 |
| Right stick | Camera at 1300 input pixels/second; menu cursor at 650 |
| D-pad up / left / right / down | F5 perspective / B emote / T chat / Q drop |
| D-pad in menus | Arrow keys |
| ZR / ZL | Left / right mouse button |
| L / R | Previous / next hotbar item |
| Plus / minus | Escape / Tab |
| Left stick press | Ctrl sprint |
| Apple logical A | Space jump; mouse click in menus |
| Apple logical B | Shift sneak; Escape in menus |
| Apple logical X / Y | E inventory / Q drop |

Face-button labels above are Apple's logical names; their correspondence with physical Nintendo labels has not been separately validated. Stick dead zone is 0.15, elapsed-time cap is 50 ms, and fractional camera deltas are accumulated before sending integer pixels. This avoids losing subpixel movement at high polling rates. Mouse sensitivity in the game still affects the result.

## Evidence and limits

In the September 2026 investigation, an earlier Android-gamepad bridge crashed while opening a world, including an offline copy. The same copy opened in an unmodified source-built client. This keyboard/mouse v2 variant subsequently opened the world and accepted Joy-Con movement, camera, buttons, and revised D-pad controls, as reported by the tester.

The precise cause of the earlier gamepad crash is not established. LAN multiplayer, long sessions, reconnect behavior, and restart stability of this final variant remain unverified. This is not a complete native-controller UI implementation: walking is digital and menus use keyboard/mouse semantics. Preserve the tested implementation before making further changes.

Original project licensing remains in `LICENSE`. Integration and reproducible-build scripts are maintained in the companion manifest fork.
