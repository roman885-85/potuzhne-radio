using System;
using System.IO;
using System.Text;
using System.Text.Json;

namespace PotuzhneRadio;

/// <summary>
/// Налаштування: %APPDATA%\PotuzhneRadio\settings.json. Головне тут — адреса
/// останнього радіо: при старті її перевіряємо першою і, якщо радіо на місці,
/// підключаємось одразу, без пошуку.
/// </summary>
public sealed class Settings
{
    /// Адреса, за якою підключались востаннє (IP або IP:порт).
    public string? LastAddress { get; set; }
    /// Його ім'я в мережі — щоб серед кількох знайдених виділити саме його.
    public string? LastHost { get; set; }
    public string? LastStation { get; set; }

    // Вікно — щоб відкривалось там і такого розміру, як його лишили.
    public int WindowX { get; set; }
    public int WindowY { get; set; }
    public int WindowWidth { get; set; }
    public int WindowHeight { get; set; }
    public bool WindowMaximized { get; set; }

    static string FilePath => Path.Combine(AppPaths.DataDir, "settings.json");

    // Кирилиця — як є, а не Св…: файл має читатися людиною.
    static readonly JsonSerializerOptions Json = new()
    {
        WriteIndented = true,
        Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    public static Settings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<Settings>(File.ReadAllText(FilePath, Encoding.UTF8), Json) ?? new Settings();
        }
        catch (Exception ex)
        {
            Log.Write($"settings.json не прочитано, беру типові: {ex.Message}");
        }
        return new Settings();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(AppPaths.DataDir);
            // Спершу в тимчасовий файл, потім заміна: обірване записування
            // не має лишити порожній settings.json.
            var tmp = FilePath + ".tmp";
            File.WriteAllText(tmp, JsonSerializer.Serialize(this, Json), new UTF8Encoding(false));
            // Файл, скопійований з флешки чи мережі, буває «лише для читання» —
            // тоді заміна падала б з «доступ заборонено».
            if (File.Exists(FilePath)) File.SetAttributes(FilePath, FileAttributes.Normal);
            File.Move(tmp, FilePath, overwrite: true);
        }
        catch (Exception ex)
        {
            Log.Write($"settings.json не збережено: {ex.Message}");
        }
    }
}
