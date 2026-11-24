# Changelog

## 0.5 — pre-release

- Renamed the project to **CRTower Defence** and moved the game folder to
  `ms0:/PSP/GAME/CRTowerDefence`.
- New hand-drawn CRT-monitor XMB icon (`ICON0.PNG`).
- Added `V0.5` version marker to the pause menu.
- Frost tower sound rebalanced: lower volume, softer tone, longer retrigger gap.
- Gold in the HUD now uses compact `K` / `M` / `B` notation so it cannot overlap
  the castle HP readout.
- Added cursor-speed setting in the pause menu (100% down to 0.1%), saved with
  the run.
- Added a `PERKS & MODS` page listing active perks with stack counts and active
  enemy modifiers. Cursed perks are marked with `!`.
- Added an `ENEMY MODIFIER` pop-up every 10 waves; it waits for the perk choice
  and pause menu so it cannot be missed.
- Save format v3 (cursor speed), with automatic migration of v2 saves.
- Fixed heavy-wave rendering by splitting GE draw commands at 30,000 vertices
  instead of overflowing the 65,535-vertex command limit.
- Added repository documentation, license, ignore rules and icon build script.

## Earlier pre-release work

- Initial native C port of the browser prototype.
- Five tower types, drawable path, waves, perks and modifiers.
- English UI and proportional hand-drawn font.
- CRT themes, sound synthesis, save/load, pause/help pages.
