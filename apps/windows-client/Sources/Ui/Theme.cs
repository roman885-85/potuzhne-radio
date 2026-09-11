using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;

namespace PotuzhneRadio;

/// <summary>
/// Кольори й шрифти — ті самі, що на сторінці радіо та в його екранному меню:
/// чорний фон, панелі #111/#1b1b1b, жовтий акцент #e6d25a.
/// </summary>
static class Theme
{
    public static readonly Color Bg = Color.FromArgb(0x00, 0x00, 0x00);
    public static readonly Color Panel = Color.FromArgb(0x11, 0x11, 0x11);
    public static readonly Color Panel2 = Color.FromArgb(0x1b, 0x1b, 0x1b);
    public static readonly Color Line = Color.FromArgb(0x2a, 0x2a, 0x2a);
    public static readonly Color Text = Color.FromArgb(0xf2, 0xf2, 0xf2);
    public static readonly Color Muted = Color.FromArgb(0x8c, 0x8c, 0x8c);
    public static readonly Color Dim = Color.FromArgb(0x55, 0x55, 0x55);
    public static readonly Color Accent = Color.FromArgb(0xe6, 0xd2, 0x5a);
    public static readonly Color AccentHover = Color.FromArgb(0xf0, 0xdf, 0x74);
    public static readonly Color AccentDown = Color.FromArgb(0xc9, 0xb7, 0x48);
    public static readonly Color AccentText = Color.FromArgb(0x00, 0x00, 0x00);
    public static readonly Color Bad = Color.FromArgb(0xef, 0x5b, 0x5b);

    const string Family = "Segoe UI";

    static readonly Dictionary<(int, float, FontStyle), Font> Fonts = new();

    /// <summary>
    /// Шрифт розміру px (у пікселях при 100%) для екрана з таким DPI. Задаємо в
    /// пікселях свідомо: так розмір не залежить від того, як WinForms перераховує
    /// пункти між моніторами, — масштаб рахуємо самі.
    /// </summary>
    public static Font F(int dpi, float px, FontStyle style = FontStyle.Regular)
    {
        var key = (dpi, px, style);
        lock (Fonts)
        {
            if (!Fonts.TryGetValue(key, out var f))
                Fonts[key] = f = new Font(Family, px * dpi / 96f, style, GraphicsUnit.Pixel);
            return f;
        }
    }

    public static GraphicsPath Rounded(RectangleF r, float radius)
    {
        var p = new GraphicsPath();
        var d = Math.Min(radius * 2, Math.Min(r.Width, r.Height));
        if (d <= 0.5f)
        {
            p.AddRectangle(r);
            return p;
        }
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    // ---------------------------------------------------------------------
    //  Темний заголовок вікна (Windows 10 20H1+ / 11)
    // ---------------------------------------------------------------------

    [DllImport("dwmapi.dll")]
    static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

    /// Без цього над чорним вікном висить білий системний заголовок.
    public static void DarkTitleBar(Form f)
    {
        try
        {
            var on = 1;
            if (DwmSetWindowAttribute(f.Handle, 20, ref on, sizeof(int)) != 0)   // DWMWA_USE_IMMERSIVE_DARK_MODE
                DwmSetWindowAttribute(f.Handle, 19, ref on, sizeof(int));        // те саме в Windows 10 до 20H1
            var caption = 0x00000000;                                             // COLORREF 0x00BBGGRR — чорний
            DwmSetWindowAttribute(f.Handle, 35, ref caption, sizeof(int));        // DWMWA_CAPTION_COLOR (лише Windows 11)
        }
        catch
        {
            // Старіша Windows — лишається звичайний заголовок.
        }
    }

    [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
    static extern int SetWindowTheme(IntPtr hwnd, string? app, string? idList);

    /// Темні смуги прокрутки у списку.
    public static void DarkScrollbars(Control c)
    {
        try { SetWindowTheme(c.Handle, "DarkMode_Explorer", null); }
        catch { }
    }

    // ---------------------------------------------------------------------
    //  Тека «Завантаження»
    // ---------------------------------------------------------------------

    [DllImport("shell32.dll")]
    static extern int SHGetKnownFolderPath([MarshalAs(UnmanagedType.LPStruct)] Guid rfid, uint flags, IntPtr token, out IntPtr path);

    /// Справжня тека «Завантаження» користувача — її можуть перенести на інший
    /// диск, тож %USERPROFILE%\Downloads лише запасний варіант.
    public static string DownloadsFolder()
    {
        try
        {
            var downloads = new Guid("374DE290-123F-4565-9164-39C4925E467B");
            if (SHGetKnownFolderPath(downloads, 0, IntPtr.Zero, out var p) == 0)
            {
                try { return Marshal.PtrToStringUni(p) ?? Fallback(); }
                finally { Marshal.FreeCoTaskMem(p); }
            }
        }
        catch { }
        return Fallback();

        static string Fallback() =>
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");
    }

    /// Відкрити адресу в браузері за замовчуванням.
    public static void OpenExternal(string url)
    {
        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(url) { UseShellExecute = true });
        }
        catch (Exception ex)
        {
            Log.Write($"Не вдалося відкрити {url}: {ex.Message}");
            MessageBox.Show($"Не вдалося відкрити браузер.\n\nАдреса: {url}", "ПОТУЖНЕ РАДІО",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }
}
