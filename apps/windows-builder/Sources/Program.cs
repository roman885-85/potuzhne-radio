namespace PotuzhneRadio;

static class Program
{
    [STAThread]
    static void Main()
    {
        Log.FileName = "builder-log.txt";
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.ThreadException += (_, e) => Crash(e.Exception);
        AppDomain.CurrentDomain.UnhandledException += (_, e) => Crash(e.ExceptionObject as Exception);
        Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
        Log.Write($"——— Збірка ПОТУЖНОГО РАДІО {typeof(Program).Assembly.GetName().Version} ———");
        Application.Run(new BuilderForm());
    }

    static void Crash(Exception? ex)
    {
        Log.Write($"Непередбачена помилка: {ex}");
        try
        {
            MessageBox.Show("Сталася непередбачена помилка. Подробиці записано в журнал:\n" + Log.FilePath +
                            "\n\n" + (ex?.Message ?? ""), "Збірка ПОТУЖНОГО РАДІО", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        catch { }
    }
}
