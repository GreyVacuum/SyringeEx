#include "Log.h"
#include "SyringeDebugger.h"
#include "Support.h"
#include "resource.h"

#include <string>

#include <commctrl.h>
#include <shellapi.h>

std::vector<std::string> GetArguments()
{
    // Get argc, argv in wide chars
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);

    // Convert to UTF-8. Skip the first argument as it contains the path to Syringe itself
    std::vector<std::string> argv(argc - 1);
    for (int i = 1; i < argc; ++i)
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, nullptr, 0, nullptr, nullptr);
        argv[i - 1].resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argv[i - 1].data(), len, nullptr, nullptr);
    }

    LocalFree(argvW);

    return argv;
}

int Run(const std::vector<std::string>& arguments)
{
    constexpr auto const VersionString = "注入器 (扩展)";

    InitCommonControls();

    Log::Open("syringe.log");

    Log::WriteLine(VersionString);
    Log::WriteLine("===============");
    Log::WriteLine();
    Log::WriteLine("WinMain: arguments = \"%.*s\"", printable(arguments));

    auto failure = "Could not load executable.";
    auto exit_code = ERROR_ERRORS_ENCOUNTERED;

    try
    {
        auto const command = parse_command_line(arguments);

        Log::WriteLine(
            "WinMain: Trying to load executable file \"%.*s\"...",
            printable(command.executable_name));
        Log::WriteLine();

        SyringeDebugger Debugger{ command.executable_name, command.syringe_arguments };
        failure = "Could not run executable.";

        Log::WriteLine("WinMain: SyringeDebugger::FindDLLs();");
        Log::WriteLine();
        Debugger.FindDLLs();

        Log::WriteLine(
            "WinMain: SyringeDebugger::Run(\"%.*s\");",
            printable(command.game_arguments));
        Log::WriteLine();

        Debugger.Run(command.game_arguments);
        Log::WriteLine("WinMain: SyringeDebugger::Run finished.");
        Log::WriteLine("WinMain: Exiting on success.");
        return ERROR_SUCCESS;
    }
    catch (lasterror const& e)
    {
        auto const message = replace(e.message, "%1", e.insert);
        Log::WriteLine("WinMain: %s (%d)", message.c_str(), e.error);

        auto const msg = std::string(failure) + "\n\n" + message;
        MessageBoxA(nullptr, msg.c_str(), VersionString, MB_OK | MB_ICONERROR);

        exit_code = static_cast<long>(e.error);
    }
    catch (invalid_command_arguments const&)
    {
        // offer to open the releases page (link button). If user chooses YES,
        // open the link and finish immediately without showing the usage box.
        if (MessageBoxA(nullptr,
            "是否打开发布页面以获取支持或更新？",
            VersionString,
            MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            ShellExecuteA(nullptr, "open",
                "https://github.com/GreyVacuum/SyringeEx/tags",
                nullptr, nullptr, SW_SHOWNORMAL);

            Log::WriteLine("WinMain: User chose to open releases page, exiting.");
            return ERROR_INVALID_PARAMETER; // finish immediately
        }

#define MSG_BOX_INFO MB_OK | MB_ICONINFORMATION
        const char* szUsageInfo =
            "SyringeEX 魔改版\n"
            "更新日期：2026/02/05  \n"
            "当前版本： " SYRINGEEX_VER_TEXT " \n"
            "\n"
            "-----用法说明-----\n"
            "\n"
            "注入举例:\n"
            "Syringe \"game.exe\"%* --args= -CD -? ... -i=?.dll -i=?.dll -pathlnject=Files --norootlnject --nodetach --nowait --handshakes\n"
            "\n"
            "用法说明:\n"
            "[Syringe (注入器名称)]\n"
            "[game (被注入程序的名称.exe)]\n"
            "\n"
            "[--args=\"<命令行参数列表>\"]\n"
            "[-i=<指定注入文件.dll> ...  (全部dll不在执行，并以你设置的指定读取。)]\n"
            "[-x=<不指定注入文件.dll> ...  (全部dll会排除你指定的dll不读取。)]\n"
            "[-pathlnject=<指定路径文件夹> ...  (根据你的路径进行读取，同时不影响目录根的读取。)]\n"
            "\n"
            "-提醒- >  -i与-x以及-pathlnject(可写更多个这样的参数)\n"
            "\n"
            "[--norootlnject(禁用目录根读取dll，而不是一直允许读取。)]\n"
            "[--nodetach (在注入后保持调试器连接状态，而不是自动断开。)]\n"
            "[--nowait (导致注射器在断开连接后立即弹出，而不会等待目标进程结束。)]\n"
            "[--handshakes (DLL加载时的握手验证，而不是跳过DLL的握手验证。)]\n"
            "\n";
            
        // ask whether to generate the usage file; if yes, show usage and create the file
        if (MessageBoxA(nullptr,
            "是否生成并显示使用说明？(将创建 'Syringe 使用说明.txt')",
            VersionString,
            MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            MessageBoxA(nullptr, szUsageInfo, VersionString, MSG_BOX_INFO);

            // create a text file next to the executable: "Syringe 使用说明.txt"
            {
                WCHAR exePath[MAX_PATH] = {0};
                if (GetModuleFileNameW(nullptr, exePath, MAX_PATH))
                {
                    std::wstring path(exePath);
                    auto pos = path.find_last_of(L"\\/");
                    std::wstring dir = (pos == std::wstring::npos) ? L"." : path.substr(0, pos);
                    std::wstring outFile = dir + L"\\Syringe 使用说明.txt";

                    HANDLE hFile = CreateFileW(outFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hFile != INVALID_HANDLE_VALUE)
                    {
                        DWORD written = 0;

                        // Produce an ANSI-encoded file (system code page). Try to interpret
                        // the source literal as UTF-8 first, then fall back to CP_ACP.
                        std::wstring wbuf;
                        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, szUsageInfo, -1, nullptr, 0);
                        if (wlen > 0)
                        {
                            wbuf.assign(static_cast<size_t>(wlen - 1), L'\0');
                            MultiByteToWideChar(CP_UTF8, 0, szUsageInfo, -1, &wbuf[0], wlen);
                        }
                        else
                        {
                            wlen = MultiByteToWideChar(CP_ACP, 0, szUsageInfo, -1, nullptr, 0);
                            if (wlen > 0)
                            {
                                wbuf.assign(static_cast<size_t>(wlen - 1), L'\0');
                                MultiByteToWideChar(CP_ACP, 0, szUsageInfo, -1, &wbuf[0], wlen);
                            }
                        }

                        if (!wbuf.empty())
                        {
                            int alen = WideCharToMultiByte(CP_ACP, 0, wbuf.c_str(), -1, nullptr, 0, nullptr, nullptr);
                            if (alen > 0)
                            {
                                std::string abuf(static_cast<size_t>(alen - 1), '\0');
                                WideCharToMultiByte(CP_ACP, 0, wbuf.c_str(), -1, &abuf[0], alen, nullptr, nullptr);
                                if (!abuf.empty())
                                {
                                    WriteFile(hFile, abuf.c_str(), static_cast<DWORD>(abuf.size()), &written, nullptr);
                                }
                            }
                        }
                        else
                        {
                            // fallback: write raw bytes
                            DWORD len = static_cast<DWORD>(strlen(szUsageInfo));
                            if (len)
                                WriteFile(hFile, szUsageInfo, len, &written, nullptr);
                        }

                        CloseHandle(hFile);
                    }
                }
            }
        }

        Log::WriteLine(
            "WinMain: Invalid command line arguments given, exiting...");

        exit_code = ERROR_INVALID_PARAMETER;
    }

    Log::WriteLine("WinMain: Exiting on failure.");
    return static_cast<int>(exit_code);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    return Run(GetArguments());
}
