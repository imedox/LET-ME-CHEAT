# LETMECHEAT CD 1.13.00 v1.1

An ASI / DLL mod for `CrimsonDesert.exe` that ports seven Cheat Engine scripts into a
single, self-contained injection DLL with per-feature on/off switches via an INI file.

## Runtime Files

| File | Purpose |
|---|---|
| `LETMECHEAT.dll` / `LETMECHEAT.asi` | The mod itself (rename `.dll` → `.asi` for ASI loaders) |
| `LETMECHEAT.ini` | Per-feature configuration (auto-created with defaults on first run) |
| `LETMECHEAT.log` | Debug log (only written when `LogEnabled=1`) |

## Features

All five cheats are implemented as raw code-cave trampolines or inline byte patches —
no MinHook, no third-party libraries, Windows SDK only.

| INI Key | Default | Description |
|---|---|---|
| `AbyssGearNoDurLoss` | `1` | Abyss Gear durability never decreases |
| `InfPolishDur` | `1` | Weapon polishing durability is infinite (value never written lower) |
| `EzParry` | `1` | Both parry gate checks always pass |
| `FastDragonCD` | `1` | Dragon cooldown counter always reads 0 |
| `RepNoDecrease` | `1` | Reputation never goes down |
| `FastFriendship` | `1` | NPC friendship is set to maximum (100) |
| `FastPetFriendship` | `1` | Pet friendship is set to maximum (95) |
| `LogEnabled` | `0` | Write startup and hook diagnostics to `LETMECHEAT.log` |

## Config

`LETMECHEAT.ini` is created automatically next to the DLL on first load:

```ini
[General]
AbyssGearNoDurLoss=1
InfPolishDur=1
EzParry=1
FastDragonCD=1
RepNoDecrease=1
FastFriendship=1
FastPetFriendship=1
LogEnabled=0
```

Set any value to `0` to disable that feature. The game must be restarted for changes
to take effect.

## How It Works

On `DLL_PROCESS_ATTACH` the mod:
1. Verifies it is running inside `CrimsonDesert.exe`.
2. Reads `LETMECHEAT.ini` (writing defaults if absent).
3. For each enabled feature, scans the main module's executable sections for a
   unique AOB pattern, then either:
   - **Trampoline** — allocates a writeable cave (`PAGE_READWRITE`), encodes the
     pre/post logic + original bytes, then updates memory to executable (`PAGE_EXECUTE_READ`)
     and redirects the injection site with a 14/16-byte absolute JMP.
   - **Inline patch** — overwrites 1–6 bytes directly (e.g. `je` → `jmp`,
     `mov ecx,[...]` → `xor ecx,ecx`).

On `DLL_PROCESS_DETACH` all original bytes are restored and code caves are freed.

## Build

- **Solution:** `LETMECHEAT.slnx`
- **Project:** `LETMECHEAT/LETMECHEAT.vcxproj`
- **Recommended config:** `Release | x64`

```
MSBuild LETMECHEAT\LETMECHEAT.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output: `LETMECHEAT\x64\Release\LETMECHEAT.dll`

No third-party dependencies — Windows SDK only.
