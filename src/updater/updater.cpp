#include <string>
#include <iostream>
#include <regex>
#include <windows.h>

#include <cpr/cpr.h>

// Have to include getopt in the compilation unit or else its compiled with the wrong runtime
#include "getopt.h"

void startProcessWrapper(std::string name, std::string args, boolean wait) { //) {//, boolean hidden,
    HANDLE stdout_handle = NULL;
    STARTUPINFO startupInfo={sizeof(startupInfo)};
    // if (hidden) {
        startupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startupInfo.wShowWindow = SW_HIDE;
        startupInfo.hStdOutput = stdout_handle;
    // }
    PROCESS_INFORMATION processInfo;
    LPSTR cmdArgs = const_cast<char *>(args.c_str());
    if (!CreateProcess(name.c_str(), cmdArgs,
        NULL, NULL, FALSE,
        //(hidden) ? CREATE_NO_WINDOW : 0
        CREATE_NO_WINDOW
        , NULL, NULL, &startupInfo, &processInfo)
    ) {
        std::cout << "CreateProcess failed (" << GetLastError() << ")" << std::endl;
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    }
    if (wait) {
        WaitForSingleObject(&processInfo.hProcess, INFINITE);
    }
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
}

void displayWelcome() {
    MessageBoxA(NULL, "Welcome to Lemon, the Citra Beta builds! \n\nPlease understand that these builds are made "
     "automatically by a bot, so don't expect any support for them on the forum or github.",
     "Lemon - The Citra Beta", MB_OK | MB_ICONINFORMATION);
}

// void startCitra() {
//     startProcessWrapper("citra-qt.exe", "", false, true);
// }

void installMSVCRuntime(const char* msvc_redist_exe) {
    // this updater is linked statically with the runtime so it should be safe to run on any system
    // TODO change this to support 32 bit as well?
    // check to see if they have a recent enough version of the runtime

    WCHAR szBuffer[512];
    DWORD dwBufferSize = sizeof(szBuffer);
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\DevDiv\\vc\\Servicing\\14.0\\RuntimeMinimum", 0, KEY_READ, &hKey);
    if (RegQueryValueExW(hKey, L"Version", 0, NULL, (LPBYTE)szBuffer, &dwBufferSize)) {
        // The installer uses a UAC Prompt so we need to use ShellExecuteEx to wait for it to finish
        SHELLEXECUTEINFO shExInfo = {0};
        shExInfo.cbSize = sizeof(shExInfo);
        shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExInfo.hwnd = 0;
        shExInfo.lpVerb = "runas";
        shExInfo.lpFile = msvc_redist_exe;
        shExInfo.lpParameters = "/silent /install /quiet";
        shExInfo.lpDirectory = 0;
        shExInfo.nShow = SW_HIDE;
        shExInfo.hInstApp = 0;
        if (ShellExecuteEx(&shExInfo)) {
            WaitForSingleObject(shExInfo.hProcess, INFINITE);
        }
        CloseHandle(shExInfo.hProcess);
    }
}

int main(int argc, char** argv) {
    int option_index = 0;
    int argc_w;
    auto argv_w = CommandLineToArgvW(GetCommandLineW(), &argc_w);

    if (argv_w == nullptr) {
        std::cout << "Failed to get command line arguments" << std::endl;
        return -1;
    }

    static struct option long_options[] = {
        { "squirrel-firstrun", no_argument, 0, 'f' },
        { "squirrel-install", required_argument, 0, 'i' },
        { "squirrel-obsolete", required_argument, 0, 'o' },
        { "squirrel-updated", required_argument, 0, 'u' },
        { "squirrel-uninstall", required_argument, 0, 'r' },
        { 0, 0, 0, 0 }
    };

    // Get the path to the vcredist
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::string fullpath = std::string(buffer);
    std::string path = fullpath.substr(0, fullpath.find_last_of("\\/"));
    std::string msvc_redist_exe = path + "\\vcredist_x64.exe";
    std::string updater_exe = path.substr(0, path.find_last_of("\\/") + 1) + "Update.exe";
    std::cout << fullpath << std::endl;
    std::cout << path << std::endl;
    std::cout << msvc_redist_exe << std::endl;
    std::cout << updater_exe << std::endl;
    while (optind < argc) {
        char arg = getopt_long(argc, argv, "fi:o:u:r:", long_options, &option_index);
        if (arg != -1) {
            switch (arg) {
            case 'f':
                std::cout << "Displayed Welcome" << std::endl;
                displayWelcome();
                std::cout << "Start citra" << std::endl;
                startProcessWrapper(updater_exe, "Update.exe --processStart=citra-qt.exe", false); // true, false);
                return 0;
            case 'i':
                std::cout << "Trying to install msvc redist" << std::endl;
                installMSVCRuntime(msvc_redist_exe.c_str());
                std::cout << "Trying to make a shortcut" << std::endl;
                startProcessWrapper(updater_exe, "Update.exe --createShortcut=updater.exe", true); //, true, false);
                return 0;
            case 'o':
                std::cout << "Obsolete do nothing?" << std::endl;
                startProcessWrapper(updater_exe, "Update.exe --processStart=citra-qt.exe", false); // true, false);
                return 0;
            case 'u':
                std::cout << "Update... do nothing?" << std::endl;
                startProcessWrapper(updater_exe, "Update.exe --processStart=citra-qt.exe", false); // true, false);
                return 0;
            case 'r':
                std::cout << "Uninstalling. Remove shortcut" << std::endl;
                startProcessWrapper(updater_exe, "Update.exe --removeShortcut=updater.exe", true); //, true, false);
                return 0;
            }
        } else {
            optind++;
        }
    }
    // Normal Run so just start citra-qt
    startProcessWrapper(updater_exe, "Update.exe --processStart=citra-qt.exe", false); //, true, true);

    // check for updates in the background
    // fetch the latest tag number from github
    auto r = cpr::Get(cpr::Url{"https://api.github.com/repos/jroweboy/lemon/releases/latest"});
    if (r.status_code == 200) {
        // fetch the tag name from the json
        std::regex tag_regex(".*\"tag_name\":\\s*\"(.*)\".*");
        std::smatch match;
        if (std::regex_search(r.text, match, tag_regex)) {
            std::string tag_name = match[1];
            // download the latest version and get it ready
            startProcessWrapper(updater_exe, "Update.exe --update=https://www.github.com/jroweboy/lemon/"+tag_name, true);
            // and we are done! Next boot it'll point to the new version.
        }
    }

    return 0;
}
