# Dear ImGui

This directory contains the minimal Dear ImGui sources used by Xchecklist.
They are vendored from the official `ocornut/imgui` repository at tag
`v1.92.6` (commit `6ded5230d043aa32c755e65c910c2af5002fb9f9`).

Only the core library and OpenGL 2 renderer backend are included. Xchecklist
provides its own platform/input integration through the X-Plane SDK. See
`LICENSE.txt` for the upstream license.
