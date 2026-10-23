# Tick Buffer

A Windows Geode mod targeting Geometry Dash 2.2081.

Press **F6** to cycle the early-jump buffer: **2 → 4 → 8 → 16 → 24 ticks → OFF**. When a primary-button press is made while the player is airborne, the mod holds that press for the selected window and applies it on landing if the player lands before the window expires. A release made while the press is pending is preserved.

This is a small independent implementation based on the requested behavior, not a verified reconstruction of a missing original binary/source. Compatibility with CBF and Superb Input Precision still needs an in-game test, as input-processing mods can hook overlapping parts of the game.

## Build

The GitHub Actions workflow builds the Windows Geode package and uploads it as an artifact. You can also build locally with the Geode SDK installed and the GEODE_SDK environment variable set.
