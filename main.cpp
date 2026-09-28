#include <QApplication>
#include <QMessageBox>
#include <windows.h>
#include "ui/MainWindow.h"

static bool isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("File Recovery Tool");
    app.setApplicationVersion("1.0");
    app.setStyle("Fusion");

    if (!isRunningAsAdmin()) {
        QMessageBox::warning(nullptr, "Administrator Required",
            "This application requires Administrator privileges to access raw disk data.\n\n"
            "Please restart as Administrator (Right-click → Run as administrator).");
        return 1;
    }

    MainWindow window;
    window.show();
    return app.exec();
}
