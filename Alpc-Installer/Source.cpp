#include "InstallUninstall.hpp"

int main()
{
    printf("Available commands: \r\n");
    printf("   * install       - Installs the alpc-monitor solution in current directory. \r\n");
    printf("   * uninstall     - Uninstalls the alpc-monitor solution in current directory. \r\n");

    char command[0x1000] = { 0 };
    printf("Please input the command:\r\n");
    gets_s(command, sizeof(command));

    std::string_view commandView(command);

    /* We need to disable redirection for arch != x86. */
    PVOID oldRedirection = nullptr;
    if (OsArchitecture::ix86 != HelperCurrentOsArchitecture())
    {
        if (FALSE == Wow64DisableWow64FsRedirection(&oldRedirection))
        {
            printf("[!] Failed to disable fs redirection %d! \r\n",
                   GetLastError());
            return -1;
        }
    }

    if (commandView == "install")
    {
        DoInstall();
    }
    else if (commandView == "uninstall")
    {
        DoUninstall();
    }
    else
    {
        printf("[!] Unrecognized command %s!\r\n", command);
    }

    /* Revert the redirection. */
    if (OsArchitecture::ix86 != HelperCurrentOsArchitecture() && nullptr != oldRedirection)
    {
        Wow64RevertWow64FsRedirection(oldRedirection);
    }
    return 0;
}