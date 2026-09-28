#include "DiskScanner.h"
#include <windows.h>
#include <string>
#include <vector>

std::vector<DriveInfo> DiskScanner::listDrives() {
    std::vector<DriveInfo> drives;
    DWORD driveMask = GetLogicalDrives();

    for (int i = 0; i < 26; ++i) {
        if (!(driveMask & (1 << i))) continue;

        char letter = 'A' + i;
        std::string root = std::string(1, letter) + ":\\";
        UINT driveType = GetDriveTypeA(root.c_str());

        if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE) continue;

        DriveInfo info;
        info.driveLetter = std::string(1, letter) + ":";
        info.rawPath = "\\\\.\\" + info.driveLetter;
        info.isRemovable = (driveType == DRIVE_REMOVABLE);
        info.fsType = detectFilesystem(root);

        char labelBuf[MAX_PATH] = {};
        char fsBuf[MAX_PATH] = {};
        ULARGE_INTEGER totalBytes{};
        GetVolumeInformationA(root.c_str(), labelBuf, MAX_PATH, nullptr, nullptr, nullptr, fsBuf, MAX_PATH);
        info.label = std::string(labelBuf);

        ULARGE_INTEGER freeBytesAvailable{}, totalNumberOfBytes{}, totalNumberOfFreeBytes{};
        if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes))
            info.totalSize = totalNumberOfBytes.QuadPart;
        else
            info.totalSize = 0;

        drives.push_back(info);
    }
    return drives;
}

HANDLE DiskScanner::openDrive(const std::string& rawPath) {
    return CreateFileA(
        rawPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
        nullptr
    );
}

FilesystemType DiskScanner::detectFilesystem(const std::string& driveLetter) {
    char fsBuf[MAX_PATH] = {};
    std::string root = driveLetter;
    if (root.back() != '\\') root += '\\';
    GetVolumeInformationA(root.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fsBuf, MAX_PATH);
    std::string fs(fsBuf);
    if (fs == "NTFS")   return FilesystemType::NTFS;
    if (fs == "FAT32")  return FilesystemType::FAT32;
    if (fs == "exFAT")  return FilesystemType::EXFAT;
    return FilesystemType::UNKNOWN;
}

uint64_t DiskScanner::getDriveSize(HANDLE hDrive) {
    LARGE_INTEGER size{};
    DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &size, sizeof(size), nullptr, nullptr);
    return static_cast<uint64_t>(size.QuadPart);
}
