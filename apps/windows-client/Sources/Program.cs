namespace PotuzhneRadio;

/// <summary>
/// Точка входу. Прапорці — лише для діагностики пошуку:
///   --only-mdns   шукати тільки через mDNS
///   --only-scan   шукати тільки перебором підмережі
/// </summary>
static class Program
{
    [STAThread]
    static void Main(string[] args)
    {
        var opt = new DiscoveryOptions();
        if (args.Contains("--only-mdns", StringComparer.OrdinalIgnoreCase)) opt.UseScan = false;
        if (args.Contains("--only-scan", StringComparer.OrdinalIgnoreCase)) opt.UseMdns = false;

        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        Application.ThreadException += (_, e) => Crash(e.Exception);
        AppDomain.CurrentDomain.UnhandledException += (_, e) => Crash(e.ExceptionObject as Exception);
        Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);

        Log.Write($"——— ПОТУЖНЕ РАДІО {typeof(Program).Assembly.GetName().Version} ———" +
                  (opt.UseMdns && opt.UseScan ? "" : $" (mDNS {opt.UseMdns}, перебір {opt.UseScan})"));
        Application.Run(new MainForm(opt));
    }

    static void Crash(Exception? ex)
    {
        Log.Write($"Непередбачена помилка: {ex}");
        try
        {
            MessageBox.Show(
                "Сталася непередбачена помилка. Подробиці записано в журнал:\n" + Log.FilePath +
                "\n\n" + (ex?.Message ?? ""),
                "ПОТУЖНЕ РАДІО", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        catch { }
    }
}
