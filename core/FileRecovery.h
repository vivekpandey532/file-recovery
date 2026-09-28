#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class RecoveryMethod { MFT, FAT, CARVING };

// Represents one contiguous run of clusters on disk (NTFS data runs)
struct DataRun {
    uint64_t offsetOnDisk;  // absolute byte offset on raw disk
    uint64_t length;        // byte length of this run
};

struct RecoveredFile {
    std::string fileName;
    std::string extension;
    uint64_t fileSize       = 0;
    std::string deletedDate;
    uint64_t offsetOnDisk   = 0;    // byte offset on raw disk (first run / carving start)
    uint64_t mftRecordNumber = 0;   // NTFS only
    RecoveryMethod method;
    bool isRecoverable      = false;
    std::vector<DataRun> dataRuns;  // NTFS: decoded data runs for fragmented files
};

class FileRecovery {
public:
    // Recover a file to destinationPath
    static bool recoverFile(const std::string& rawDrivePath,
                            const RecoveredFile& file,
                            const std::string& destinationPath);
private:
    static bool recoverByMFT(HANDLE hDrive, const RecoveredFile& file, const std::string& dest);
    static bool recoverByCarving(HANDLE hDrive, const RecoveredFile& file, const std::string& dest);
};
