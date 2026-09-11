using System;
using System.IO;
using System.Text;

namespace PotuzhneRadio;

/// <summary>
/// Де програма тримає свої файли. Усе — в профілі користувача, поруч із .exe
/// нічого не пишеться: файл може лежати там, куди писати не можна, а тека
/// WebView2 поруч із ним лише заважала б.
/// </summary>
public static class AppPaths
{
    /// %APPDATA%\PotuzhneRadio — налаштування й журнал.
    public static string DataDir =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "PotuzhneRadio");

    /// %LOCALAPPDATA%\PotuzhneRadio\WebView2 — кеш і профіль вбудованого браузера.
    /// Він великий і прив'язаний до цього комп'ютера, тому не в Roaming.
    public static string WebViewDir =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "PotuzhneRadio", "WebView2");
}

/// <summary>
/// Журнал роботи: %APPDATA%\PotuzhneRadio\log.txt. Туди пишеться, які мережі
/// перевірялись, хто відповів на mDNS, до чого підключились і чому відпали.
/// Коли «радіо не знаходиться», відповідь шукати саме тут.
/// </summary>
public static class Log
{
    static readonly object Gate = new();

    /// Для перевірки на Mac: писати в консоль, а не у файл.
    public static bool ToConsole;

    /// Понад цей розмір журнал переїжджає в log.old.txt і починається заново.
    const long MaxBytes = 512 * 1024;

    /// Ім'я файлу журналу: у клієнта log.txt, у програми збірки — свій.
    public static string FileName = "log.txt";

    public static string FilePath => Path.Combine(AppPaths.DataDir, FileName);

    public static void Write(string message)
    {
        var line = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff}  {message}";
        lock (Gate)
        {
            if (ToConsole)
            {
                Console.WriteLine(line);
                return;
            }
            try
            {
                Directory.CreateDirectory(AppPaths.DataDir);
                var path = FilePath;
                var info = new FileInfo(path);
                if (info.Exists && info.Length > MaxBytes)
                {
                    File.Move(path, Path.ChangeExtension(path, ".old.txt"), overwrite: true);
                    info.Refresh();
                }
                // BOM на початку: без нього Блокнот і PowerShell 5.1 читають
                // українську як кашу.
                var fresh = !info.Exists;
                using var w = new StreamWriter(path, append: true, new UTF8Encoding(encoderShouldEmitUTF8Identifier: fresh));
                w.WriteLine(line);
            }
            catch
            {
                // Журнал — допоміжна річ: якщо писати нікуди, програма працює далі.
            }
        }
    }
}
