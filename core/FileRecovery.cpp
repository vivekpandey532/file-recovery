#include "FileRecovery.h"
#include "DiskScanner.h"
#include <windows.h>
#include <fstream>
#include <vector>

bool FileRecovery::recoverFile(const std::string& rawDrivePath,
                                const RecoveredFile& file,
                                const std::string& destinationPath)
{
    HANDLE hDrive = DiskScanner::openDrive(rawDrivePath);
    if (hDrive == INVALID_HANDLE_VALUE) return false;

    bool ok = false;
    if (file.method == RecoveryMethod::CARVING)
        ok = recoverByCarving(hDrive, file, destinationPath);
    else
        ok = recoverByMFT(hDrive, file, destinationPath);

    CloseHandle(hDrive);
    return ok;
}

// Reads a contiguous region from hDrive into an open ofstream, up to `length` bytes.
static bool readRegion(HANDLE hDrive, uint64_t offset, uint64_t length, std::ofstream& out) {
    constexpr uint32_t CHUNK = 512 * 1024;
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(hDrive, li, nullptr, FILE_BEGIN)) return false;

    std::vector<uint8_t> buf(CHUNK);
    uint64_t remaining = length;
    while (remaining > 0) {
        DWORD toRead = static_cast<DWORD>(std::min(static_cast<uint64_t>(CHUNK), remaining));
        DWORD bytesRead = 0;
        if (!ReadFile(hDrive, buf.data(), toRead, &bytesRead, nullptr) || bytesRead == 0) return false;
        out.write(reinterpret_cast<char*>(buf.data()), bytesRead);
        remaining -= bytesRead;
    }
    return true;
}

// MFT recovery: reassemble file by walking decoded data runs in order.
bool FileRecovery::recoverByMFT(HANDLE hDrive, const RecoveredFile& file, const std::string& dest) {
    if (file.fileSize == 0) return false;

    // If data runs were decoded by NTFSParser, use them to reassemble the file.
    if (!file.dataRuns.empty()) {
        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;

        uint64_t remaining = file.fileSize;
        for (const auto& run : file.dataRuns) {
            if (remaining == 0) break;
            uint64_t toRead = std::min(run.length, remaining);
            if (!readRegion(hDrive, run.offsetOnDisk, toRead, out)) return false;
            remaining -= toRead;
        }
        return out.good();
    }

    // Fallback: single contiguous read from offsetOnDisk (e.g. resident or single-run).
    return recoverByCarving(hDrive, file, dest);
}

bool FileRecovery::recoverByCarving(HANDLE hDrive, const RecoveredFile& file, const std::string& dest) {
    if (file.fileSize == 0 || file.offsetOnDisk == 0) return false;

    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    return readRegion(hDrive, file.offsetOnDisk, file.fileSize, out) && out.good();
}
