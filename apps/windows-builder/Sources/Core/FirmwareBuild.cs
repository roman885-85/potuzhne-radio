using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace PotuzhneRadio;

/// <summary>
/// Збірка прошивки — крок у крок як firmware/rebuild.sh на Mac:
///   1. VERSION і src\extras\yoBuild.h (версія + час збірки);
///   2. web\app.js / app.css → data\www\*.gz;
///   3. arduino-cli compile;
///   4. mkspiffs — образ файлової системи;
///   5. esptool merge-bin — повний образ;
///   6. готові файли — у firmware\ проєкту під іменами ПОТУЖНОГО РАДІО;
///   7. перевірка відмітки POTUZHNE-RADIO-FW|… у прошивці.
/// Відмінність одна: компілюємо з робочої копії в теці інструментів (шлях
/// латиницею), бо gcc для Windows не дружить із кирилицею в шляхах.
/// </summary>
public sealed class FirmwareBuild
{
    readonly Runner _run;
    readonly Project _project;

    /// Нова фаза для рядка стану.
    public event Action<string>? Phase;

    public FirmwareBuild(Runner run, Project project)
    {
        _run = run;
        _project = project;
    }

    void Step(string phase, string log)
    {
        Phase?.Invoke(phase);
        _run.Emit(">>> " + log);
    }

    public async Task<FwInfo> RunAsync(string version, CancellationToken ct)
    {
        if (!Tools.CoreReady) throw new InvalidOperationException("інструменти ще не встановлено");

        var build = DateTime.Now.ToString(Fmt.BuildFormat, CultureInfo.InvariantCulture);
        Step($"Версія {version}: готую збірку", $"версія {version}, збірка {build}");
        _project.WriteVersion(version);
        _project.WriteBuildHeader(version, build);
        foreach (var gz in _project.GzipWeb()) _run.Emit($"сторінка: {gz}");

        _run.Emit(">>> робоча копія проєкту");
        var (c1, r1) = await Task.Run(() => Mirror.Sync(_project.Sketch, Tools.WorkSketch), ct);
        var (c2, r2) = await Task.Run(() => Mirror.Sync(Path.Combine(_project.Sketchbook, "libraries"),
                                                       Path.Combine(Tools.WorkSketchbook, "libraries")), ct);
        _run.Emit($"скетч: оновлено {c1}, прибрано {r1}; бібліотеки: оновлено {c2}, прибрано {r2}");
        Tools.WriteConfig();

        // Старі образи — геть: інакше після невдалої компіляції далі пішли б учорашні.
        Directory.CreateDirectory(Tools.OutDir);
        foreach (var f in Directory.EnumerateFiles(Tools.OutDir)) File.Delete(f);

        Step("Компілюю прошивку", "компіляція (перший раз — 10–15 хвилин, далі швидше)");
        await _run.RunAsync(Tools.Cli, new[]
        {
            "--config-file", Tools.Config, "--no-color", "compile",
            "--fqbn", Tools.Fqbn,
            "--build-path", Tools.BuildPath,
            "--output-dir", Tools.OutDir,
            Tools.WorkSketch,
        }, null, ct);

        string Out(string name) => Path.Combine(Tools.OutDir, name);
        foreach (var need in new[] { "yoRadio.ino.bin", "yoRadio.ino.bootloader.bin", "yoRadio.ino.partitions.bin" })
            if (!File.Exists(Out(need))) throw new InvalidOperationException($"після компіляції немає {need}");

        Step("Складаю образ файлів (сторінка, списки)", "образ файлової системи");
        await _run.RunAsync(Tools.Mkspiffs!, new[]
        {
            "-c", Path.Combine(Tools.WorkSketch, "data"), "-b", "4096", "-p", "256", "-s", "0x200000", Out("yoRadio.spiffs.bin"),
        }, Tools.OutDir, ct);

        Step("Складаю повний образ", "склейка одного образу");
        File.Copy(Tools.BootApp0, Out("boot_app0.bin"), overwrite: true);
        await _run.RunAsync(Tools.Esptool!, new[]
        {
            "--chip", "esp32s3", "merge-bin", "-o", "PotuzhneRadio-ES3C28P-full.bin",
            "--flash-mode", "dio", "--flash-freq", "80m", "--flash-size", "16MB",
            "0x0", "yoRadio.ino.bootloader.bin",
            "0x8000", "yoRadio.ino.partitions.bin",
            "0xe000", "boot_app0.bin",
            "0x10000", "yoRadio.ino.bin",
            "0x610000", "yoRadio.spiffs.bin",
        }, Tools.OutDir, ct);

        // Імена — нашого проєкту: те, що бачить власник, не нагадує yoRadio.
        var fwDir = _project.Firmware;
        Directory.CreateDirectory(fwDir);
        void Put(string from, string to) => File.Copy(Out(from), Path.Combine(fwDir, to), overwrite: true);
        Put("PotuzhneRadio-ES3C28P-full.bin", "PotuzhneRadio-ES3C28P-full.bin");
        Put("yoRadio.ino.bin", "PotuzhneRadio-ES3C28P-update.bin");
        Put("yoRadio.ino.bootloader.bin", "PotuzhneRadio-ES3C28P-bootloader.bin");
        Put("yoRadio.ino.partitions.bin", "PotuzhneRadio-ES3C28P-partitions.bin");
        Put("yoRadio.spiffs.bin", "PotuzhneRadio-ES3C28P-files.bin");
        Put("boot_app0.bin", "boot_app0.bin");
        var www = Path.Combine(_project.Sketch, "data", "www");
        Directory.CreateDirectory(_project.WebDir);
        foreach (var gz in new[] { "app.js.gz", "app.css.gz" })
            if (File.Exists(Path.Combine(www, gz)))
                File.Copy(Path.Combine(www, gz), Path.Combine(_project.WebDir, gz), overwrite: true);

        var fw = FwInfo.Read(_project.UpdateBin)
                 ?? throw new InvalidOperationException("у зібраній прошивці немає відмітки POTUZHNE-RADIO-FW");
        if (fw.Version != version || fw.Build != build)
            throw new InvalidOperationException($"у прошивці відмітка {fw.Version} / {fw.Build}, а збирали {version} / {build}");
        Step("Готово", $"готово: {fw.Summary}, образи в {fwDir}");
        return fw;
    }
}
