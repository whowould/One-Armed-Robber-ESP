#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <cstdint>
#include <vector>

using NtReadVirtualMemory_t = NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

class Process {
public:
    Process();
    ~Process() { close(); }

    bool attach(const wchar_t* name);
    void close();

    std::uintptr_t module_base() const { return base_; }
    HANDLE handle() const { return handle_; }
    bool alive() const { return handle_ != nullptr; }

    template <class T>
    T read(std::uintptr_t address) const {
        T value{};
        read_bytes(address, &value, sizeof(T));
        return value;
    }

    bool read_bytes(std::uintptr_t address, void* out, size_t size) const;
    std::uintptr_t find_vtable(std::uintptr_t vtable) const;

private:
    HANDLE handle_ = nullptr;
    std::uintptr_t base_ = 0;
    NtReadVirtualMemory_t nt_read_ = nullptr;
};
