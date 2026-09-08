/* X Coin desktop launchers.
 *
 * Build twice:
 *   Linux wallet:   gcc -O2 -o "X Coin Wallet" launcher.c
 *   Linux practice: gcc -O2 -DPRACTICE -o "X Coin Practice Wallet" launcher.c
 *   Windows wallet:   x86_64-w64-mingw32-gcc -O2 -mwindows -o "X Coin Wallet.exe" launcher.c
 *   Windows practice: x86_64-w64-mingw32-gcc -O2 -mwindows -DPRACTICE -o "X Coin Practice Wallet.exe" launcher.c
 *
 * Practice always passes -regtest so it never writes the main ledger
 * or the main wallet.dat. Default practice datadir is ~/.xcoin/regtest
 * (Windows: %APPDATA%\\XCoin\\regtest).
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#else
#define _GNU_SOURCE
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef PRACTICE
#define EXTRA_REGTEST 1
#else
#define EXTRA_REGTEST 0
#endif

#ifdef _WIN32

static int file_exists(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void die(const char *msg)
{
    MessageBoxA(NULL, msg, "X Coin", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

static void join2(char *out, size_t n, const char *a, const char *b)
{
    snprintf(out, n, "%s\\%s", a, b);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow)
{
    (void)hInstance;
    (void)hPrev;
    (void)nShow;

    char self[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, self, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        die("Could not locate this launcher.");

    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s", self);
    char *slash = strrchr(dir, '\\');
    if (!slash)
        die("Could not locate the X Coin folder.");
    *slash = 0;

    char qt[MAX_PATH];
    join2(qt, sizeof(qt), dir, "xcoin-qt.exe");
    if (!file_exists(qt)) {
        join2(qt, sizeof(qt), dir, "bin\\xcoin-qt.exe");
        if (!file_exists(qt))
            die("Could not find xcoin-qt.exe. Put this launcher in the X Coin folder.");
    }

    char plugins[MAX_PATH];
    join2(plugins, sizeof(plugins), dir, "plugins");
    if (GetFileAttributesA(plugins) == INVALID_FILE_ATTRIBUTES)
        join2(plugins, sizeof(plugins), dir, "bin\\plugins");
    SetEnvironmentVariableA("QT_PLUGIN_PATH", plugins);

    char cmdline[32768];
    if (EXTRA_REGTEST) {
        if (lpCmdLine && lpCmdLine[0])
            snprintf(cmdline, sizeof(cmdline), "\"%s\" -regtest %s", qt, lpCmdLine);
        else
            snprintf(cmdline, sizeof(cmdline), "\"%s\" -regtest", qt);
    } else {
        if (lpCmdLine && lpCmdLine[0])
            snprintf(cmdline, sizeof(cmdline), "\"%s\" %s", qt, lpCmdLine);
        else
            snprintf(cmdline, sizeof(cmdline), "\"%s\"", qt);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(qt, cmdline, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        die("Could not start xcoin-qt.exe.");
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

#else /* Unix */

static int exists_exec(const char *path)
{
    return access(path, X_OK) == 0;
}

static void prepend_env(const char *key, const char *value)
{
    const char *old = getenv(key);
    if (!old || !old[0]) {
        setenv(key, value, 1);
        return;
    }
    size_t n = strlen(value) + 1 + strlen(old) + 1;
    char *buf = malloc(n);
    if (!buf)
        return;
    snprintf(buf, n, "%s:%s", value, old);
    setenv(key, buf, 1);
    free(buf);
}

int main(int argc, char **argv)
{
    char self[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n < 0) {
        perror("readlink");
        return 1;
    }
    self[n] = 0;

    char dirbuf[PATH_MAX];
    snprintf(dirbuf, sizeof(dirbuf), "%s", self);
    char *dir = dirname(dirbuf);

    char qt[PATH_MAX];
    snprintf(qt, sizeof(qt), "%s/bin/xcoin-qt", dir);
    if (!exists_exec(qt)) {
        snprintf(qt, sizeof(qt), "%s/xcoin-qt", dir);
        if (!exists_exec(qt)) {
            fprintf(stderr, "Could not find xcoin-qt next to this launcher.\n");
            return 1;
        }
    }

    char libdir[PATH_MAX], plugindir[PATH_MAX], platforms[PATH_MAX];
    snprintf(libdir, sizeof(libdir), "%s/lib", dir);
    snprintf(plugindir, sizeof(plugindir), "%s/plugins", dir);
    if (access(plugindir, F_OK) != 0)
        snprintf(plugindir, sizeof(plugindir), "%s/bin/plugins", dir);
    snprintf(platforms, sizeof(platforms), "%s/platforms", plugindir);

    if (access(libdir, F_OK) == 0)
        prepend_env("LD_LIBRARY_PATH", libdir);
    if (access(plugindir, F_OK) == 0)
        setenv("QT_PLUGIN_PATH", plugindir, 0);
    if (access(platforms, F_OK) == 0)
        setenv("QT_QPA_PLATFORM_PLUGIN_PATH", platforms, 0);

    int extra = EXTRA_REGTEST ? 1 : 0;
    char **newargv = calloc((size_t)argc + extra + 1, sizeof(char *));
    if (!newargv)
        return 1;
    newargv[0] = qt;
    int i = 1;
    if (EXTRA_REGTEST)
        newargv[i++] = "-regtest";
    for (int a = 1; a < argc; a++)
        newargv[i++] = argv[a];
    execv(qt, newargv);
    fprintf(stderr, "execv %s: %s\n", qt, strerror(errno));
    return 1;
}

#endif
