// dllmain.cpp – LETMECHEAT cheat plugin for CrimsonDesert.exe
// Ported by imedox from (CE scripts) created by bbfox (https://opencheattables.com.) to a standalone DLL.
//
// Each feature can be toggled in LETMESLEEP.ini:
//   [General]
//   AbyssGearNoDurLoss   = 1   ; Abyss Gear durability no decrease
//   InfPolishDur         = 1   ; Infinite weapon polishing durability
//   EzParry              = 1   ; Always pass parry gate checks
//   FastDragonCD         = 1   ; Dragon cooldown always 0
//   RepNoDecrease        = 1   ; Reputation never goes down
//   LogEnabled           = 0   ; Write LETMESLEEP.log

#include "pch.h"

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Internal namespace
// ---------------------------------------------------------------------------
namespace {

constexpr const char* kModName = "LETMECHEAT";

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
HMODULE g_selfModule = nullptr;

FILE*   g_logFile    = nullptr;
bool    g_logEnabled = false;

wchar_t g_pluginDir[MAX_PATH] = {};

// Feature flags (loaded from INI)
bool g_featAbyssGearNoDur   = true;
bool g_featInfPolishDur     = true;
bool g_featEzParry          = true;
bool g_featFastDragonCD     = true;
bool g_featRepNoDecrease    = true;
bool g_featFastFriendship   = true;
bool g_featFastPetFriendship= true;

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
void Log(const char* fmt, ...) {
    if (!g_logEnabled || !g_logFile) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(g_logFile, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(g_logFile, fmt, args);
    va_end(args);
    std::fflush(g_logFile);
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------
std::wstring BuildPath(const wchar_t* fileName) {
    std::wstring p = g_pluginDir;
    if (!p.empty() && p.back() != L'\\') p += L'\\';
    p += fileName;
    return p;
}

void ResolvePluginDir() {
    wchar_t dllPath[MAX_PATH] = {};
    if (!GetModuleFileNameW(g_selfModule, dllPath, static_cast<DWORD>(std::size(dllPath)))) return;
    wcsncpy_s(g_pluginDir, std::size(g_pluginDir), dllPath, _TRUNCATE);
    wchar_t* slash = std::wcsrchr(g_pluginDir, L'\\');
    if (slash) *slash = L'\0';
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
void WriteDefaultConfig(const std::wstring& ini) {
    WritePrivateProfileStringW(L"General", L"AbyssGearNoDurLoss", L"1", ini.c_str());
    WritePrivateProfileStringW(L"General", L"InfPolishDur",       L"1", ini.c_str());
    WritePrivateProfileStringW(L"General", L"EzParry",            L"1", ini.c_str());
    WritePrivateProfileStringW(L"General", L"FastDragonCD",       L"1", ini.c_str());
    WritePrivateProfileStringW(L"General", L"RepNoDecrease",      L"1", ini.c_str());
    WritePrivateProfileStringW(L"General", L"FastFriendship",     L"1", ini.c_str());
    WritePrivateProfileStringW(L"General", L"FastPetFriendship",  L"1", ini.c_str());
    WritePrivateProfileStringW(L"General", L"LogEnabled",         L"0", ini.c_str());
}

void LoadConfig() {
    const std::wstring ini = BuildPath(L"LETMECHEAT.ini");
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
        WriteDefaultConfig(ini);

    auto readBool = [&](const wchar_t* key, bool def) {
        return GetPrivateProfileIntW(L"General", key, def ? 1 : 0, ini.c_str()) != 0;
    };
    g_featAbyssGearNoDur = readBool(L"AbyssGearNoDurLoss", true);
    g_featInfPolishDur   = readBool(L"InfPolishDur",       true);
    g_featEzParry        = readBool(L"EzParry",            true);
    g_featFastDragonCD   = readBool(L"FastDragonCD",       true);
    g_featRepNoDecrease  = readBool(L"RepNoDecrease",      true);
    g_featFastFriendship   = readBool(L"FastFriendship",   true);
    g_featFastPetFriendship = readBool(L"FastPetFriendship", true);
    g_logEnabled         = readBool(L"LogEnabled",         false);
}

void OpenLog() {
    if (!g_logEnabled) return;
    const std::wstring logPath = BuildPath(L"LETMECHEAT.log");
    _wfopen_s(&g_logFile, logPath.c_str(), L"w");
    if (!g_logFile) g_logEnabled = false;
}

// ---------------------------------------------------------------------------
// Target process guard
// ---------------------------------------------------------------------------
bool IsTargetProcess() {
    wchar_t exePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, exePath, static_cast<DWORD>(std::size(exePath)))) return false;
    const wchar_t* name = std::wcsrchr(exePath, L'\\');
    name = name ? name + 1 : exePath;
    return _wcsicmp(name, L"CrimsonDesert.exe") == 0;
}

// ---------------------------------------------------------------------------
// AOB scanner
// ---------------------------------------------------------------------------
bool ParseAob(const char* pattern, std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& mask) {
    bytes.clear(); mask.clear();
    const char* p = pattern;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (p[0] == '?' && p[1] == '?') { bytes.push_back(0); mask.push_back(0); p += 2; continue; }
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = hex(p[0]), lo = hex(p[1]);
        if (hi < 0 || lo < 0) return false;
        bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
        mask.push_back(0xFF);
        p += 2;
    }
    return !bytes.empty() && bytes.size() == mask.size();
}

std::uintptr_t ScanModule(const char* pattern) {
    std::vector<std::uint8_t> bytes, mask;
    if (!ParseAob(pattern, bytes, mask)) return 0;
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (!base) return 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    const std::size_t need = bytes.size();
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        std::size_t sz = std::max<std::size_t>(sec->Misc.VirtualSize, sec->SizeOfRawData);
        if (sz < need) continue;
        const auto* beg = base + sec->VirtualAddress;
        const auto* end = beg + (sz - need + 1);
        for (const auto* q = beg; q < end; ++q) {
            bool ok = true;
            for (std::size_t j = 0; j < need; ++j) {
                if (mask[j] && q[j] != bytes[j]) { ok = false; break; }
            }
            if (ok) return reinterpret_cast<std::uintptr_t>(q);
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Patch helpers
// ---------------------------------------------------------------------------

// Overwrite bytes at addr with src, making page writable temporarily.
bool PatchBytes(std::uintptr_t addr, const void* src, std::size_t len) {
    DWORD old{};
    if (!VirtualProtect(reinterpret_cast<void*>(addr), len, PAGE_EXECUTE_READWRITE, &old))
        return false;
    std::memcpy(reinterpret_cast<void*>(addr), src, len);
    VirtualProtect(reinterpret_cast<void*>(addr), len, old, &old);
    return true;
}

// Write a 64-bit absolute JMP: FF 25 00 00 00 00 <addr:8>  (14 bytes)
bool WriteAbsJmp(std::uintptr_t from, std::uintptr_t to) {
    std::uint8_t buf[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0, 0, 0, 0, 0, 0, 0, 0
    };
    std::memcpy(buf + 6, &to, 8);
    return PatchBytes(from, buf, sizeof(buf));
}

// Allocate a code cave near the target (within ±2GB if possible, else anywhere)
std::uint8_t* AllocCave(std::size_t size) {
    auto* p = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    return p;
}

bool ProtectCave(void* cave, std::size_t size) {
    DWORD old{};
    return VirtualProtect(cave, size, PAGE_EXECUTE_READ, &old) != 0;
}

// ---------------------------------------------------------------------------
// Saved original bytes for restoration on shutdown
// ---------------------------------------------------------------------------
struct PatchRecord {
    std::uintptr_t          addr;
    std::vector<std::uint8_t> original;
    std::uint8_t*           cave;   // may be null
};

std::vector<PatchRecord> g_patches;

void RecordAndPatch(std::uintptr_t addr, const void* newBytes, std::size_t len,
                    std::uint8_t* cave = nullptr) {
    PatchRecord rec;
    rec.addr = addr;
    rec.original.resize(len);
    std::memcpy(rec.original.data(), reinterpret_cast<const void*>(addr), len);
    rec.cave = cave;
    PatchBytes(addr, newBytes, len);
    g_patches.push_back(std::move(rec));
}

void UninstallAll() {
    // Restore in reverse order
    for (auto it = g_patches.rbegin(); it != g_patches.rend(); ++it) {
        PatchBytes(it->addr, it->original.data(), it->original.size());
        if (it->cave) VirtualFree(it->cave, 0, MEM_RELEASE);
    }
    g_patches.clear();
}

// ---------------------------------------------------------------------------
// NOPs helper
// ---------------------------------------------------------------------------
void BuildNops(std::uint8_t* dst, std::size_t n) {
    std::memset(dst, 0x90, n);
}

// ---------------------------------------------------------------------------
// CHEAT 1 – Abyss Gear Durability No Decrease
// ---------------------------------------------------------------------------
// CE script:
//   Injection point: CrimsonDesert.exe+1EBC438  (cmp cx,di)
//   The trampoline prepends:  mov cx,64 / mov di,cx
//   Then reassembles the 4 original instructions (0xE bytes total) and jumps back.
//
// Injection point AOB (first-match):
//   66 3B ?? 66 0F 4C ?? 66 3B ?? 66 89 ?? 02
// We patch 14 bytes (to fit the far JMP) at the injection point.
// The cave runs:
//   mov  cx, 0x64         ; B9 64 00 66 → actually: 66 B9 64 00
//   mov  di, cx           ; 66 89 CF
//   [original 14 bytes]
//   jmp  back (abs)
// ---------------------------------------------------------------------------
constexpr const char* kAob_AbyssGear =
    "66 3B ?? 66 0F 4C ?? 66 3B ?? 66 89 ?? 02";

bool InstallAbyssGearNoDur() {
    std::uintptr_t addr = ScanModule(kAob_AbyssGear);
    if (!addr) { Log("[E] AbyssGear AOB not found\n"); return false; }
    Log("[i] AbyssGear AOB @ %p\n", reinterpret_cast<void*>(addr));

    // We need 14 bytes at injection site for the jumper.
    // Original 14 bytes:
    //   +0  66 3B CF           cmp cx,di          (3)
    //   +3  66 0F 4C F9        cmovl di,cx        (4)
    //   +7  66 3B F7           cmp si,di          (3)
    //   +A  66 89 7B 02        mov [rbx+02],di    (4)
    //                                        total = 14 bytes
    // We'll patch exactly 14 bytes → fit a 14-byte absolute JMP.

    const std::size_t patchLen = 14;
    std::uint8_t* cave = AllocCave(64);
    if (!cave) { Log("[E] AbyssGear: cave alloc failed\n"); return false; }

    std::uint8_t* c = cave;

    // mov cx, 0x64  →  66 B9 64 00
    *c++ = 0x66; *c++ = 0xB9; *c++ = 0x64; *c++ = 0x00;
    // mov di, cx   →  66 89 CF
    *c++ = 0x66; *c++ = 0x89; *c++ = 0xCF;

    // Copy original 14 bytes
    std::memcpy(c, reinterpret_cast<const void*>(addr), patchLen);
    c += patchLen;

    // Absolute JMP back (addr + patchLen)
    std::uintptr_t retAddr = addr + patchLen;
    *c++ = 0xFF; *c++ = 0x25; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00;
    std::memcpy(c, &retAddr, 8); c += 8;

    ProtectCave(cave, 64);

    // Install: absolute JMP to cave at addr, pad with NOPs
    std::uint8_t jmp[14];
    jmp[0] = 0xFF; jmp[1] = 0x25;
    std::memset(jmp + 2, 0x00, 4);
    std::uintptr_t caveAddr = reinterpret_cast<std::uintptr_t>(cave);
    std::memcpy(jmp + 6, &caveAddr, 8);

    RecordAndPatch(addr, jmp, patchLen, cave);
    Log("[+] AbyssGear installed (cave=%p)\n", cave);
    return true;
}

// ---------------------------------------------------------------------------
// CHEAT 2 – Infinite Weapon Polishing Durability
// ---------------------------------------------------------------------------
// CE script:
//   Injection point: CrimsonDesert.exe+1C8899F  (mov [rbx+50],di)
//   The trampoline adds a guard:
//     cmp  [rbx+50], di
//     jge  skip_dec
//     mov  [rbx+50], di      ; original write
//  skip_dec:
//     mov  rbx,[rsp+20]      ; original +4
//     mov  rax,[rbx]         ; original +9
//     xor  edx,edx           ; original +C
//     jmp  back
//
// Injection point AOB:
//   66 89 ?? 50 ?? 8B ?? 24 ?? ?? 8B ?? 33 ??
//
// Patch size needed: 13 bytes (+0..+C covers 4+5+3+2 = 14 bytes actually)
//
// Let's check:
//   +0  66 89 7B 50   mov [rbx+50],di      (4)
//   +4  48 8B 5C 24 20 mov rbx,[rsp+20]    (5)
//   +9  48 8B 03      mov rax,[rbx]        (3)
//   +C  33 D2         xor edx,edx          (2)
//                                    total = 14
// ---------------------------------------------------------------------------
constexpr const char* kAob_InfPolish =
    "66 89 ?? 50 ?? 8B ?? 24 ?? ?? 8B ?? 33 ??";

bool InstallInfPolishDur() {
    std::uintptr_t addr = ScanModule(kAob_InfPolish);
    if (!addr) { Log("[E] InfPolish AOB not found\n"); return false; }
    Log("[i] InfPolish AOB @ %p\n", reinterpret_cast<void*>(addr));

    const std::size_t patchLen = 14;
    std::uint8_t* cave = AllocCave(128);
    if (!cave) { Log("[E] InfPolish: cave alloc failed\n"); return false; }

    std::uint8_t* c = cave;

    // cmp  word ptr [rbx+50], di  →  66 3B 7B 50
    *c++ = 0x66; *c++ = 0x3B; *c++ = 0x7B; *c++ = 0x50;
    // jge  short +7 (skip the write, land after these 7 bytes)
    // distance: 7 bytes (66 89 7B 50 = 4 bytes skipped, then next section)
    // We'll compute: after jge, if taken we skip the next 4 bytes (mov [rbx+50],di)
    // jge short:  7D <rel8>
    *c++ = 0x7D; *c++ = 0x04;  // jge +4 (skip next 4 bytes)
    // mov [rbx+50],di  →  66 89 7B 50
    *c++ = 0x66; *c++ = 0x89; *c++ = 0x7B; *c++ = 0x50;
    // skip_dec:
    // mov rbx,[rsp+20]  →  48 8B 5C 24 20
    *c++ = 0x48; *c++ = 0x8B; *c++ = 0x5C; *c++ = 0x24; *c++ = 0x20;
    // mov rax,[rbx]  →  48 8B 03
    *c++ = 0x48; *c++ = 0x8B; *c++ = 0x03;
    // xor edx,edx  →  33 D2
    *c++ = 0x33; *c++ = 0xD2;

    // Back JMP
    std::uintptr_t retAddr = addr + patchLen;
    *c++ = 0xFF; *c++ = 0x25; *c++ = 0; *c++ = 0; *c++ = 0; *c++ = 0;
    std::memcpy(c, &retAddr, 8); c += 8;

    ProtectCave(cave, 128);

    // Install patch
    std::uint8_t jmp[14];
    jmp[0] = 0xFF; jmp[1] = 0x25;
    std::memset(jmp + 2, 0, 4);
    std::uintptr_t caveAddr = reinterpret_cast<std::uintptr_t>(cave);
    std::memcpy(jmp + 6, &caveAddr, 8);

    RecordAndPatch(addr, jmp, patchLen, cave);
    Log("[+] InfPolish installed (cave=%p)\n", cave);
    return true;
}

// ---------------------------------------------------------------------------
// CHEAT 3 – Ez Parry
// ---------------------------------------------------------------------------
// CE script analysis (from Cheat.md):
//   The injection point is CrimsonDesert.exe+841FE9.
//   The script simply patches conditional jumps to make parry checks always pass.
//
// Looking at the disassembly at the injection point:
//   +841FE9: 80 BD 60 04 00 00 00  cmp byte ptr [rbp+460],00
//   +841FF0: 74 1C                 je CrimsonDesert.exe+84200E  ← gate 2 check
//
// From the OLD data notes in Cheat.md, the 4 je patches are:
//   je @ +841F71: 74 1C  → patch to EB 1C (jmp) – skip: skip the gate check, always treat as "parry ok"
//   je @ +841F7A: 74 13  → patch to EB 13
//   je @ +841FE7: 74 25  → patch to EB 25  (this is what the "INJECTING HERE" is about)
//   je @ +841FF0: 74 1C  → patch to EB 1C
//
// However the actual approach from the CE script for "ez parry" is different —
// it patches the `je` (74) after the final `test bl,bl` check to `jmp` (EB)
// so that the "skip to next enemy" branch is never taken and parry is always triggered.
//
// From the injection point +841FE9 context:
//   +841FE7: 74 25   je CrimsonDesert.exe+84200E   ← "bl=0, skip" gate
//   +841FF0: 74 1C   je CrimsonDesert.exe+84200E   ← "[rbp+460]=0, skip" gate
//
// The CE script does NOT have explicit ENABLE code (just the ORIGINAL CODE block).
// The typical "ez parry" approach: replace the two `je` instructions with `jmp` (EB).
// This makes both gate checks unconditionally pass.
//
// AOB for the block around +841FE7:
//   80 BD 60 04 00 00 00        cmp byte ptr [rbp+00000460],00
// That's the injection point. We search for this 7-byte sequence.
// Immediately before it at -2 bytes:   84 DB 74 25
// We'll search for:  84 DB 74 25 80 BD 60 04 00 00 00 74 1C
// ---------------------------------------------------------------------------
//
// Simpler: search for the 7-byte CMP at the injection point, then:
//   patch the byte at -2 (relative) which is 0x74 → 0xEB  (je → jmp)
//   and patch the byte at +7 which is also 0x74 → 0xEB
//
// CE injection-point AOB: 80 BD 60 04 00 00 00
//
// Full context AOB used for scanning (unique):
//   84 DB 74 25 80 BD 60 04 00 00 00 74 1C
// ---------------------------------------------------------------------------
constexpr const char* kAob_EzParry =
    "84 DB 74 25 80 BD 60 04 00 00 00 74 1C";

bool InstallEzParry() {
    std::uintptr_t addr = ScanModule(kAob_EzParry);
    if (!addr) { Log("[E] EzParry AOB not found\n"); return false; }
    Log("[i] EzParry AOB @ %p\n", reinterpret_cast<void*>(addr));

    // addr+2: byte 0x74 (je +25)   → patch to 0xEB (jmp +25)
    // addr+11: byte 0x74 (je +1C)  → patch to 0xEB (jmp +1C)
    std::uint8_t jmpOp = 0xEB;
    RecordAndPatch(addr + 2,  &jmpOp, 1);
    RecordAndPatch(addr + 11, &jmpOp, 1);

    Log("[+] EzParry installed\n");
    return true;
}

// ---------------------------------------------------------------------------
// CHEAT 4 – Fast Dragon Cooldown
// ---------------------------------------------------------------------------
// CE script:
//   Injection point: CrimsonDesert.exe+7E5F2F
//   AOB: 8B 8E 90 00 00 00 85 ?? 75 ?? ?? 8B
//   Patch: replace  mov ecx,[rsi+90]  (6 bytes) with:
//             xor ecx,ecx            (2 bytes: 33 C9)
//             nop × 4                (4 bytes: 90 90 90 90)
// ---------------------------------------------------------------------------
constexpr const char* kAob_DragonCD =
    "8B 8E 90 00 00 00 85 ?? 75 ?? ?? 8B";

bool InstallFastDragonCD() {
    std::uintptr_t addr = ScanModule(kAob_DragonCD);
    if (!addr) { Log("[E] FastDragonCD AOB not found\n"); return false; }
    Log("[i] FastDragonCD AOB @ %p\n", reinterpret_cast<void*>(addr));

    // Patch 6 bytes: 33 C9 90 90 90 90
    const std::uint8_t patch[6] = { 0x33, 0xC9, 0x90, 0x90, 0x90, 0x90 };
    RecordAndPatch(addr, patch, sizeof(patch));

    Log("[+] FastDragonCD installed\n");
    return true;
}

// ---------------------------------------------------------------------------
// CHEAT 5 – Reputation No Decrease
// ---------------------------------------------------------------------------
// CE script:
//   Injection point: CrimsonDesert.exe+1CCA5C1 (call ...)
//   AOB: E8 ?? ?? ?? ?? ?? 8B ?? ?? 85 ?? 74 ?? 8B ?? 04 89 ?? 08
//
// The script injects a trampoline that — after the original call that returns
// a pointer to the rep record in RAX — prevents writing a smaller value:
//
//   Original at injection point:
//   +0  E8 xx xx xx xx   call ...      (5)
//   +5  48 8B C8         mov rcx,rax   (3)
//   +8  48 85 C0         test rax,rax  (3)
//   +B  74 XX            je ...        (2)
//   +D  8B 50 04         mov edx,[rax+04]  (3) ← first byte after injection
//                                      total injected = 13 bytes
//
// Wait – the CE script says: inject at the CALL, patch $10 bytes.
// Let's count exactly: the injection point AOB has 'E8' at start = CALL opcode.
// Bytes injected (E8 ?? ?? ?? ??  = 5) + (48 8B C8 = 3) + (48 85 C0 = 3) + (74 ?? = 2) = 13
// But AOB says $10 = 16 bytes? Let me re-read: alloc(INJECT_NO_REP_DECo, $10) = 16 bytes saved.
//
// From injection point AOB:
//   E8 ?? ?? ?? ??   (5)
//   ?? 8B ?? ??      hmm...
//
// Looking more carefully at the actual disasm:
//   +1CCA5C1: E8 4A BB 63 FE          call CrimsonDesert.exe+306110      (5)
//   +1CCA5C6: 48 8B C8                mov rcx,rax                        (3)
//   +1CCA5C9: 48 85 C0                test rax,rax                       (3)
//   +1CCA5CC: 74 40                   je CrimsonDesert.exe+1CCA60E       (2)
//   +1CCA5CE: 8B 50 04                mov edx,[rax+04]                   (3)
//                                                             total = 16 = $10 ✓
//
// CE cave logic: execute original 5+3+3+2 = 13 bytes, then:
//   check if [rax+08] > edi (current rep > new rep?)
//   if so: use old value, and clamp to 100
//   The key insight: the script PREVENTS [rax+08] being set to a LOWER value.
//   It checks: cmp [rax+08],edi / jle @@F / mov edi,[rax+08] / cmp edi,100 / jge @@F / add edi,1
//   Then falls through to: mov edx,[rax+04]  (which is the original +D byte)
//
// In practice the simplest correct port:
//   After the call+3 instructions (13 bytes), add:
//     cmp  [rax+08], edi    ; is current rep > new rep?
//     jle  short done       ; new >= current, allow decrease
//     mov  edi, [rax+08]    ; keep old (higher) rep
//   done:
//     mov edx,[rax+04]      ; original +D instruction
//     jmp  back
// ---------------------------------------------------------------------------
constexpr const char* kAob_RepNoDec =
    "E8 ?? ?? ?? ?? 48 8B C8 48 85 C0 74 ?? 8B 50 04";

bool InstallRepNoDecrease() {
    std::uintptr_t addr = ScanModule(kAob_RepNoDec);
    if (!addr) { Log("[E] RepNoDecrease AOB not found\n"); return false; }
    Log("[i] RepNoDecrease AOB @ %p\n", reinterpret_cast<void*>(addr));

    // Patch 16 bytes ( = $10 from CE script)
    const std::size_t patchLen = 16;
    std::uint8_t* cave = AllocCave(128);
    if (!cave) { Log("[E] RepNoDecrease: cave alloc failed\n"); return false; }

    std::uint8_t* c = cave;

    // Reassemble original 13 bytes: CALL(5) + MOV RCX,RAX(3) + TEST RAX,RAX(3) + JE(2)
    // We copy them verbatim; the CALL is relative so we must keep the original bytes
    // as-is (they'll work because we're in a cave—BUT the relative CALL target changes!).
    //
    // Solution: re-encode the CALL as absolute indirect:
    //   FF 15 00 00 00 00 <target_abs>   (14 bytes)
    // We know original call target from the E8 bytes:
    std::int32_t callRel;
    std::memcpy(&callRel, reinterpret_cast<const void*>(addr + 1), 4);
    std::uintptr_t callTarget = addr + 5 + static_cast<std::uintptr_t>(static_cast<std::int64_t>(callRel));

    // FF 15 00 00 00 00 <8-byte target>  — absolute indirect CALL
    *c++ = 0xFF; *c++ = 0x15; *c++ = 0x02; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00;
    // jump over the embedded address
    *c++ = 0xEB; *c++ = 0x08;
    std::memcpy(c, &callTarget, 8); c += 8;

    // mov rcx,rax: 48 8B C8
    *c++ = 0x48; *c++ = 0x8B; *c++ = 0xC8;
    // test rax,rax: 48 85 C0
    *c++ = 0x48; *c++ = 0x85; *c++ = 0xC0;

    // Instead of copying the 'je' (which has a relative target that's now wrong),
    // we rebuild it as:  je <abs via FF25>
    // Simpler: use the je-taken offset (+B from original) → original target
    std::int8_t jeRel;
    std::memcpy(&jeRel, reinterpret_cast<const void*>(addr + 11 + 1), 1);
    std::uintptr_t jeTarget = addr + 13 + static_cast<std::int64_t>(jeRel);
    // Encode as jne-over then abs-jmp  (or just: test rax,rax + jne short +14 + abs-jmp)
    // Safer: je → jne skip_abs_jmp; FF25-jmp-to-target; skip_abs_jmp:
    *c++ = 0x75; *c++ = 0x0E; // jne short (skip 14-byte abs jmp if rax!=0)
    // abs jmp to je-target (rax==0 path):
    *c++ = 0xFF; *c++ = 0x25; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00;
    std::memcpy(c, &jeTarget, 8); c += 8;

    // rax != 0 path: add the no-decrease guard:
    //   cmp  [rax+08], edi
    //   jle  short +5  (skip)
    //   mov  edi, [rax+08]
    // Bytes:
    //   3B 78 08          cmp edi,[rax+08]   ← actually cmp [rax+08],edi = 39 78 08
    //   7E 03             jle +3
    //   8B 78 08          mov edi,[rax+08]
    *c++ = 0x39; *c++ = 0x78; *c++ = 0x08;  // cmp [rax+08], edi
    *c++ = 0x7E; *c++ = 0x03;               // jle short +3 (skip mov)
    *c++ = 0x8B; *c++ = 0x78; *c++ = 0x08; // mov edi,[rax+08]

    // Original +D: mov edx,[rax+04]  → 8B 50 04
    *c++ = 0x8B; *c++ = 0x50; *c++ = 0x04;

    // Back JMP
    std::uintptr_t retAddr = addr + patchLen;
    *c++ = 0xFF; *c++ = 0x25; *c++ = 0; *c++ = 0; *c++ = 0; *c++ = 0;
    std::memcpy(c, &retAddr, 8); c += 8;

    ProtectCave(cave, 128);

    // Install patch with 16-byte absolute JMP (FF25 + 8-byte addr fits in 14, pad 2 NOPs)
    std::uint8_t jmp[16];
    jmp[0] = 0xFF; jmp[1] = 0x25;
    std::memset(jmp + 2, 0, 4);
    std::uintptr_t caveAddr = reinterpret_cast<std::uintptr_t>(cave);
    std::memcpy(jmp + 6, &caveAddr, 8);
    jmp[14] = 0x90; jmp[15] = 0x90; // 2 NOPs to fill 16 bytes

    RecordAndPatch(addr, jmp, patchLen, cave);
    Log("[+] RepNoDecrease installed (cave=%p)\n", cave);
    return true;
}

constexpr const char* kAob_FastFriendship =
    "74 ?? ?? 8B ?? 20 E8 ?? ?? ?? ?? 3C FE 7E ??";

bool InstallFastFriendship() {
    std::uintptr_t addr = ScanModule(kAob_FastFriendship);
    if (!addr) { Log("[E] FastFriendship AOB not found\n"); return false; }
    Log("[i] FastFriendship AOB @ %p\n", reinterpret_cast<void*>(addr));

    const std::size_t patchLen = 15;
    std::uint8_t* cave = AllocCave(128);
    if (!cave) { Log("[E] FastFriendship: cave alloc failed\n"); return false; }

    std::int32_t callDisp{};
    std::memcpy(&callDisp, reinterpret_cast<const void*>(addr + 7), 4);
    std::uintptr_t callTarget = addr + 11 + callDisp;

    std::int8_t jleOffset{};
    std::memcpy(&jleOffset, reinterpret_cast<const void*>(addr + 14), 1);
    std::uintptr_t jleTarget = addr + 15 + jleOffset;

    std::uintptr_t returnTarget = addr + 15;

    std::uint8_t* c = cave;

    // test rax, rax -> 48 85 C0
    *c++ = 0x48; *c++ = 0x85; *c++ = 0xC0;

    // jz is_zero_target
    std::uint8_t* jzInst = c;
    *c++ = 0x74; *c++ = 0x00;

    // cmp qword ptr [rax+20], 0x64 -> 48 83 78 20 64
    *c++ = 0x48; *c++ = 0x83; *c++ = 0x78; *c++ = 0x20; *c++ = 0x64;

    // jae short skip_write -> 73 08
    *c++ = 0x73; *c++ = 0x08;

    // mov qword ptr [rax+20], 0x64 -> 48 C7 40 20 64 00 00 00
    *c++ = 0x48; *c++ = 0xC7; *c++ = 0x40; *c++ = 0x20; *c++ = 0x64; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00;

    // skip_write:
    // mov rcx, [rax+20] -> 48 8B 48 20
    *c++ = 0x48; *c++ = 0x8B; *c++ = 0x48; *c++ = 0x20;

    // mov r11, callTarget -> 49 BB [8-byte address]
    *c++ = 0x49; *c++ = 0xBB;
    std::memcpy(c, &callTarget, 8); c += 8;

    // call r11 -> 41 FF D3
    *c++ = 0x41; *c++ = 0xFF; *c++ = 0xD3;

    // cmp al, 0xFE -> 3C FE
    *c++ = 0x3C; *c++ = 0xFE;

    // jle jle_target
    *c++ = 0x7E; *c++ = 0x0D;

    // returnTarget path: mov r11, returnTarget; jmp r11
    *c++ = 0x49; *c++ = 0xBB;
    std::memcpy(c, &returnTarget, 8); c += 8;
    *c++ = 0x41; *c++ = 0xFF; *c++ = 0xE3;

    // jle_target:
    std::uint8_t* jleTargetPos = c;
    *c++ = 0x49; *c++ = 0xBB;
    std::memcpy(c, &jleTarget, 8); c += 8;
    *c++ = 0x41; *c++ = 0xFF; *c++ = 0xE3;

    // is_zero_target:
    std::uint8_t* isZeroTargetPos = c;
    *c++ = 0x49; *c++ = 0xBB;
    std::memcpy(c, &returnTarget, 8); c += 8;
    *c++ = 0x41; *c++ = 0xFF; *c++ = 0xE3;

    // Patch placeholder
    std::size_t dist = isZeroTargetPos - (jzInst + 2);
    jzInst[1] = static_cast<std::uint8_t>(dist);

    ProtectCave(cave, 128);

    // Install patch
    std::uint8_t jmp[15];
    jmp[0] = 0xFF; jmp[1] = 0x25;
    std::memset(jmp + 2, 0, 4);
    std::uintptr_t caveAddr = reinterpret_cast<std::uintptr_t>(cave);
    std::memcpy(jmp + 6, &caveAddr, 8);
    jmp[14] = 0x90; // NOP

    RecordAndPatch(addr, jmp, patchLen, cave);
    Log("[+] FastFriendship installed (cave=%p)\n", cave);
    return true;
}

constexpr const char* kAob_FastPetFriendship =
    "C5 ?? 10 ?? C5 ?? 10 ?? 20 C5 ?? 10 ?? 40 C5 ?? 10 ?? 50 C5";

bool InstallFastPetFriendship() {
    std::uintptr_t addr = ScanModule(kAob_FastPetFriendship);
    if (!addr) { Log("[E] FastPetFriendship AOB not found\n"); return false; }
    Log("[i] FastPetFriendship AOB @ %p\n", reinterpret_cast<void*>(addr));

    const std::size_t patchLen = 14;
    std::uint8_t* cave = AllocCave(64);
    if (!cave) { Log("[E] FastPetFriendship: cave alloc failed\n"); return false; }

    std::uint8_t* c = cave;

    // cmp dword ptr [rbx+20], 95 (0x5F) -> 83 7B 20 5F
    *c++ = 0x83; *c++ = 0x7B; *c++ = 0x20; *c++ = 0x5F;

    // jae short code (+7 bytes to skip the mov)
    *c++ = 0x73; *c++ = 0x07;

    // mov dword ptr [rbx+20], 95 (0x5F) -> C7 43 20 5F 00 00 00
    *c++ = 0xC7; *c++ = 0x43; *c++ = 0x20; *c++ = 0x5F; *c++ = 0x00; *c++ = 0x00; *c++ = 0x00;

    // Copy original 14 bytes
    std::memcpy(c, reinterpret_cast<const void*>(addr), patchLen);
    c += patchLen;

    // Absolute JMP back (addr + patchLen)
    std::uintptr_t retAddr = addr + patchLen;
    *c++ = 0xFF; *c++ = 0x25; *c++ = 0; *c++ = 0; *c++ = 0; *c++ = 0;
    std::memcpy(c, &retAddr, 8); c += 8;

    ProtectCave(cave, 64);

    // Install patch
    std::uint8_t jmp[14];
    jmp[0] = 0xFF; jmp[1] = 0x25;
    std::memset(jmp + 2, 0, 4);
    std::uintptr_t caveAddr = reinterpret_cast<std::uintptr_t>(cave);
    std::memcpy(jmp + 6, &caveAddr, 8);

    RecordAndPatch(addr, jmp, patchLen, cave);
    Log("[+] FastPetFriendship installed (cave=%p)\n", cave);
    return true;
}

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------
DWORD WINAPI InitThreadProc(void*) {
    ResolvePluginDir();
    LoadConfig();
    OpenLog();

    Log("================================================\n");
    Log("  %s\n", kModName);
    Log("  Built %s %s\n", __DATE__, __TIME__);
    Log("================================================\n");
    Log("[i] AbyssGearNoDurLoss=%d InfPolishDur=%d EzParry=%d FastDragonCD=%d RepNoDecrease=%d FastFriendship=%d FastPetFriendship=%d\n",
        g_featAbyssGearNoDur, g_featInfPolishDur, g_featEzParry,
        g_featFastDragonCD, g_featRepNoDecrease, g_featFastFriendship, g_featFastPetFriendship);

    if (g_featAbyssGearNoDur)    InstallAbyssGearNoDur();
    if (g_featInfPolishDur)      InstallInfPolishDur();
    if (g_featEzParry)           InstallEzParry();
    if (g_featFastDragonCD)      InstallFastDragonCD();
    if (g_featRepNoDecrease)     InstallRepNoDecrease();
    if (g_featFastFriendship)    InstallFastFriendship();
    if (g_featFastPetFriendship) InstallFastPetFriendship();

    Log("[+] Init complete\n");
    return 0;
}

void Shutdown() {
    UninstallAll();
    Log("[i] Shutdown complete\n");
    if (g_logFile) { std::fclose(g_logFile); g_logFile = nullptr; }
}

} // namespace

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_selfModule = hModule;
        DisableThreadLibraryCalls(hModule);

        if (!IsTargetProcess()) return TRUE;

        HANDLE t = CreateThread(nullptr, 0, InitThreadProc, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (reserved == nullptr) Shutdown();
    }
    return TRUE;
}
