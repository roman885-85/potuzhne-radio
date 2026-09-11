using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace PotuzhneRadio;

/// <summary>
/// Інструменти збірки: arduino-cli і ядро ESP32 для нього. Живуть у
/// %LOCALAPPDATA%\PotuzhneRadio — ставляться один раз (близько гігабайта
/// завантаження) і далі служать усім збіркам.
///
/// Якщо в імені користувача є не-латинські літери, беремо C:\ProgramData:
/// компілятор ESP32 для Windows (gcc) працює з шляхами через кодову сторінку
/// ANSI і на кирилиці поза нею просто не знаходить файлів.
/// </summary>
public static class Tools
{
    public const string CliVersion = "1.5.1";
    public const string CoreVersion = "3.3.3";
    public const string Core = "esp32:esp32@" + CoreVersion;
    public const string IndexUrl = "https://espressif.github.io/arduino-esp32/package_esp32_index.json";
    public static string CliZipUrl => $"https://downloads.arduino.cc/arduino-cli/arduino-cli_{CliVersion}_Windows_64bit.zip";

    public const string Fqbn = "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,FlashMode=qio," +
                               "PSRAM=opi,CPUFreq=240,PartitionScheme=huge_app,LoopCore=1,EventsCore=1,DebugLevel=none";

    public static string Root { get; } = PickRoot();

    static string PickRoot()
    {
        var local = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "PotuzhneRadio");
        if (local.All(c => c < 128)) return local;
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "PotuzhneRadio");
    }

    public static string Cli => Path.Combine(Root, "arduino-cli.exe");
    public static string Config => Path.Combine(Root, "arduino-cli.yaml");
    public static string Data => Path.Combine(Root, "Arduino15");
    public static string Staging => Path.Combine(Data, "staging");
    /// Постійна тека проміжних файлів: з нею друга й наступні збірки йдуть хвилини, а не чверть години.
    public static string BuildPath => Path.Combine(Root, "bp");
    public static string OutDir => Path.Combine(Root, "out");
    /// Робоча копія скетчу й бібліотек: шлях латиницею, без пробілів і дужок
    /// (тека «Проєкт» лежить у «Програми\Збірка ПОТУЖНОГО РАДІО (Windows)»).
    public static string Work => Path.Combine(Root, "work");
    public static string WorkSketch => Path.Combine(Work, "yoRadio");
    public static string WorkSketchbook => Path.Combine(Work, "sketchbook");

    public static string Esp32Hardware => Path.Combine(Data, "packages", "esp32", "hardware", "esp32", CoreVersion);
    public static string BootApp0 => Path.Combine(Esp32Hardware, "tools", "partitions", "boot_app0.bin");

    public static bool CliReady => File.Exists(Cli);
    public static bool CoreReady => Directory.Exists(Esp32Hardware) && File.Exists(BootApp0) &&
                                    Mkspiffs != null && Esptool != null;

    public static string? Mkspiffs => FindTool("mkspiffs", "mkspiffs.exe");
    public static string? Esptool => FindTool("esptool_py", "esptool.exe");

    /// packages\esp32\tools\&lt;name&gt;\&lt;версія&gt;\&lt;exe&gt; — найновіша версія.
    static string? FindTool(string name, string exe)
    {
        var dir = Path.Combine(Data, "packages", "esp32", "tools", name);
        if (!Directory.Exists(dir)) return null;
        return Directory.EnumerateDirectories(dir)
            .OrderByDescending(d => VersionKey(Path.GetFileName(d)))
            .Select(d => Path.Combine(d, exe))
            .FirstOrDefault(File.Exists);
    }

    static string VersionKey(string v) =>
        string.Join('.', v.Split('.', '-', '+').Select(p => int.TryParse(p, out var n) ? n.ToString("D8") : p));

    /// <summary>
    /// arduino-cli.yaml: дані (ядро ESP32) — у Arduino15 поруч, бібліотеки —
    /// з робочої копії теки проєкту, глобальні бібліотеки Arduino не чіпаються.
    /// </summary>
    public static void WriteConfig()
    {
        Directory.CreateDirectory(Root);
        static string Q(string s) => "'" + s.Replace("'", "''") + "'";
        var yaml =
            "# Створює «Збірка ПОТУЖНОГО РАДІО». Вручну не правити — перепишеться.\n" +
            "board_manager:\n" +
            "  additional_urls:\n" +
            $"    - {IndexUrl}\n" +
            "directories:\n" +
            $"  data: {Q(Data)}\n" +
            $"  downloads: {Q(Staging)}\n" +
            $"  user: {Q(WorkSketchbook)}\n" +
            "library:\n" +
            "  enable_unsafe_install: true\n" +
            "logging:\n" +
            "  level: warn\n" +
            "network:\n" +
            "  connection_timeout: 300s\n";
        File.WriteAllText(Config, yaml, new UTF8Encoding(false));
    }

    /// Вільне місце на диску з інструментами, байт.
    public static long FreeSpace()
    {
        try { return new DriveInfo(Path.GetPathRoot(Root)!).AvailableFreeSpace; }
        catch { return long.MaxValue; }
    }

    /// Розмір теки (для «завантажено N МБ» під час установки ядра).
    public static long DirSize(string dir)
    {
        try
        {
            return Directory.Exists(dir)
                ? Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories).Sum(f => { try { return new FileInfo(f).Length; } catch { return 0L; } })
                : 0;
        }
        catch
        {
            return 0;
        }
    }
}

