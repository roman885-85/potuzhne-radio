using System;
using System.IO;
using System.Text;
using System.Text.Json;

namespace PotuzhneRadio;

/// <summary>Налаштування програми збірки: %APPDATA%\PotuzhneRadio\builder.json.</summary>
public sealed class BuilderSettings
{
    /// Тека проєкту, якщо «Проєкт» поруч із програмою не знайшовся.
    public string? ProjectPath { get; set; }
    /// Остання адреса, введена вручну.
    public string? LastAddress { get; set; }
    public int WindowX { get; set; }
    public int WindowY { get; set; }
    public int WindowWidth { get; set; }
    public int WindowHeight { get; set; }

    static string FilePath => Path.Combine(AppPaths.DataDir, "builder.json");

    static readonly JsonSerializerOptions Json = new()
    {
        WriteIndented = true,
        Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    public static BuilderSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<BuilderSettings>(File.ReadAllText(FilePath, Encoding.UTF8), Json) ?? new();
        }
        catch (Exception ex)
        {
            Log.Write($"builder.json не прочитано: {ex.Message}");
        }
        return new();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(AppPaths.DataDir);
            var tmp = FilePath + ".tmp";
            File.WriteAllText(tmp, JsonSerializer.Serialize(this, Json), new UTF8Encoding(false));
            // Файл, скопійований з флешки чи мережі, буває «лише для читання» —
            // тоді заміна падала б з «доступ заборонено».
            if (File.Exists(FilePath)) File.SetAttributes(FilePath, FileAttributes.Normal);
            File.Move(tmp, FilePath, overwrite: true);
        }
        catch (Exception ex)
        {
            Log.Write($"builder.json не збережено: {ex.Message}");
        }
    }
}
