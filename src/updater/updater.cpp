#include <string>
#include <iostream>
#include <regex>
#include <windows.h>

// Have to include getopt in the compilation unit or else its compiled with the wrong runtime
#include <cpr/cpr.h>
#include "getopt.h"
#include "tagname.h"

void startProcessWrapper(std::string name, std::string args, boolean wait) {
    HANDLE stdout_handle = NULL;
    STARTUPINFO startupInfo={sizeof(startupInfo)};
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;
    startupInfo.hStdOutput = stdout_handle;
    PROCESS_INFORMATION processInfo;
    LPSTR cmdArgs = const_cast<char *>(args.c_str());
    if (!CreateProcess(name.c_str(), cmdArgs,
        NULL, NULL, FALSE, CREATE_NO_WINDOW,
        NULL, NULL, &startupInfo, &processInfo)) {
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
    MessageBoxA(NULL, "You are currently running the Citra Nightly Unstable branch.\n"
        "This is a build of Citra with additional changes that have not yet been fully tested or deemed stable."
        "As such, please do not report bugs against this build.\n\n"
        "If you believe you've found a bug, please reattempt on our nightly stable builds.\n\n"
        "Citra will automatically update when new nightly builds are released.",
    "Lemon - The Citra Beta", MB_OK | MB_ICONINFORMATION);
}

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

    // Get the path to the main applications
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::string fullpath = std::string(buffer);
    std::string path = fullpath.substr(0, fullpath.find_last_of("\\/"));
    std::string msvc_redist_exe = path + "\\vcredist_x64.exe";
    std::string updater_exe = path.substr(0, path.find_last_of("\\/") + 1) + "Update.exe";
    while (optind < argc) {
        char arg = getopt_long(argc, argv, "fi:o:u:r:", long_options, &option_index);
        if (arg != -1) {
            switch (arg) {
            case 'f':
                // First time run. Display a welcome message and exit quickly.
                displayWelcome();
                startProcessWrapper(updater_exe, "Update.exe --processStart=citra-qt.exe", false);
                return 0;
            case 'i':
                // On install. Do what is needed and exist fast.
                installMSVCRuntime(msvc_redist_exe.c_str());
                startProcessWrapper(updater_exe, "Update.exe --createShortcut=updater.exe", true);
                return 0;
            case 'o':
                // Obsolete. Not sure what to use it for.
                return 0;
            case 'u':
                // Update. Fresh version so just run it an exit.
                startProcessWrapper(updater_exe, "Update.exe --processStart=citra-qt.exe", false);
                return 0;
            case 'r':
                // This throws an error when everything is somehow gone when it tries to remove the shortcut...
                startProcessWrapper(updater_exe, "Update.exe --removeShortcut=updater.exe", true);
                return 0;
            }
        } else {
            optind++;
        }
    }
    // check for updates in the background
    // fetch the latest tag number from github
    auto r = cpr::Get(cpr::Url{"https://api.github.com/repos/jroweboy/lemon/releases/latest"});
    if (r.status_code == 200) {
        // fetch the tag name from the json
        std::smatch match;
        std::regex tag_regex(".*\"tag_name\":\\s*\"(.*)\".*");
        if (std::regex_search(r.text, match, tag_regex)) {
            std::string tag_name = match[1];
            // if the version is the same, just start citra
            if (tag_name != Updater::tag_name) {
                // display a loading animation ??
                std::cout << "Hang tight! Downloading the latest updates just for you" << std::endl;
                std::cout << "Citra will launch as soon as the update is finished" << std::endl;
                // download the latest version and get it ready
                startProcessWrapper(updater_exe, "Update.exe --update=https://github.com/jroweboy/lemon/releases/download/"+tag_name, true);
                // and we are done! Next boot it'll point to the new version.
                return 0;
            }
        }
    }
    // Normal Run so just start citra-qt
    startProcessWrapper(updater_exe, "Update.exe --processStart=citra-qt.exe", false);
    return 0;
}
