#pragma once
#include "FileRecovery.h"
#include <windows.h>
#include <vector>
#include <functional>

#pragma pack(push, 1)
struct FAT32BootSector {
    uint8_t  jump[3];
    char     oemName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t  numFATs;
    uint16_t rootEntryCount;    // 0 for FAT32
    uint16_t totalSectors16;
    uint8_t  mediaType;
    uint16_t fatSize16;         // 0 for FAT32
    uint16_t sectorsPerTrack;
    uint16_t numberOfHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;
    uint32_t fatSize32;
    uint16_t extFlags;
    uint16_t fsVersion;
    uint32_t rootCluster;
    uint16_t fsInfoSector;
    uint16_t backupBootSector;
    uint8_t  reserved[12];
    uint8_t  driveNumber;
    uint8_t  reserved1;
    uint8_t  bootSignature;
    uint32_t volumeId;
    char     volumeLabel[11];
    char     fsType[8];         // "FAT32   "
};

struct FAT32DirEntry {
    char     name[8];
    char     ext[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  createTimeTenth;
    uint16_t createTime;
    uint16_t createDate;
    uint16_t lastAccessDate;
    uint16_t firstClusterHigh;
    uint16_t writeTime;
    uint16_t writeDate;
    uint16_t firstClusterLow;
    uint32_t fileSize;
};
#pragma pack(pop)

constexpr uint8_t FAT_ENTRY_DELETED  = 0xE5;
constexpr uint8_t FAT_ATTR_DIRECTORY = 0x10;
constexpr uint8_t FAT_ATTR_VOLUME    = 0x08;
constexpr uint8_t FAT_ATTR_LFN      = 0x0F;

class FATParser {
public:
    std::vector<RecoveredFile> scanDeletedFiles(
        HANDLE hDrive,
        std::function<void(int, int)> progressCallback = nullptr
    );

private:
    bool readBootSector(HANDLE hDrive);
    uint64_t clusterToOffset(uint32_t cluster) const;
    void scanDirectory(HANDLE hDrive, uint32_t cluster,
                       std::vector<RecoveredFile>& results,
                       std::set<uint32_t>& visitedDirs);
    std::string fatDateTimeToString(uint16_t date, uint16_t time);

    uint32_t bytesPerSector    = 512;
    uint32_t sectorsPerCluster = 8;
    uint32_t reservedSectors   = 32;
    uint32_t numFATs           = 2;
    uint32_t fatSize           = 0;
    uint32_t rootCluster       = 2;
    uint64_t partitionOffset   = 0;
};
