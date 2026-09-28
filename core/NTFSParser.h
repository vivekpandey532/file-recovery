#pragma once
#include "FileRecovery.h"
#include "DiskScanner.h"
#include <windows.h>
#include <vector>
#include <functional>

#pragma pack(push, 1)
struct MFTRecordHeader {
    char     signature[4];       // "FILE"
    uint16_t updateSeqOffset;
    uint16_t updateSeqSize;
    uint64_t logFileSeqNum;
    uint16_t sequenceNumber;
    uint16_t hardLinkCount;
    uint16_t firstAttrOffset;
    uint16_t flags;              // 0=deleted file, 1=in-use, 2=deleted dir, 3=in-use dir
    uint32_t usedSize;
    uint32_t allocSize;
    uint64_t baseFileRecord;
    uint16_t nextAttrId;
    uint16_t padding;
    uint32_t mftRecordNumber;
};

struct AttributeHeader {
    uint32_t type;
    uint32_t length;
    uint8_t  nonResident;
    uint8_t  nameLength;
    uint16_t nameOffset;
    uint16_t flags;
    uint16_t attributeId;
};

struct ResidentAttributeHeader {
    uint32_t contentLength;
    uint16_t contentOffset;
    uint16_t unused;
};

struct NonResidentAttributeHeader {
    uint64_t startingVCN;
    uint64_t lastVCN;
    uint16_t dataRunOffset;
    uint16_t compressionUnit;
    uint32_t padding;
    uint64_t allocSize;
    uint64_t realSize;
    uint64_t initializedSize;
};
#pragma pack(pop)

// NTFS attribute types
constexpr uint32_t ATTR_STANDARD_INFO = 0x10;
constexpr uint32_t ATTR_FILE_NAME     = 0x30;
constexpr uint32_t ATTR_DATA          = 0x80;
constexpr uint32_t ATTR_END           = 0xFFFFFFFF;

class NTFSParser {
public:
    // progressCallback(current, total)
    std::vector<RecoveredFile> scanDeletedFiles(
        HANDLE hDrive,
        std::function<void(int, int)> progressCallback = nullptr
    );

private:
    bool locateMFT(HANDLE hDrive);
    bool parseMFTRecord(const uint8_t* record, RecoveredFile& outFile);
    void applyFixup(uint8_t* record, size_t recordSize);

    uint64_t mftOffset = 0;
    uint32_t bytesPerSector = 512;
    uint32_t sectorsPerCluster = 8;
    uint32_t bytesPerCluster = 4096;
    uint32_t bytesPerMFTRecord = 1024;
};
