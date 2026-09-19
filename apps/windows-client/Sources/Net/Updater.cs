using System.Diagnostics;
using System.Net.Http;
using System.Reflection;
using System.Text.Json;

namespace PotuzhneRadio;

/// <summary>
/// Оновлення самої програми з GitHub.
///
/// Раніше програму доводилось завантажувати вручну: радіо оновлювалось саме, а
/// Windows, Mac і Android — ні. Тепер програма дивиться у випуск на GitHub і,
/// якщо там свіжіша за неї, пропонує оновитись і робить це сама.
///
/// Порівнюємо не з номером випуску, а з файлом PotuzhneRadio-clients.json —
/// там записано, якої версії програма справді лежить у цьому випуску. Якщо
/// програму не перезбирали, вона переноситься з минулого разу зі своїм
/// номером, і оновлення дарма не пропонується.
/// </summary>
static class Updater
{
    const string Repo = "roman885-85/potuzhne-radio";
    const string ExeAsset = "PotuzhneRadio-Windows.exe";
    const string ManifestAsset = "PotuzhneRadio-clients.json";

    public sealed record Found(string Version, string Url, string Notes);

    public static string MyVersion =>
        Assembly.GetExecutingAssembly().GetName().Version is { } v
            ? $"{v.Major}.{v.Minor}.{v.Build}"
            : "0";

    /// «1.4.38» новіша за «1.4.9» — порівнюємо числами, не рядками.
    public static bool IsNewer(string a, string b)
    {
        var pa = a.Split('.');
        var pb = b.Split('.');
        for (int i = 0; i < Math.Max(pa.Length, pb.Length); i++)
        {
            int x = i < pa.Length && int.TryParse(pa[i], out var xv) ? xv : 0;
            int y = i < pb.Length && int.TryParse(pb[i], out var yv) ? yv : 0;
            if (x != y) return x > y;
        }
        return false;
    }

    static HttpClient Http()
    {
        var h = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
        h.DefaultRequestHeaders.Add("User-Agent", "potuzhne-radio-win");
        return h;
    }

    /// Що лежить у свіжому випуску. null — нового немає або не достукались.
    public static async Task<Found?> CheckAsync()
    {
        try
        {
            using var http = Http();
            var json = await http.GetStringAsync($"https://api.github.com/repos/{Repo}/releases/latest");
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            string? Asset(string name)
            {
                if (!root.TryGetProperty("assets", out var assets)) return null;
                foreach (var a in assets.EnumerateArray())
                    if (a.TryGetProperty("name", out var n) && n.GetString() == name)
                        return a.TryGetProperty("browser_download_url", out var u) ? u.GetString() : null;
                return null;
            }

            var exe = Asset(ExeAsset);
            if (exe == null) return null;

            var there = root.TryGetProperty("tag_name", out var tag) ? tag.GetString() ?? "" : "";
            if (there.StartsWith('v')) there = there[1..];

            var man = Asset(ManifestAsset);
            if (man != null)
            {
                try
                {
                    using var m = JsonDocument.Parse(await http.GetStringAsync(man));
                    if (m.RootElement.TryGetProperty("windows", out var w) && w.GetString() is { Length: > 0 } wv)
                        there = wv;
                }
                catch (Exception ex) { Log.Write($"Оновлення: маніфест — {ex.Message}"); }
            }

            if (there.Length == 0 || !IsNewer(there, MyVersion)) return null;
            var notes = root.TryGetProperty("body", out var b) ? b.GetString() ?? "" : "";
            return new Found(there, exe, notes);
        }
        catch (Exception ex)
        {
            Log.Write($"Оновлення: перевірка — {ex.Message}");
            return null;
        }
    }

    /// Завантажити, підмінити себе й перезапуститись.
    public static async Task InstallAsync(Found f)
    {
        var self = Environment.ProcessPath
            ?? throw new InvalidOperationException("Невідомо, де лежить сама програма");
        var tmp = Path.Combine(Path.GetTempPath(), "potuzhne-update-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tmp);
        var fresh = Path.Combine(tmp, "PotuzhneRadio.exe");

        using (var http = Http())
        using (var src = await http.GetStreamAsync(f.Url))
        using (var dst = File.Create(fresh))
            await src.CopyToAsync(dst);

        if (new FileInfo(fresh).Length < 1024 * 1024)
            throw new InvalidOperationException("Завантажений файл надто малий — щось пішло не так");

        //  Себе на ходу не підміниш: лишаємо коротку вказівку, яка дочекається
        //  виходу програми, підмінить файл і запустить нову.
        var cmd = Path.Combine(tmp, "swap.cmd");
        await File.WriteAllTextAsync(cmd, $"""
            @echo off
            :wait
            tasklist /fi "PID eq {Environment.ProcessId}" | find "{Environment.ProcessId}" >nul
            if not errorlevel 1 (
              timeout /t 1 /nobreak >nul
              goto wait
            )
            move /y "{fresh}" "{self}" >nul
            start "" "{self}"
            rmdir /s /q "{tmp}"
            """, System.Text.Encoding.Default);

        Process.Start(new ProcessStartInfo("cmd.exe", $"/c \"{cmd}\"")
        {
            CreateNoWindow = true,
            UseShellExecute = false,
        });
        Application.Exit();
    }

    /// Питаємо людину й оновлюємось. silent — мовчати, коли нового немає.
    public static async Task RunAsync(IWin32Window? owner, bool silent)
    {
        var f = await CheckAsync();
        if (f == null)
        {
            if (!silent)
                MessageBox.Show(owner, $"У вас найсвіжіша версія — {MyVersion}.",
                    "Оновлення не потрібне", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        var first = f.Notes.Split('\n').FirstOrDefault()?.Trim() ?? "";
        var text = (first.Length > 0 ? first + "\n\n" : "")
                 + $"Зараз у вас {MyVersion}. Оновити зараз? Програма закриється й відкриється знову.";
        if (MessageBox.Show(owner, text, $"Є нова версія — {f.Version}",
                MessageBoxButtons.OKCancel, MessageBoxIcon.Information) != DialogResult.OK) return;

        try { await InstallAsync(f); }
        catch (Exception ex)
        {
            Log.Write($"Оновлення: встановлення — {ex}");
            MessageBox.Show(owner, ex.Message, "Не вдалося оновити",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }
}
