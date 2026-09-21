# macOS Joy-Con gamepad default

Apple GameController's paired Joy-Con (L/R) is sent through the normal gamepad callbacks.
The default is `full`: buttons, both analog sticks, and triggers. The existing keyboard/mouse
translation remains available with `MCPELAUNCHER_JOYCON_MODE=keyboard`.

**Client compatibility:** the Minecraft launcher must use `AINPUT_SOURCE_JOYSTICK` for
GameActivity motion events. The corresponding manifest change applies that fix at build
configuration; using this default with an uncorrected client reproduced a crash.

`MCPELAUNCHER_JOYCON_MODE` also accepts `connect`, `neutral`, `buttons`, `left`, and `right`
for diagnostics. The former `MCPELAUNCHER_JOYCON_PROBE` variable remains a fallback alias.
`MCPELAUNCHER_JOYCON_PROBE_GATE` optionally controls registration via a file's existence.
Unknown mode values use the keyboard fallback. Gamepad modes suppress GLFW gamepad
registration to avoid duplicate input from the paired Joy-Con.

Minecraft 1.26.51.1 on Apple Silicon: the user verified menus, world loading, movement,
view control, and both triggers without a crash using the full mode and corrected client.
Long sessions, reconnection, restart, LAN and other controllers have not been verified.
Disconnect state release and button edges during the client's input-switch delay remain
known limitations. See the manifest's `joycon/experiments/REVIEW.md` for the evidence.
