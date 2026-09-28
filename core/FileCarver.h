#pragma once
#include "FileRecovery.h"
#include <windows.h>
#include <vector>
#include <functional>

struct FileSignature {
    std::string extension;
    std::vector<uint8_t> header;    // magic bytes at start
    std::vector<uint8_t> footer;    // optional end marker
    uint64_t maxSize;               // max expected file size in bytes
};

class FileCarver {
public:
    FileCarver();

    std::vector<RecoveredFile> carve(
        HANDLE hDrive,
        uint64_t driveSize,
        std::function<void(int, int)> progressCallback = nullptr
    );

private:
    std::vector<FileSignature> signatures;
    void initSignatures();

    static constexpr uint32_t SECTOR_SIZE  = 512;
    static constexpr uint32_t READ_BUFFER  = 1024 * 1024; // 1MB chunks
};
