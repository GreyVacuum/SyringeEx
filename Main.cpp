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
#define MSG_BOX_INFO MB_OK | MB_ICONINFORMATION
        const char* szUsageInfo =
            "-----用法说明-----\n"
            "\n"
            "注入举例:\n"
            "<Syringe> <gamemd.exe> --args= -CD -i=Ares.dll -i=Phobos.dll\n\n"
            "用法说明:\n"
            "[Syringe (注入器名称)]\n"
            "[game (被注入程序的名称.exe)]\n"
            "[--args=\"<命令行参数列表>\"]\n"
            "[-i=<指定注入文件.dll> ...]\n"
            "[-X=<不指定注入文件.dll> ...]\n"
            "[--nodetach (在注入后保持调试器连接状态，而不是自动断开。)]\n"
            "[--nowait (导致注射器在断开连接后立即弹出，而不会等待目标进程结束。)]\n"
            "[--handshakes (DLL加载时的握手验证，而不是跳过DLL的握手验证。)]\n\n"
            "*注:\n"
            "1.注入器名称  *可不包含后缀名。\n"
            "2.被注入程序的名称  *必须有后缀名。\n"
            "3.-i与-X  *均写多的指定的文件。\n";
            
        MessageBoxA(nullptr, szUsageInfo, VersionString, MSG_BOX_INFO);

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
