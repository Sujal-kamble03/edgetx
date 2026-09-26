# 128x64 Main Display

The 128x64 target uses a custom main dashboard implemented in
[`radio/src/gui/128x64/view_main.cpp`](https://github.com/EdgeTX/edgetx/blob/main/radio/src/gui/128x64/view_main.cpp).
The dashboard is drawn by `drawCustomMainDashboard()` and is called from
`menuMainView()` after main-view navigation events have been processed.

## Display state

The dashboard reads the following inputs on every display update:

| Input | Display or behavior |
| --- | --- |
| `SA` | Shows `ARMED` when high, otherwise `DISARM`. |
| `SD` | Shows `RDY` when high, otherwise `NORDY`. It also gates the movement indicators. |
| `SE` | Selects `CSU` when high, otherwise `MU`. |
| `SC` | Selects the spool in `CSU` mode and controls DF in `MU` mode. It also selects the rotation direction pair. |
| `SB` | Selects the vertical direction pair when high and the left/right pair when low. |
| Potentiometer 2 | Maps its value to a ten-segment speed indicator. |
| Elevator stick | Selects forward/back or clockwise/counter-clockwise direction. |
| Aileron stick | Selects right/left direction when the left/right pair is enabled. |

The display also shows connection and transmitter battery information. These
indicators are currently fixed by the dashboard implementation rather than by
the selectable legacy main-view pages. 

## Mode-change transition

`updateDashboardModeState()` watches `SE`. The first update records the current
mode without showing a prompt. When `SE` changes afterward, the dashboard enters
the pending state and displays either `CSU MODE` or `MU MODE`, followed by
`SET SC HIGH`.

The prompt remains visible until `SC` has completed this acknowledgment:

1. Move `SC` away from high.
2. Return `SC` to high.

This prevents a mode change from immediately being interpreted as a movement
command. The transition state is held by `dashboardDirectionPending` and
`dashboardTransitionScWentAway`.

## Conditional movement indicators

Movement indicators are enabled only when all relevant conditions are true:

- `MU` mode, `SD` high, and `SB` high: elevator selects forward or back.
- `MU` mode, `SD` high, and `SC` high: elevator selects clockwise or
  counter-clockwise.
- `MU` mode, `SD` high, and `SB` low: aileron selects right or left.

The dashboard remembers which of `SB` or `SC` was moved most recently. Only the
corresponding direction pair is then eligible for highlighting. If both switches
change during one update, the `SC` assignment is processed last and therefore
takes precedence.

When a direction is selected, its cell is filled continuously. When a pair is
enabled but no direction is selected, the pair blinks with `BLINK_ON_PHASE`.
The DF indicator is shown as active only when `SC` was the most recently moved
switch and DF is enabled (`MU` mode with `SC` low).

## Rendering order

The main-display path is:

1. `menuMainView()` updates the dashboard mode state.
2. A pending mode transition draws the transition prompt and returns.
3. Main-view navigation and menu events are handled.
4. `drawCustomMainDashboard()` draws the current dashboard and returns.

The older `switch (view_base)` renderer remains in the source below the return
for compatibility and reference, but it is not reached by this 128x64 path.
Consequently, `g_eeGeneral.view` still changes in response to view-navigation
events, while the custom dashboard remains the visible main display.

The legacy trim setting (`ModelData::displayTrims`) belongs to that older
renderer and does not currently change the custom dashboard described here.

## Maintaining the logic

When changing a condition, update both the state calculation and the visual
rule that consumes it. In particular, check these interactions:

- `SE` changes must still require the `SC` away-and-back acknowledgment.
- `SD` must remain part of every movement-enable condition.
- The remembered last-moved switch affects both direction highlighting and DF.
- Direction priority is forward, back, clockwise, counter-clockwise, right,
  then left.
- Changes to the custom dashboard should be checked on hardware or a matching
  128x64 simulator; there are currently no automated tests for this renderer.