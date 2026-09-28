#include "FATParser.h"
#include <cstring>
#include <ctime>
#include <set>

bool FATParser::readBootSector(HANDLE hDrive) {
    uint8_t buf[512] = {};
    DWORD bytesRead = 0;
    SetFilePointer(hDrive, 0, nullptr, FILE_BEGIN);
    if (!ReadFile(hDrive, buf, 512, &bytesRead, nullptr) || bytesRead != 512)
        return false;

    auto* boot = reinterpret_cast<FAT32BootSector*>(buf);
    if (strncmp(boot->fsType, "FAT32   ", 8) != 0) return false;

    bytesPerSector    = boot->bytesPerSector;
    sectorsPerCluster = boot->sectorsPerCluster;
    reservedSectors   = boot->reservedSectors;
    numFATs           = boot->numFATs;
    fatSize           = boot->fatSize32;
    rootCluster       = boot->rootCluster;
    return true;
}

uint64_t FATParser::clusterToOffset(uint32_t cluster) const {
    uint64_t dataStart = partitionOffset +
        (reservedSectors + numFATs * fatSize) * bytesPerSector;
    return dataStart + static_cast<uint64_t>(cluster - 2) * sectorsPerCluster * bytesPerSector;
}

std::string FATParser::fatDateTimeToString(uint16_t date, uint16_t time) {
    if (date == 0) return "Unknown";
    int year  = ((date >> 9) & 0x7F) + 1980;
    int month = (date >> 5) & 0x0F;
    int day   = date & 0x1F;
    int hour  = (time >> 11) & 0x1F;
    int min   = (time >> 5) & 0x3F;
    int sec   = (time & 0x1F) * 2;
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, min, sec);
    return std::string(buf);
}

void FATParser::scanDirectory(HANDLE hDrive, uint32_t cluster,
                               std::vector<RecoveredFile>& results,
                               std::set<uint32_t>& visitedDirs) {
    if (visitedDirs.count(cluster)) return;
    visitedDirs.insert(cluster);

    uint32_t cur = cluster;
    while (cur >= 2 && cur < 0x0FFFFFF8) {
        uint64_t offset = clusterToOffset(cur);
        uint32_t clusterSize = sectorsPerCluster * bytesPerSector;
        std::vector<uint8_t> buf(clusterSize);

        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(offset);
        SetFilePointerEx(hDrive, li, nullptr, FILE_BEGIN);

        DWORD bytesRead = 0;
        if (!ReadFile(hDrive, buf.data(), clusterSize, &bytesRead, nullptr)) break;

        uint32_t numEntries = clusterSize / sizeof(FAT32DirEntry);
        for (uint32_t i = 0; i < numEntries; ++i) {
            auto* entry = reinterpret_cast<FAT32DirEntry*>(buf.data() + i * sizeof(FAT32DirEntry));

            if (entry->name[0] == 0x00) goto next_cluster; // no more entries in this dir
            if (entry->attributes == FAT_ATTR_LFN) continue;
            if (entry->attributes & FAT_ATTR_VOLUME) continue;

            // Recurse into live (non-deleted) subdirectories
            if ((entry->attributes & FAT_ATTR_DIRECTORY) &&
                static_cast<uint8_t>(entry->name[0]) != FAT_ENTRY_DELETED &&
                entry->name[0] != '.' ) {
                uint32_t subCluster = (static_cast<uint32_t>(entry->firstClusterHigh) << 16)
                                    | entry->firstClusterLow;
                if (subCluster >= 2)
                    scanDirectory(hDrive, subCluster, results, visitedDirs);
                continue;
            }

            if (static_cast<uint8_t>(entry->name[0]) != FAT_ENTRY_DELETED) continue;
            if (entry->attributes & FAT_ATTR_DIRECTORY) continue; // skip deleted dirs

            RecoveredFile file;
            file.fileName = "?" + std::string(entry->name + 1, 7);
            while (!file.fileName.empty() && file.fileName.back() == ' ')
                file.fileName.pop_back();

            std::string ext(entry->ext, 3);
            while (!ext.empty() && ext.back() == ' ') ext.pop_back();
            if (!ext.empty()) {
                file.extension = ext;
                file.fileName += "." + ext;
            }

            file.fileSize    = entry->fileSize;
            file.deletedDate = fatDateTimeToString(entry->writeDate, entry->writeTime);
            file.method      = RecoveryMethod::FAT;
            file.isRecoverable = (entry->fileSize > 0);

            uint32_t startCluster = (static_cast<uint32_t>(entry->firstClusterHigh) << 16)
                                  | entry->firstClusterLow;
            file.offsetOnDisk    = clusterToOffset(startCluster);
            file.mftRecordNumber = 0;
            results.push_back(file);
        }

        next_cluster:
        // Follow FAT chain to next cluster of this directory
        uint64_t fatOffset = partitionOffset +
            static_cast<uint64_t>(reservedSectors) * bytesPerSector +
            cur * 4;
        LARGE_INTEGER li2;
        li2.QuadPart = static_cast<LONGLONG>(fatOffset);
        SetFilePointerEx(hDrive, li2, nullptr, FILE_BEGIN);
        uint32_t nextCluster = 0;
        DWORD rd = 0;
        ReadFile(hDrive, &nextCluster, 4, &rd, nullptr);
        nextCluster &= 0x0FFFFFFF;
        cur = nextCluster;
    }
}

std::vector<RecoveredFile> FATParser::scanDeletedFiles(
    HANDLE hDrive,
    std::function<void(int, int)> progressCallback)
{
    std::vector<RecoveredFile> results;
    if (!readBootSector(hDrive)) return results;
    if (progressCallback) progressCallback(0, 1);
    std::set<uint32_t> visitedDirs;
    scanDirectory(hDrive, rootCluster, results, visitedDirs);
    if (progressCallback) progressCallback(1, 1);
    return results;
}