/// <summary>
/// Запуск зовнішньої програми з рядками виводу в журнал і зупинкою всього
/// дерева процесів (arduino-cli запускає gcc десятками).
/// </summary>
public sealed class Runner
{
    Process? _proc;
    readonly object _gate = new();

    public event Action<string>? Line;

    public void Emit(string s) => Line?.Invoke(s);

    public async Task RunAsync(string exe, IEnumerable<string> args, string? workDir, CancellationToken ct)
    {
        var psi = new ProcessStartInfo(exe)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
            WorkingDirectory = workDir ?? Tools.Root,
        };
        foreach (var a in args) psi.ArgumentList.Add(a);

        var p = new Process { StartInfo = psi, EnableRaisingEvents = true };
        p.OutputDataReceived += (_, e) => { if (!string.IsNullOrWhiteSpace(e.Data)) Emit(e.Data); };
        p.ErrorDataReceived += (_, e) => { if (!string.IsNullOrWhiteSpace(e.Data)) Emit(e.Data); };
        Log.Write($"Запуск: {exe} {string.Join(' ', psi.ArgumentList.Select(x => x.Contains(' ') ? $"\"{x}\"" : x))}");
        if (!p.Start()) throw new InvalidOperationException($"не вдалося запустити {Path.GetFileName(exe)}");
        lock (_gate) _proc = p;
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        try
        {
            using var reg = ct.Register(() => Kill());
            await p.WaitForExitAsync(CancellationToken.None).ConfigureAwait(false);
            p.WaitForExit(); // дочитати вивід до кінця
        }
        finally
        {
            lock (_gate) _proc = null;
        }
        ct.ThrowIfCancellationRequested();
        if (p.ExitCode != 0)
            throw new ToolFailedException(Path.GetFileNameWithoutExtension(exe), p.ExitCode);
    }

    public void Kill()
    {
        lock (_gate)
        {
            try { if (_proc is { HasExited: false }) _proc.Kill(entireProcessTree: true); }
            catch (Exception ex) { Log.Write($"Зупинка процесу: {ex.Message}"); }
        }
    }
}

public sealed class ToolFailedException(string tool, int code) : Exception($"{tool} завершився з кодом {code}")
{
    public string Tool { get; } = tool;
    public int Code { get; } = code;
}

/// <summary>Установка інструментів — повторює те, що на Mac зроблено раз руками.</summary>
public sealed class ToolsInstaller
{
    readonly Runner _run;

    /// (фаза, частка 0…1 або null — невідомо, подробиці)
    public event Action<string, double?, string>? Progress;

    public ToolsInstaller(Runner run) => _run = run;

    void Report(string phase, double? frac, string detail = "") => Progress?.Invoke(phase, frac, detail);

