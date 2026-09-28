#include "NTFSParser.h"
#include <cstring>
#include <ctime>

#pragma pack(push, 1)
struct NTFSBootSector {
    uint8_t  jump[3];
    char     oemId[8];
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint8_t  reserved[7];
    uint8_t  mediaDescriptor;
    uint16_t reserved2;
    uint16_t sectorsPerTrack;
    uint16_t numberOfHeads;
    uint32_t hiddenSectors;
    uint32_t reserved3;
    uint32_t reserved4;
    uint64_t totalSectors;
    uint64_t mftClusterNumber;
    uint64_t mftMirrorCluster;
    int8_t   clustersPerMFTRecord;
    uint8_t  reserved5[3];
    int8_t   clustersPerIndexBlock;
    uint8_t  reserved6[3];
    uint64_t volumeSerialNumber;
    uint32_t checksum;
    uint8_t  bootCode[426];
    uint16_t endMarker;
};

struct FileNameAttribute {
    uint64_t parentDirectory;
    uint64_t creationTime;
    uint64_t modifiedTime;
    uint64_t mftModifiedTime;
    uint64_t accessTime;
    uint64_t allocSize;
    uint64_t realSize;
    uint32_t flags;
    uint32_t reparse;
    uint8_t  fileNameLength;
    uint8_t  fileNamespace;
    uint16_t fileName[1];
};

struct StandardInformation {
    uint64_t creationTime;
    uint64_t modifiedTime;
    uint64_t mftModifiedTime;
    uint64_t accessTime;
};
#pragma pack(pop)

static std::string windowsTimeToString(uint64_t winTime) {
    if (winTime == 0) return "Unknown";
    ULARGE_INTEGER uli;
    uli.QuadPart = winTime - 116444736000000000ULL;
    time_t t = uli.QuadPart / 10000000ULL;
    char buf[32];
    struct tm tm_info;
    gmtime_s(&tm_info, &t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
    return std::string(buf);
}

static std::string wideToUtf8(const uint16_t* wstr, int len) {
    if (len <= 0) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)wstr, len, nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)wstr, len, result.data(), size, nullptr, nullptr);
    return result;
}

bool NTFSParser::locateMFT(HANDLE hDrive) {
    uint8_t bootSector[512] = {};
    DWORD bytesRead = 0;
    SetFilePointer(hDrive, 0, nullptr, FILE_BEGIN);
    if (!ReadFile(hDrive, bootSector, 512, &bytesRead, nullptr) || bytesRead != 512)
        return false;

    auto* boot = reinterpret_cast<NTFSBootSector*>(bootSector);
    if (strncmp(boot->oemId, "NTFS    ", 8) != 0) return false;

    bytesPerSector    = boot->bytesPerSector;
    sectorsPerCluster = boot->sectorsPerCluster;
    bytesPerCluster   = bytesPerSector * sectorsPerCluster;

    if (boot->clustersPerMFTRecord < 0)
        bytesPerMFTRecord = 1u << (-boot->clustersPerMFTRecord);
    else
        bytesPerMFTRecord = boot->clustersPerMFTRecord * bytesPerCluster;

    mftOffset = boot->mftClusterNumber * bytesPerCluster;
    return true;
}

void NTFSParser::applyFixup(uint8_t* record, size_t recordSize) {
    auto* header = reinterpret_cast<MFTRecordHeader*>(record);
    uint16_t* updateSeq = reinterpret_cast<uint16_t*>(record + header->updateSeqOffset);
    uint16_t seqNum = updateSeq[0];
    for (uint16_t i = 1; i < header->updateSeqSize; ++i) {
        size_t sectorEnd = i * bytesPerSector - 2;
        if (sectorEnd + 1 < recordSize)
            *reinterpret_cast<uint16_t*>(record + sectorEnd) = updateSeq[i];
    }
}

