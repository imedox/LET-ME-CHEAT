# LETMECHEAT Mod

## Description
**LETMECHEAT** is a self-contained, lightweight injection mod (DLL/ASI) for *Crimson Desert* (`CrimsonDesert.exe`). The mod translates multiple popular memory cheats (originally Cheat Engine scripts) into native assembly hacks, eliminating the need to run Cheat Engine in the background. It infuses code-cave trampolines and inline byte patches at runtime, with a simple local configuration file (`LETMECHEAT.ini`) allowing players to easily toggle individual features on or off.

---

## Installation Instructions
Follow these steps to install and set up the mod:

1. **Get the Mod Binary:**
   - Download the compiled `LETMECHEAT.asi`. 
   - *(Optional)* If compiling from the source, open the solution `LETMECHEAT.slnx` in Visual Studio and build it using the **Release | x64** configuration. The compiled binary will be placed at `LETMECHEAT/x64/Release/LETMECHEAT.dll`.
2. **Prepare for ASI Loading:**
   - If using a standard ASI loader (such as Ultimate ASI Loader), rename `LETMECHEAT.dll` to `LETMECHEAT.asi`.
3. **Deploy Files:**
   - Copy the library file (`LETMECHEAT.dll` or `LETMECHEAT.asi`) to your Crimson Desert game installation folder (where `CrimsonDesert.exe` is located).
4. **Configure Settings:**
   - Start the game once. The mod will automatically generate a default configuration file named `LETMECHEAT.ini` in the same directory.
   - Open `LETMECHEAT.ini` in any text editor to customize your settings:
     ```ini
     [General]
     AbyssGearNoDurLoss=1  ; 1 = Enabled, 0 = Disabled
     InfPolishDur=1
     EzParry=1
     FastDragonCD=1
     RepNoDecrease=1
     FastFriendship=1
     FastPetFriendship=1
     LogEnabled=0          ; Write startup and hook diagnostics to LETMECHEAT.log
     ```
   - Set any option to `0` to disable it, and restart the game for changes to take effect.

---

## Main Features
* **Abyss Gear Durability Guard (`AbyssGearNoDurLoss`)**: Abyss Gear durability never decreases.
* **Infinite Weapon Polishing (`InfPolishDur`)**: Weapon polishing durability is infinite (the value is never written lower).
* **Easy Parry (`EzParry`)**: Triggers automatic parry gate bypasses, making both parry checks always succeed.
* **No Dragon Cooldown (`FastDragonCD`)**: The dragon ability cooldown counter always reads 0, enabling instant reuse.
* **Reputation Guard (`RepNoDecrease`)**: Reputation points never drop.
* **Fast Friendship (`FastFriendship`)**: Instantly sets NPC friendship structure to maximum (100).
* **Fast Pet Friendship (`FastPetFriendship`)**: Instantly sets Pet friendship level to maximum (95).
* **Startup Logging (`LogEnabled`)**: Generates runtime diagnostics and log information to `LETMECHEAT.log` if toggled on.

---

## Requirements
* **Game Executable**: Companion to *Crimson Desert* PC version (`CrimsonDesert.exe`).
* **Hook Loader**: A standard ASI Loader (e.g., Ultimate ASI Loader's `dinput8.dll`) or a general DLL Injector to hook and load the plugin at startup.
* **Permissions**: Ensure the game folder has write permissions if you wish to let the mod auto-generate the configuration and log files.

---

## Shout Outs
* **bbfox** (at [opencheattables.com](https://opencheattables.com)) — For creating the original Cheat Engine scripts, finding the base offsets, and mapping out the AOB patterns.
* **imedox** — For translating the original Cheat Engine script injections into a clean C++ wrapper DLL using robust trampoline/cave allocations.
* **The Open Cheat Tables Community** — For providing invaluable research, advice, and testing feedback.
