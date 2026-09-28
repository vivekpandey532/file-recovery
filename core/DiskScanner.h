#pragma once
#include <string>
#include <vector>
#include <windows.h>

enum class FilesystemType { NTFS, FAT32, EXFAT, UNKNOWN };

struct DriveInfo {
    std::string driveLetter;     // e.g. "C:"
    std::string label;
    std::string rawPath;         // e.g. "\\.\C:"
    FilesystemType fsType;
    uint64_t totalSize;
    bool isRemovable;
};

class DiskScanner {
public:
    static std::vector<DriveInfo> listDrives();
    static HANDLE openDrive(const std::string& rawPath);
    static FilesystemType detectFilesystem(const std::string& driveLetter);
    static uint64_t getDriveSize(HANDLE hDrive);
};
