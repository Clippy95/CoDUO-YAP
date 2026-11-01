# CoDUO-YAP ­– Yet another plugin for Call of Duty: United Offensive
Aiming to provide enhancements, quality-of-life improvements and advanced features for CoD:UO SP & MP, with the main focus on SP

<p align="center">
  <img src="https://github.com/user-attachments/assets/e4531a86-b2bc-44fa-95b9-9c40e3d48245"
       alt="Highlights"/>
</p>


<details open><summary><h2>Features/cvars:</h2></summary>

| Console variable or feature | Description |
|------|-------------|
| `cg_fixedAspectFOV` | Implements HOR+ field of view (FOV) scaling when set to `1`<br><img src="https://github.com/user-attachments/assets/6c71a533-7836-42e6-b948-2b27dad6d32e" alt="cg_fixedAspectFOV"/> |
| `cg_fixedAspect` | Fixes HUD stretching, not recommended to be changed live, requires `vid_restart` for some elements |
| `r_fixedaspect_clear` | Clears color and depth buffers before drawing, `1` always clears it, while `2` only clears it while in UI, menus & cinematics |
| [`.roq`, `.bik`] Cinematics | Properly displayed in aspect ratios wider than 4:3 and no longer appear as a black screen |
| `safeArea_horizontal` | Horizontal safe area as a fraction of the screen width, might not affect all HUD elements, setting this to `0.0` will cause the HUD to be in 4:3 borders |
| `safeArea_vertical` | Not fully implemented |
| `r_arb_fragment_shader_wrap_ati` | Wraps the OpenGL extension `GL_ATI_FRAGMENT_SHADER` to `GL_ARB_FRAGMENT_SHADER` not 100% accurate to the original extension, implementation is hardware agnostic and works on AMD,NVIDIA, Intel <br><sup>(for Nvidia it requires seta r_nv_texture_shader "0" and seta r_nv_register_combiners "0" otherwise those extensions will take priority over ATI fragment shader.)</sup> <br><sup> Below is footage taken on an AMD GPU, Drivers: Adrenalin 25.12.1, the drivers have a missing `GL_ATI_FRAGMENT_SHADER` implementation </sup> <br><img width="540" src="https://github.com/user-attachments/assets/971831bc-c8e9-4642-8ebe-cb5e8ca5e158" alt="r_arb_fragment_shader_wrap_ati showcase"/> |
| `r_fog_drawsun_workaround` | Fixes sun/moon going invisible when `r_fog` is enabled on modern GPU drivers |
| `cg_fovscale` | Applies a multiplier to overall FOV |
| `cg_fovscale_ads` | Applies a multiplier to Aim down sights (ADS) FOV |
| `m_rawinput` | Toggles raw mouse input |
| `cg_weaponFov` | Weapon FOV, independent of `cg_fov`: default `80`, `0` follows `cg_fov`. Blends back to stock while Aiming down sights |
| `cg_fovMin` | Sets the minimum FOV that the game will allow during ADS |
| `r_qol_texture_filter_anisotropic` | Determines anisotropic filtering, applies only if GPU vendor supports `GL_EXT_texture_filter_anisotropic` |
| `r_noborder` | Borderless windowed when `r_fullscreen` is set to `0`,<br>`r_fullscreen 0;r_noborder 1;vid_ypos 0;vid_xpos 0;vid_restart` is recommended |
| `r_mode_auto` | When set to `1`, it will automatically determine your screen resolution when `r_mode` is set to `-1` |
| `player_sprintSpeedScale` | Scales sprint speed **(SP only)** |
| `player_sprintMult` | Controls how fast the sprint meter will run out **(SP only)** |
| `player_sprintIdleRegen` | `1` keeps the sprint meter regenerating while sprint key is held but you are standing still or blocked **(SP only)** |
| `g_enemyFireDist` | Distance before tagging client's crosshair red,<br>`0` disables this functionality **(SP only)** |
| `cg_drawCrosshair_friendly_green` | `1` will change the crosshair color to green when aiming at friendly NPCs **(SP only)** |
| `cg_weaponSprint_mod` <h5 id="cg_weaponsprint_mod"></h5> | Restores <sup><sup>yes this was planned for UO</sup></sup>  and implements new **weapon bobbing** while sprinting and **rotation attributes** inspired by T4, requires new eWeapon files for each weapon<img src="https://github.com/user-attachments/assets/dc0ccdae-9d7a-4c06-8941-c7535c0b1771" alt="cg_weaponsprint_mod"/> |
| `reload_eweapons` | Console command to reload eWeapon definitions |
| `bg_allowJumpShot` | allows player to keep ADS while jumping **(SP only)** |
| `yap_xanim_iw3_transitionTime` | implements xanim transitionTime for weapon animations, akin to IW3+ this achieves lerp'd cancel weapon switches animations |
| `cg_subtitle_centered_enable` | Aligns subtitles to the center rather than the left |
| `cg_ammo_overwrite_size_enabled` & `cg_ammo_overwrite_size` | Allows you to customize the font size of the ammo counter |
| `branding`| Draws current version on top left |
| and possible some more… | Use the `qol_showallcvars` command to show all cvars registered by the plugin! |
</details>