bool NTFSParser::parseMFTRecord(const uint8_t* record, RecoveredFile& outFile) {
    auto* header = reinterpret_cast<const MFTRecordHeader*>(record);

    if (strncmp(header->signature, "FILE", 4) != 0) return false;
    // flags: 0 = deleted file, 2 = deleted directory — we want deleted files only
    if (header->flags != 0) return false;

    outFile.mftRecordNumber = header->mftRecordNumber;
    outFile.method = RecoveryMethod::MFT;
    outFile.isRecoverable = true;

    uint32_t offset = header->firstAttrOffset;
    bool hasFileName = false;

    while (offset + sizeof(AttributeHeader) <= bytesPerMFTRecord) {
        auto* attr = reinterpret_cast<const AttributeHeader*>(record + offset);
        if (attr->type == ATTR_END || attr->length == 0) break;

        if (attr->type == ATTR_FILE_NAME && !attr->nonResident) {
            auto* res = reinterpret_cast<const ResidentAttributeHeader*>(record + offset + sizeof(AttributeHeader));
            auto* fn  = reinterpret_cast<const FileNameAttribute*>(record + offset + sizeof(AttributeHeader) + sizeof(ResidentAttributeHeader));
            if (fn->fileNamespace != 2) { // skip DOS-only names
                outFile.fileName    = wideToUtf8(fn->fileName, fn->fileNameLength);
                outFile.fileSize    = fn->realSize;
                outFile.deletedDate = windowsTimeToString(fn->modifiedTime);
                hasFileName = true;
                // extract extension
                auto dot = outFile.fileName.rfind('.');
                outFile.extension = (dot != std::string::npos) ? outFile.fileName.substr(dot + 1) : "";
            }
        }

        if (attr->type == ATTR_DATA && attr->nonResident) {
            auto* nr = reinterpret_cast<const NonResidentAttributeHeader*>(
                record + offset + sizeof(AttributeHeader));
            outFile.fileSize = nr->realSize;

            // Decode the data run list to get actual cluster locations on disk.
            // Each run entry: 1 header byte (high nibble = LCN bytes, low nibble = length bytes),
            // followed by the run length (in clusters) and a signed relative LCN delta.
            const uint8_t* runPtr = record + offset + sizeof(AttributeHeader) + nr->dataRunOffset;
            int64_t currentLCN = 0;

            while (*runPtr != 0x00) {
                uint8_t header   = *runPtr++;
                uint8_t lenBytes = header & 0x0F;
                uint8_t lcnBytes = (header >> 4) & 0x0F;

                if (lenBytes == 0) break;

                // Read run length (unsigned)
                uint64_t runLength = 0;
                for (uint8_t b = 0; b < lenBytes; ++b)
                    runLength |= static_cast<uint64_t>(*runPtr++) << (b * 8);

                // Read LCN delta (signed)
                int64_t lcnDelta = 0;
                if (lcnBytes > 0) {
                    uint64_t raw = 0;
                    for (uint8_t b = 0; b < lcnBytes; ++b)
                        raw |= static_cast<uint64_t>(*runPtr++) << (b * 8);
                    // Sign-extend if the high bit of the last byte is set
                    if (raw & (1ULL << (lcnBytes * 8 - 1)))
                        raw |= ~((1ULL << (lcnBytes * 8)) - 1);
                    lcnDelta = static_cast<int64_t>(raw);
                    currentLCN += lcnDelta;

                    DataRun dr;
                    dr.offsetOnDisk = static_cast<uint64_t>(currentLCN) * bytesPerCluster;
                    dr.length       = runLength * bytesPerCluster;
                    outFile.dataRuns.push_back(dr);
                }
                // lcnBytes == 0 means a sparse run (all zeros) — skip it
            }

            // Set offsetOnDisk to the first run's start for display / single-run fallback
            if (!outFile.dataRuns.empty())
                outFile.offsetOnDisk = outFile.dataRuns[0].offsetOnDisk;
        }

        offset += attr->length;
    }

    return hasFileName;
}

std::vector<RecoveredFile> NTFSParser::scanDeletedFiles(
    HANDLE hDrive,
    std::function<void(int, int)> progressCallback)
{
    std::vector<RecoveredFile> results;
    if (!locateMFT(hDrive)) return results;

    // Read $MFT data attribute to get total MFT size
    std::vector<uint8_t> mftRecord(bytesPerMFTRecord);
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(mftOffset);
    SetFilePointerEx(hDrive, li, nullptr, FILE_BEGIN);

    DWORD bytesRead = 0;
    if (!ReadFile(hDrive, mftRecord.data(), bytesPerMFTRecord, &bytesRead, nullptr))
        return results;

    applyFixup(mftRecord.data(), bytesPerMFTRecord);

    // Find $MFT DATA attribute to get total MFT size
    uint64_t mftTotalSize = 0;
    auto* hdr = reinterpret_cast<MFTRecordHeader*>(mftRecord.data());
    uint32_t off = hdr->firstAttrOffset;
    while (off + sizeof(AttributeHeader) <= bytesPerMFTRecord) {
        auto* attr = reinterpret_cast<AttributeHeader*>(mftRecord.data() + off);
        if (attr->type == ATTR_END || attr->length == 0) break;
        if (attr->type == ATTR_DATA && attr->nonResident) {
            auto* nr = reinterpret_cast<NonResidentAttributeHeader*>(
                mftRecord.data() + off + sizeof(AttributeHeader));
            mftTotalSize = nr->realSize;
        }
        off += attr->length;
    }

    uint64_t totalRecords = mftTotalSize / bytesPerMFTRecord;
    std::vector<uint8_t> recBuf(bytesPerMFTRecord);

    for (uint64_t i = 0; i < totalRecords; ++i) {
        if (progressCallback && (i % 1000 == 0))
            progressCallback(static_cast<int>(i), static_cast<int>(totalRecords));

        uint64_t recordOffset = mftOffset + i * bytesPerMFTRecord;
        li.QuadPart = static_cast<LONGLONG>(recordOffset);
        SetFilePointerEx(hDrive, li, nullptr, FILE_BEGIN);

        bytesRead = 0;
        if (!ReadFile(hDrive, recBuf.data(), bytesPerMFTRecord, &bytesRead, nullptr))
            continue;
        if (bytesRead < bytesPerMFTRecord) continue;

        applyFixup(recBuf.data(), bytesPerMFTRecord);

        RecoveredFile file;
        file.offsetOnDisk = recordOffset;
        if (parseMFTRecord(recBuf.data(), file))
            results.push_back(file);
    }

    if (progressCallback) progressCallback(static_cast<int>(totalRecords), static_cast<int>(totalRecords));
    return results;
}
