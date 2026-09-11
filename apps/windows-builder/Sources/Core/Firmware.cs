using System;
using System.Globalization;
using System.IO;
using System.Text;

namespace PotuzhneRadio;

/// <summary>
/// Що записано в прошивці: відмітка «POTUZHNE-RADIO-FW|плата|версія|збірка|».
/// Її ж читає сторінка радіо в розділі «Оновлення».
/// </summary>
public sealed record FwInfo(string Board, string Version, string Build, long Size, string Path)
{
    public const string Mark = "POTUZHNE-RADIO-FW|";

    public static FwInfo? Read(string path)
    {
        try
        {
            if (!File.Exists(path)) return null;
            var data = File.ReadAllBytes(path);
            var i = data.AsSpan().IndexOf(Encoding.ASCII.GetBytes(Mark));
            if (i < 0) return null;
            var text = Encoding.UTF8.GetString(data, i, Math.Min(80, data.Length - i));
            var parts = text.Split('|');
            if (parts.Length < 5) return null; // «…|збірка|» — після збірки ще одна риска
            return new FwInfo(parts[1], parts[2], parts[3], data.LongLength, path);
        }
        catch
        {
            return null;
        }
    }

    public string Summary => $"ПОТУЖНЕ РАДІО {Version} · зібрано {Build} · {Fmt.Size(Size)}";
}

public static class Fmt
{
    /// 1 234 567 → «1,2 МБ» — як у Mac-версії.
    public static string Size(long b)
    {
        if (b < 1024) return $"{b} Б";
        if (b < 1_048_576) return $"{b / 1024} КБ";
        return (b / 1_048_576.0).ToString("0.0", CultureInfo.InvariantCulture).Replace('.', ',') + " МБ";
    }

    public static string Elapsed(TimeSpan t) => $"{(int)t.TotalMinutes}:{t.Seconds:00}";

    public const string BuildFormat = "dd.MM.yyyy HH:mm";

    public static DateTime? ParseBuild(string s) =>
        DateTime.TryParseExact(s, BuildFormat, CultureInfo.InvariantCulture, DateTimeStyles.None, out var d) ? d : null;

    /// Порівняння збірки у файлі зі збіркою на радіо — текст для списку.
    public static string Compare(FwInfo? fw, string radioBuild)
    {
        if (fw == null) return "";
        var a = ParseBuild(fw.Build);
        var b = ParseBuild(radioBuild);
        if (a == null || b == null) return "";
        if (a > b) return "у файлі новіша";
        if (a < b) return "у файлі старіша — радіо повернеться до неї";
        return "та сама збірка";
    }

    /// 1.0 → 1.1, 1.9 → 1.10, 2 → 2.1 — як кнопка «Наступна» в Mac-версії.
    public static string NextVersion(string v)
    {
        var p = new System.Collections.Generic.List<string>(v.Trim().Split('.'));
        if (p.Count < 2) p.Add("0");
        if (int.TryParse(p[^1], out var last)) p[^1] = (last + 1).ToString(CultureInfo.InvariantCulture);
        return string.Join('.', p);
    }

    public static bool VersionValid(string v) =>
        System.Text.RegularExpressions.Regex.IsMatch(v ?? "", @"^\d{1,3}(\.\d{1,3}){0,2}$");
}
