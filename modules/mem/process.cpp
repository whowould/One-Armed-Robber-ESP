#include "process.h"
#include "../offsets/offsets.h"
#include <cstdio>

Process::Process() {
    if (const auto ntdll = GetModuleHandleW(L"ntdll.dll"))
        nt_read_ = reinterpret_cast<NtReadVirtualMemory_t>(GetProcAddress(ntdll, "NtReadVirtualMemory"));
}

bool Process::attach(const wchar_t* name) {
    close();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, name) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    if (!pid)
        return false;

    handle_ = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!handle_)
        return false;

    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) {
        close();
        return false;
    }
    MODULEENTRY32W mod{};
    mod.dwSize = sizeof(mod);
    if (Module32FirstW(snap, &mod)) {
        do {
            if (_wcsicmp(mod.szModule, name) == 0) {
                base_ = reinterpret_cast<std::uintptr_t>(mod.modBaseAddr);
                break;
            }
        } while (Module32NextW(snap, &mod));
    }
    CloseHandle(snap);
    if (!base_)
        close();
    return base_ != 0;
}

void Process::close() {
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
    base_ = 0;
}

bool Process::read_bytes(std::uintptr_t address, void* out, size_t size) const {
    if (!handle_ || !nt_read_ || !address || !size)
        return false;
    SIZE_T got = 0;
    return nt_read_(handle_, reinterpret_cast<PVOID>(address), out, size, &got) >= 0 && got == size;
}

std::uintptr_t Process::find_vtable(std::uintptr_t vtable) const {
    const auto needle = vtable;
    std::uintptr_t cursor = 0;
    std::size_t scanned = 0;
    MEMORY_BASIC_INFORMATION info{};

    while (scanned < 0x80000000ull) {
        if (!VirtualQueryEx(handle_, reinterpret_cast<LPCVOID>(cursor), &info, sizeof(info)))
            break;
        const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        const auto size = info.RegionSize;
        const bool usable = info.State == MEM_COMMIT && info.Type == MEM_PRIVATE && size && size <= 0x10000000ull;
        if (usable) {
            std::vector<std::uint8_t> buf(size);
            if (read_bytes(base, buf.data(), size)) {
                for (size_t i = 0; i + 8 <= size; i += 8) {
                    std::uintptr_t value = 0;
                    memcpy(&value, buf.data() + i, 8);
                    if (value != needle)
                        continue;
                    const auto found = base + i;
                    const auto players = read<std::uintptr_t>(found + off::game_instance_local_players);
                    const auto count = read<std::int32_t>(found + off::game_instance_local_players + 8);
                    if (players && count == 1)
                        return found;
                }
            }
        }
        const auto next = base + size;
        if (next <= cursor)
            break;
        cursor = next;
        scanned += size;
    }
    return 0;
}