    public async Task InstallAsync(CancellationToken ct)
    {
        Directory.CreateDirectory(Tools.Root);
        var free = Tools.FreeSpace();
        _run.Emit($">>> інструменти в {Tools.Root}; вільно на диску {Fmt.Size(free)}");
        if (free < 6L * 1024 * 1024 * 1024)
            _run.Emit("Увага: на диску менше 6 ГБ — ядру ESP32 після розпакування потрібно 4–5 ГБ.");

        if (!Tools.CliReady)
            await DownloadCliAsync(ct);
        Tools.WriteConfig();

        Report("Оновлюю перелік плат", null);
        _run.Emit(">>> перелік плат ESP32");
        await _run.RunAsync(Tools.Cli, new[] { "--config-file", Tools.Config, "--no-color", "core", "update-index" }, null, ct);

        Report("Завантажую ядро ESP32 (близько 1 ГБ)", 0);
        _run.Emit($">>> ядро {Tools.Core} — це найдовше, один раз");
        var baseline = Tools.DirSize(Tools.Staging);
        using var poll = CancellationTokenSource.CreateLinkedTokenSource(ct);
        var watcher = Task.Run(async () =>
        {
            // arduino-cli без терміналу мовчить, поки качає, — тож прогрес
            // рахуємо самі: скільки вже лягло в теку завантажень.
            const double expected = 1.0 * 1024 * 1024 * 1024;
            while (!poll.IsCancellationRequested)
            {
                try { await Task.Delay(1500, poll.Token); } catch { break; }
                var got = Math.Max(0, Tools.DirSize(Tools.Staging) - baseline);
                Report("Завантажую ядро ESP32 (близько 1 ГБ)", Math.Min(0.99, got / expected),
                    $"завантажено {Fmt.Size(got)}");
            }
        });
        try
        {
            await _run.RunAsync(Tools.Cli, new[] { "--config-file", Tools.Config, "--no-color", "core", "install", Tools.Core }, null, ct);
        }
        finally
        {
            poll.Cancel();
            await watcher;
        }
        if (!Tools.CoreReady)
            throw new InvalidOperationException("ядро встановилось не повністю: немає mkspiffs, esptool або boot_app0.bin");
        _run.Emit(">>> інструменти готові");
        Report("Інструменти готові", 1);
    }

    async Task DownloadCliAsync(CancellationToken ct)
    {
        Report("Завантажую arduino-cli", 0);
        _run.Emit($">>> arduino-cli {Tools.CliVersion}: {Tools.CliZipUrl}");
        var zip = Path.Combine(Tools.Root, "arduino-cli.zip");
        using (var http = new HttpClient { Timeout = TimeSpan.FromMinutes(10) })
        {
            http.DefaultRequestHeaders.UserAgent.ParseAdd("PotuzhneRadio-Builder/1.0");
            using var resp = await http.GetAsync(Tools.CliZipUrl, HttpCompletionOption.ResponseHeadersRead, ct);
            resp.EnsureSuccessStatusCode();
            var total = resp.Content.Headers.ContentLength ?? 0;
            await using var src = await resp.Content.ReadAsStreamAsync(ct);
            await using var dst = File.Create(zip);
            var buf = new byte[81920];
            long got = 0;
            int n;
            var last = DateTime.MinValue;
            while ((n = await src.ReadAsync(buf, ct)) > 0)
            {
                await dst.WriteAsync(buf.AsMemory(0, n), ct);
                got += n;
                if ((DateTime.UtcNow - last).TotalMilliseconds > 250)
                {
                    last = DateTime.UtcNow;
                    Report("Завантажую arduino-cli", total > 0 ? (double)got / total : null, $"{Fmt.Size(got)} з {Fmt.Size(total)}");
                }
            }
        }
        using (var archive = ZipFile.OpenRead(zip))
        {
            var entry = archive.Entries.FirstOrDefault(e => e.Name.Equals("arduino-cli.exe", StringComparison.OrdinalIgnoreCase))
                        ?? throw new InvalidOperationException("в архіві немає arduino-cli.exe");
            entry.ExtractToFile(Tools.Cli, overwrite: true);
        }
        File.Delete(zip);
        _run.Emit($"arduino-cli.exe: {Fmt.Size(new FileInfo(Tools.Cli).Length)}");
    }
}
