#include "FileCarver.h"
#include <cstring>

void FileCarver::initSignatures() {
    signatures = {
        { "jpg",  {0xFF,0xD8,0xFF},                              {0xFF,0xD9},                            10*1024*1024  },
        { "png",  {0x89,0x50,0x4E,0x47},                        {0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82}, 20*1024*1024 },
        { "gif",  {0x47,0x49,0x46,0x38},                        {0x00,0x3B},                            5*1024*1024   },
        { "bmp",  {0x42,0x4D},                                  {},                                     10*1024*1024  },
        { "pdf",  {0x25,0x50,0x44,0x46},                        {0x25,0x25,0x45,0x4F,0x46},             50*1024*1024  },
        // zip must come before the PK-based Office formats so the footer {PK\x05\x06} is used
        // to bound zip files; docx/xlsx/pptx are differentiated by inspecting the local file
        // header filename field for '[Content_Types].xml' (Office Open XML marker).
        { "zip",  {0x50,0x4B,0x03,0x04},                        {0x50,0x4B,0x05,0x06},                  100*1024*1024 },
        { "mp3",  {0xFF,0xFB},                                  {},                                     20*1024*1024  },
        { "mp4",  {0x00,0x00,0x00,0x18,0x66,0x74,0x79,0x70},   {},                                     2ULL*1024*1024*1024 },
        { "avi",  {0x52,0x49,0x46,0x46},                        {},                                     2ULL*1024*1024*1024 },
        { "exe",  {0x4D,0x5A},                                  {},                                     50*1024*1024  },
        { "xml",  {0x3C,0x3F,0x78,0x6D,0x6C},                  {},                                     5*1024*1024   },
        { "txt",  {0xEF,0xBB,0xBF},                             {},                                     5*1024*1024   },
    };
}

FileCarver::FileCarver() {
    initSignatures();
}

std::vector<RecoveredFile> FileCarver::carve(
    HANDLE hDrive,
    uint64_t driveSize,
    std::function<void(int, int)> progressCallback)
{
    std::vector<RecoveredFile> results;

    // Compute the maximum header length across all signatures for the overlap window.
    size_t maxHeaderLen = 0;
    for (const auto& sig : signatures)
        maxHeaderLen = std::max(maxHeaderLen, sig.header.size());

    // Buffer = 1MB read area + overlap prefix from previous chunk.
    std::vector<uint8_t> buffer(READ_BUFFER + maxHeaderLen);

    uint64_t diskOffset = 0;    // current position on disk (start of the fresh data in buffer)
    size_t   overlap    = 0;    // bytes carried over from the previous chunk
    int totalChunks = static_cast<int>(driveSize / READ_BUFFER) + 1;
    int chunkIndex  = 0;

    while (diskOffset < driveSize) {
        // Seek to the start of the new data (after the overlap region).
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(diskOffset);
        SetFilePointerEx(hDrive, li, nullptr, FILE_BEGIN);

        DWORD toRead = static_cast<DWORD>(
            std::min(static_cast<uint64_t>(READ_BUFFER), driveSize - diskOffset));
        DWORD bytesRead = 0;
        if (!ReadFile(hDrive, buffer.data() + overlap, toRead, &bytesRead, nullptr) || bytesRead == 0)
            break;

        DWORD totalInBuffer = static_cast<DWORD>(overlap + bytesRead);

        // Scan the combined buffer (overlap + fresh data).
        for (DWORD i = 0; i + 4 < totalInBuffer; ++i) {
            for (const auto& sig : signatures) {
                size_t hLen = sig.header.size();
                if (i + hLen > totalInBuffer) continue;
                if (memcmp(buffer.data() + i, sig.header.data(), hLen) != 0) continue;

                // Absolute byte offset of this match on the raw disk.
                // i < overlap means the match started in the previous chunk's tail.
                uint64_t matchOffset = (diskOffset - overlap) + i;

                RecoveredFile file;
                file.extension    = sig.extension;
                file.fileName     = "carved_" + std::to_string(matchOffset) + "." + sig.extension;
                file.offsetOnDisk = matchOffset;
                file.method       = RecoveryMethod::CARVING;
                file.deletedDate  = "Unknown";
                file.mftRecordNumber = 0;

                // Try to find the footer within the current buffer window.
                uint64_t estimatedSize = sig.maxSize;
                if (!sig.footer.empty()) {
                    size_t fLen = sig.footer.size();
                    for (DWORD j = i + static_cast<DWORD>(hLen); j + fLen <= totalInBuffer; ++j) {
                        if (memcmp(buffer.data() + j, sig.footer.data(), fLen) == 0) {
                            estimatedSize = (j + fLen) - i;
                            break;
                        }
                    }
                }
                file.fileSize     = estimatedSize;
                file.isRecoverable = true;
                results.push_back(file);
                i += static_cast<DWORD>(hLen) - 1;
                break;
            }
        }

        // Carry the last maxHeaderLen bytes into the next iteration as the overlap.
        overlap = std::min(static_cast<size_t>(totalInBuffer), maxHeaderLen);
        if (overlap > 0)
            std::memmove(buffer.data(), buffer.data() + totalInBuffer - overlap, overlap);

        diskOffset += bytesRead;
        ++chunkIndex;
        if (progressCallback) progressCallback(chunkIndex, totalChunks);
    }

    return results;
}
