using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;

namespace PotuzhneRadio;

/// <summary>
/// Тека проєкту — «Проєкт» поруч із програмою (її кладе туди build.sh на Mac)
/// або будь-яка інша з тією самою будовою, скажімо сама «powered radio»:
///   source\yoRadio\yoRadio.ino   — скетч (тека мусить зватися як .ino)
///   build\sketchbook\libraries\  — бібліотеки
///   firmware\VERSION             — номер версії; сюди ж лягають готові образи
/// </summary>
public sealed class Project
{
    public string Root { get; }

    public Project(string root) => Root = Path.GetFullPath(root);

    public string Sketch => Path.Combine(Root, "source", "yoRadio");
    public string Sketchbook => Path.Combine(Root, "build", "sketchbook");
    public string Firmware => Path.Combine(Root, "firmware");
    public string VersionFile => Path.Combine(Firmware, "VERSION");
    public string UpdateBin => Path.Combine(Firmware, "PotuzhneRadio-ES3C28P-update.bin");
    public string WebDir => Path.Combine(Firmware, "web");

    public bool IsValid => IsProject(Root);

    public static bool IsProject(string? dir) =>
        !string.IsNullOrEmpty(dir) &&
        File.Exists(Path.Combine(dir, "source", "yoRadio", "yoRadio.ino")) &&
        Directory.Exists(Path.Combine(dir, "build", "sketchbook"));

    /// «Проєкт» поруч із .exe, якщо він там є.
    public static string? NextToApp()
    {
        var exeDir = Path.GetDirectoryName(Environment.ProcessPath ?? "") ?? "";
        foreach (var name in new[] { "Проєкт", "Project" })
        {
            var p = Path.Combine(exeDir, name);
            if (IsProject(p)) return p;
        }
        return null;
    }

    public string ReadVersion()
    {
        try
        {
            var v = File.ReadAllText(VersionFile).Trim();
            return v.Length > 0 ? v : "1.0";
        }
        catch
        {
            return "1.0";
        }
    }

    public void WriteVersion(string v)
    {
        Directory.CreateDirectory(Firmware);
        File.WriteAllText(VersionFile, v + "\n", new UTF8Encoding(false));
    }

    /// Версія й час збірки — у прошивку (src\extras\yoBuild.h), як у rebuild.sh.
    public void WriteBuildHeader(string version, string build)
    {
        var path = Path.Combine(Sketch, "src", "extras", "yoBuild.h");
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var text = "/* Створює програма збірки під час кожної збірки — вручну не правити. */\n" +
                   $"#define PR_VERSION \"{version}\"\n" +
                   $"#define PR_BUILD   \"{build}\"\n";
        File.WriteAllText(path, text, new UTF8Encoding(false));
    }

    /// <summary>
    /// Свіжі app.js / app.css — стиснуті в data\www, звідки вони підуть в образ
    /// файлової системи. Те саме, що «gzip -9» у rebuild.sh. Повертає, що стиснуто.
    /// </summary>
    public string[] GzipWeb()
    {
        var web = Path.Combine(Sketch, "web");
        if (!File.Exists(Path.Combine(web, "app.js"))) return Array.Empty<string>();
        var www = Path.Combine(Sketch, "data", "www");
        Directory.CreateDirectory(www);
        var done = new System.Collections.Generic.List<string>();
        foreach (var name in new[] { "app.js", "app.css" })
        {
            var src = Path.Combine(web, name);
            if (!File.Exists(src)) continue;
            var dst = Path.Combine(www, name + ".gz");
            using (var input = File.OpenRead(src))
            using (var output = File.Create(dst))
            using (var gz = new GZipStream(output, CompressionLevel.SmallestSize))
                input.CopyTo(gz);
            done.Add($"{name}.gz {Fmt.Size(new FileInfo(dst).Length)}");
        }
        return done.ToArray();
    }
}

/// <summary>
/// Дзеркалювання теки: копіює лише змінені файли (розмір або час), зайві
/// прибирає. Час змін зберігається — інакше arduino-cli щоразу вважав би все
/// новим і збирав би з нуля.
/// </summary>
public static class Mirror
{
    /// Службові файли macOS («._ім'я» з FAT/exFAT-флешки, .DS_Store): «._main.cpp»
    /// компілятор узяв би за код і впав би на ньому.
    static bool Junk(string rel)
    {
        var name = Path.GetFileName(rel);
        return name.StartsWith("._", StringComparison.Ordinal) || name == ".DS_Store" ||
               rel.Split(Path.DirectorySeparatorChar).Any(p => p is ".git" or "__MACOSX");
    }

    public static (int copied, int removed) Sync(string src, string dst)
    {
        Directory.CreateDirectory(dst);
        var want = Directory.EnumerateFiles(src, "*", SearchOption.AllDirectories)
            .Select(f => Path.GetRelativePath(src, f))
            .Where(r => !Junk(r))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        var copied = 0;
        foreach (var rel in want)
        {
            var s = new FileInfo(Path.Combine(src, rel));
            var d = new FileInfo(Path.Combine(dst, rel));
            if (d.Exists && d.Length == s.Length && d.LastWriteTimeUtc == s.LastWriteTimeUtc) continue;
            Directory.CreateDirectory(d.DirectoryName!);
            if (d.Exists) d.IsReadOnly = false;
            File.Copy(s.FullName, d.FullName, overwrite: true);
            File.SetLastWriteTimeUtc(d.FullName, s.LastWriteTimeUtc);
            copied++;
        }

        var removed = 0;
        foreach (var f in Directory.EnumerateFiles(dst, "*", SearchOption.AllDirectories).ToList())
        {
            if (want.Contains(Path.GetRelativePath(dst, f))) continue;
            File.Delete(f);
            removed++;
        }
        foreach (var dir in Directory.EnumerateDirectories(dst, "*", SearchOption.AllDirectories)
                     .OrderByDescending(d => d.Length).ToList())
        {
            if (!Directory.EnumerateFileSystemEntries(dir).Any()) Directory.Delete(dir);
        }
        return (copied, removed);
    }
}