## Installation:
1. Download the [latest release](https://github.com/Clippy95/CoDUO-YAP/releases/latest) [![Release](https://img.shields.io/github/v/release/Clippy95/CoDUO-YAP?color=brightgreen)](https://github.com/Clippy95/CoDUO-YAP/releases/latest) or use [CI Actions](https://github.com/Clippy95/CoDUO-YAP/actions?query=event%3Apush+is%3Asuccess+branch%3Amaster) for semi-experimental builds [![Last Build](https://github.com/Clippy95/CoDUO-YAP/actions/workflows/main.yml/badge.svg?branch=master)](https://github.com/Clippy95/CoDUO-YAP/actions?query=event%3Apush+is%3Asuccess+branch%3Amaster)
2. Extract the contents to your game folder
3. Launch the game

### Installation notes
CoDUO-YAP can also be loaded using [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader):
- Rename `ddraw.dll` → `ddraw.asi` (the file name can also be changed)
- _For the Steam version:_ set `DontLoadFromDllMain=0` in UAL's `global.ini`

## FAQ:
**Q: Does the plugin work with the original Call of Duty (2003) release?**<br>A: No. If you want to play through the original CoD1 campaign use with [Singleplayer Improved](https://www.moddb.com/mods/singleplayer-improved) or [United Fronts](https://www.moddb.com/mods/call-of-duty-united-fronts) mod.

**Q: I'm running into an issue XYZ…**<br>A: Please report it on the [Issues](https://github.com/Clippy95/CoDUO-YAP/issues?q=is%3Aissue), but first try the [latest CI Action build](https://github.com/Clippy95/CoDUO-YAP/actions?query=event%3Apush+is%3Asuccess+branch%3Amaster) and see if the problem has already been solved.

## Credits:
- [RTCW-SP/MP](https://github.com/id-Software/RTCW-SP)
- [ioquake3](https://github.com/ioquake/ioq3)
- [AlphaYellow's Widescreen Fixes](https://github.com/alphayellow1/AlphaYellowWidescreenFixes) - for the x86 FPU operations wrappers
- [IW1x Client](https://github.com/hBrutal/iw1x-client)
- [ModUtils](https://github.com/CookiePLMonster/ModUtils/tree/master)
- [SafetyHook](https://github.com/cursey/safetyhook)
- [Vlad Loktionov](https://github.com/VladWinner) - for testing, suggestions and other several contributions
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
- [momo5502](https://github.com/momo5502)
- [JBShady](https://github.com/JBShady) - cleaned up CoD2+ crosshair texture
