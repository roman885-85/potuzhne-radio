using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

namespace PotuzhneRadio;

/// <summary>
/// Екран радіо: зверху вузька панель із кнопками, решту займає власна
/// веб-сторінка радіо у WebView2. Своїх елементів керування радіо програма не
/// має — усе, що вміє радіо, вміє його сторінка.
/// </summary>
sealed class BrowserView : UserControl
{
    // Посилання на встановлювач WebView2 Runtime від Microsoft (Evergreen Bootstrapper).
    const string RuntimeUrl = "https://go.microsoft.com/fwlink/p/?LinkId=2124703";

    readonly Panel _bar = new();
    readonly Label _name = new() { AutoSize = false, UseMnemonic = false, ForeColor = Theme.Text, BackColor = Theme.Panel, TextAlign = ContentAlignment.MiddleLeft };
    readonly Label _notice = new() { AutoSize = false, UseMnemonic = false, ForeColor = Theme.Accent, BackColor = Theme.Panel, TextAlign = ContentAlignment.MiddleRight, Visible = false };
    readonly FlatButton _other = new() { Text = "Знайти інше радіо", FontPx = 13f };
    readonly FlatButton _reload = new() { Text = "Оновити сторінку", FontPx = 13f };
    readonly FlatButton _external = new() { Text = "Відкрити в браузері", FontPx = 13f };
    readonly FlatButton _update = new() { Text = "Перевірити оновлення", FontPx = 13f };
    readonly Panel _host = new() { BackColor = Theme.Bg };
    readonly Label _loading = new() { AutoSize = false, UseMnemonic = false, ForeColor = Theme.Muted, BackColor = Theme.Bg, TextAlign = ContentAlignment.MiddleCenter, Text = "Завантажую сторінку радіо…" };
    readonly Panel _noRuntime = new() { BackColor = Theme.Bg, Visible = false };
    readonly System.Windows.Forms.Timer _noticeTimer = new() { Interval = 12000 };

    WebView2? _web;
    Task<bool>? _init;
    RadioInfo? _radio;
    Uri? _base;
    bool _lastNavFailed;
    int _retries;
    /// Коли почалося останнє завантаження файлу: перехід за посиланням, що
    /// віддає файл, WebView2 закінчує як «невдалий» — це не збій сторінки.
    DateTime _downloadAt = DateTime.MinValue;
    /// Куди йде кожна навігація (за її номером): збоєм сторінки вважаємо лише
    /// невдале відкриття самої сторінки радіо, а не переходу за посиланням з неї.
    readonly Dictionary<ulong, string> _navUris = new();
    Action? _noticeClick;

    /// Натиснули «Знайти інше радіо».
    public event Action? FindOtherRequested;
    /// Сторінка не завантажилась. MainForm сам перевірить, чи живе радіо.
    public event Action<string>? PageFailed;

    public BrowserView()
    {
        AutoScaleMode = AutoScaleMode.None;
        BackColor = Theme.Bg;

        _bar.BackColor = Theme.Panel;
        _bar.Paint += (_, e) =>
        {
            // Тонка лінія під панеллю й значок антени ліворуч.
            using var p = new Pen(Theme.Panel2, Math.Max(1, S(1)));
            e.Graphics.DrawLine(p, 0, _bar.Height - 1, _bar.Width, _bar.Height - 1);
            var icon = S(22);
            Antenna.Draw(e.Graphics, new RectangleF(S(14), (_bar.Height - icon) / 2f, icon, icon), Theme.Accent);
        };
        _bar.Controls.AddRange(new Control[] { _name, _notice, _other, _reload, _external, _update });
        foreach (var b in new[] { _other, _reload, _external, _update }) b.BackColor = Theme.Panel;
        _bar.Layout += (_, _) => LayoutBar();

        _other.Click += (_, _) => FindOtherRequested?.Invoke();
        _reload.Click += (_, _) => Reload();
        _external.Click += (_, _) => { if (_radio != null) Theme.OpenExternal(_radio.BaseUrl); };
        _update.Click += (_, _) => _ = Updater.RunAsync(FindForm(), silent: false);
        _notice.Click += (_, _) => _noticeClick?.Invoke();
        _noticeTimer.Tick += (_, _) => { _noticeTimer.Stop(); _notice.Visible = false; };

        _host.Controls.Add(_loading);
        _host.Controls.Add(_noRuntime);
        _loading.Dock = DockStyle.Fill;
        _noRuntime.Dock = DockStyle.Fill;

        Controls.Add(_host);
        Controls.Add(_bar);
        ApplyFonts();
    }

    int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);

    void ApplyFonts()
    {
        _name.Font = Theme.F(DeviceDpi, 14f, FontStyle.Bold);
        _notice.Font = Theme.F(DeviceDpi, 13f);
        _loading.Font = Theme.F(DeviceDpi, 15f);
    }

    protected override void OnDpiChangedAfterParent(EventArgs e)
    {
        base.OnDpiChangedAfterParent(e);
        ApplyFonts();
        PerformLayout();
        _bar.PerformLayout();
        if (_noRuntime.Visible) BuildNoRuntimePanel(_noRuntimeReason);
    }

    protected override void OnLayout(LayoutEventArgs e)
    {
        base.OnLayout(e);
        var barH = S(48);
        _bar.SetBounds(0, 0, Width, barH);
        _host.SetBounds(0, barH, Width, Math.Max(0, Height - barH));
    }

    void LayoutBar()
    {
        var h = _bar.Height;
        var bh = S(32);
        var gap = S(8);
        var x = _bar.Width - S(12);
        foreach (var b in new[] { _update, _external, _reload, _other })
        {
            var w = b.PreferredWidthFor();
            x -= w;
            b.SetBounds(x, (h - bh) / 2, w, bh);
            x -= gap;
        }
        var nameX = S(14) + S(22) + S(10);
        var nameW = TextRenderer.MeasureText(_name.Text, _name.Font).Width + S(8);
        var free = x - nameX;
        // Сповіщення (про завантажені файли) — між назвою й кнопками.
        if (_notice.Visible && free > S(200))
        {
            nameW = Math.Min(nameW, free / 2);
            _name.SetBounds(nameX, 0, nameW, h - 1);
            _notice.SetBounds(nameX + nameW + gap, 0, x - nameX - nameW - gap, h - 1);
        }
        else
        {
            _name.SetBounds(nameX, 0, Math.Max(0, Math.Min(nameW, free)), h - 1);
        }
    }

    // ---------------------------------------------------------------------
    //  Відкриття сторінки
    // ---------------------------------------------------------------------

    public RadioInfo? Radio => _radio;

    public async void Open(RadioInfo radio)
    {
        _radio = radio;
        _base = new Uri(radio.BaseUrl);
        _retries = 0;
        _lastNavFailed = false;
        SetName(radio);
        HideNotice();
        _loading.Visible = true;
        _loading.BringToFront();

        if (!await EnsureWebViewAsync()) return;
        if (_radio != radio) return; // поки готувався браузер, вибрали інше
        Log.Write($"Відкриваю сторінку {radio.BaseUrl}");
        _web!.CoreWebView2.Navigate(radio.BaseUrl);
    }

    public void SetName(RadioInfo radio)
    {
        _name.Text = $"ПОТУЖНЕ РАДІО · {radio.DisplayHost}";
        _bar.PerformLayout();
    }

    /// Від'єднатись: сторінку прибрати, щоб вона не стукала в радіо з фону.
    public void Close()
    {
        _radio = null;
        _base = null;
        HideNotice();
        try
        {
            if (_web?.CoreWebView2 != null) _web.CoreWebView2.Navigate("about:blank");
        }
        catch (Exception ex)
        {
            Log.Write($"about:blank: {ex.Message}");
        }
    }

    public void Reload()
    {
        if (_radio == null || _web?.CoreWebView2 == null) return;
        _retries = 0;
        // Після невдалого завантаження на екрані сторінка помилки — оновлювати
        // її нема чого, відкриваємо радіо заново.
        if (_lastNavFailed || _web.CoreWebView2.Source is null || !IsRadioUrl(_web.CoreWebView2.Source))
            _web.CoreWebView2.Navigate(_radio.BaseUrl);
        else
            _web.CoreWebView2.Reload();
    }

    /// Сама сторінка радіо: той самий сервер і шлях «/» (параметри й #розділ — байдуже).
    bool IsRadioRoot(string url) =>
        IsRadioUrl(url) && Uri.TryCreate(url, UriKind.Absolute, out var u) && (u.AbsolutePath is "/" or "");

    bool IsRadioUrl(string url) =>
        _base != null && Uri.TryCreate(url, UriKind.Absolute, out var u) &&
        Uri.Compare(u, _base, UriComponents.SchemeAndServer, UriFormat.Unescaped, StringComparison.OrdinalIgnoreCase) == 0;

    // ---------------------------------------------------------------------
    //  WebView2
    // ---------------------------------------------------------------------

    Task<bool> EnsureWebViewAsync() => _init ??= InitWebViewAsync();

    async Task<bool> InitWebViewAsync()
    {
        string? version = null;
        try
        {
            version = CoreWebView2Environment.GetAvailableBrowserVersionString();
        }
        catch (WebView2RuntimeNotFoundException)
        {
        }
        catch (Exception ex)
        {
            Log.Write($"WebView2: перевірка версії — {ex.GetType().Name}: {ex.Message}");
        }
        if (string.IsNullOrEmpty(version))
        {
            Log.Write("WebView2 Runtime не знайдено");
            ShowNoRuntime(null);
            return false;
        }
        Log.Write($"WebView2 Runtime {version}");

        try
        {
            var web = new WebView2
            {
                Dock = DockStyle.Fill,
                // Чорне тло до першого кадру — без білого спалаху на чорному вікні.
                DefaultBackgroundColor = Theme.Bg,
            };
            _host.Controls.Add(web);
            _loading.BringToFront();

            Directory.CreateDirectory(AppPaths.WebViewDir);
            var options = new CoreWebView2EnvironmentOptions
            {
                // Меню, діалоги й панель завантажень самого браузера — українською.
                Language = "uk",
            };
            var env = await CoreWebView2Environment.CreateAsync(null, AppPaths.WebViewDir, options);
            await web.EnsureCoreWebView2Async(env);
            _web = web;
            Configure(web.CoreWebView2);
            return true;
        }
        catch (Exception ex)
        {
            Log.Write($"WebView2 не запустився: {ex.GetType().Name}: {ex.Message}");
            _init = null; // дозволити ще одну спробу кнопкою
            foreach (var w in _host.Controls.OfType<WebView2>().ToList())
            {
                _host.Controls.Remove(w);
                w.Dispose();
            }
            ShowNoRuntime(ex.Message);
            return false;
        }
    }

    void Configure(CoreWebView2 core)
    {
        var s = core.Settings;
        s.AreDevToolsEnabled = false;
        s.IsStatusBarEnabled = false;
        s.AreHostObjectsAllowed = false;
        s.IsWebMessageEnabled = false;
        s.IsPasswordAutosaveEnabled = false;
        s.IsZoomControlEnabled = true;
        s.AreBrowserAcceleratorKeysEnabled = true; // F5, Ctrl+F, Ctrl+± — як у браузері

        // Файли зі сторінки — у «Завантаження» користувача. Це стосується і
        // звичайних посилань (/api/rec?dl=1&f=…), і blob:-посилань з download.
        var downloads = Theme.DownloadsFolder();
        try
        {
            core.Profile.DefaultDownloadFolderPath = downloads;
        }
        catch (Exception ex)
        {
            Log.Write($"Тека завантажень {downloads}: {ex.Message}");
        }
        Log.Write($"Завантаження йдуть у {downloads}");

        core.NavigationStarting += OnNavigationStarting;
        core.NavigationCompleted += OnNavigationCompleted;
        core.NewWindowRequested += OnNewWindowRequested;
        core.DownloadStarting += OnDownloadStarting;
        core.PermissionRequested += OnPermissionRequested;
        core.ProcessFailed += OnProcessFailed;
    }

    void OnNavigationStarting(object? sender, CoreWebView2NavigationStartingEventArgs e)
    {
        var uri = e.Uri ?? "";
        _navUris[e.NavigationId] = uri;
        if (_navUris.Count > 64) _navUris.Clear();
        if (uri.StartsWith("about:", StringComparison.OrdinalIgnoreCase) ||
            uri.StartsWith("data:", StringComparison.OrdinalIgnoreCase) ||
            uri.StartsWith("blob:", StringComparison.OrdinalIgnoreCase))
            return;
        if (IsRadioUrl(uri)) return;

        // Чуже посилання (сайт станції, довідка тощо) — у звичайний браузер,
        // а вікно програми лишається зі сторінкою радіо.
        e.Cancel = true;
        if (uri.StartsWith("http://", StringComparison.OrdinalIgnoreCase) ||
            uri.StartsWith("https://", StringComparison.OrdinalIgnoreCase) ||
            uri.StartsWith("mailto:", StringComparison.OrdinalIgnoreCase))
        {
            Log.Write($"Зовнішнє посилання — у браузер: {uri}");
            Theme.OpenExternal(uri);
        }
    }

    void OnPermissionRequested(object? sender, CoreWebView2PermissionRequestedEventArgs e)
    {
        // Кілька файлів поспіль (експорт станцій, обраного, записів) — сторінці
        // радіо дозволено без запитань. Без цього WebView2 мовчки блокує друге
        // й наступні завантаження: запит іде, а файл не з'являється.
        if (e.PermissionKind == CoreWebView2PermissionKind.MultipleAutomaticDownloads && IsRadioUrl(e.Uri ?? ""))
        {
            e.State = CoreWebView2PermissionState.Allow;
            Log.Write("Дозволено кілька завантажень поспіль");
        }
    }

    void OnNewWindowRequested(object? sender, CoreWebView2NewWindowRequestedEventArgs e)
    {
        // Нових вікон усередині програми не буває: target=_blank і window.open —
        // у звичайний браузер.
        e.Handled = true;
        var uri = e.Uri ?? "";
        if (uri.StartsWith("http://", StringComparison.OrdinalIgnoreCase) ||
            uri.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
        {
            Log.Write($"Нове вікно — у браузер: {uri}");
            Theme.OpenExternal(uri);
        }
    }

    async void OnNavigationCompleted(object? sender, CoreWebView2NavigationCompletedEventArgs e)
    {
        var src = _web?.CoreWebView2?.Source ?? "";
        if (_radio == null || src.StartsWith("about:", StringComparison.OrdinalIgnoreCase))
            return;

        if (e.IsSuccess)
        {
            _loading.Visible = false;
            _lastNavFailed = false;
            _retries = 0;
            Log.Write($"Сторінку завантажено: {src}");
            return;
        }

        // Одну навігацію перебила інша (F5 під час завантаження тощо) — не збій.
        if (e.WebErrorStatus == CoreWebView2WebErrorStatus.OperationCanceled) return;
        // Не вдався перехід за посиланням зі сторінки (скажімо, на файл) — сама
        // сторінка радіо лишається на екрані, перезавантажувати її не треба.
        if (_navUris.Remove(e.NavigationId, out var target) && !IsRadioRoot(target))
        {
            Log.Write($"Перехід на {target} не вдався ({e.WebErrorStatus}) — сторінка лишається");
            return;
        }
        // Перехід обернувся завантаженням файлу — сторінка на місці, це теж не збій.
        if ((DateTime.UtcNow - _downloadAt).TotalSeconds < 10)
        {
            Log.Write($"Перехід став завантаженням файлу ({e.WebErrorStatus}) — сторінка лишається");
            return;
        }

        _lastNavFailed = true;
        _loading.Visible = false;
        Log.Write($"Сторінка не завантажилась: {src} — {e.WebErrorStatus}, HTTP {e.HttpStatusCode}");

        // Одна повторна спроба: радіо зайняте звуком іноді рве з'єднання.
        if (_retries++ < 1 && _radio != null)
        {
            var radio = _radio;
            await Task.Delay(1500);
            if (_radio == radio && _web?.CoreWebView2 != null)
            {
                Log.Write("Повторна спроба завантажити сторінку");
                _web.CoreWebView2.Navigate(radio.BaseUrl);
            }
            return;
        }
        PageFailed?.Invoke($"{e.WebErrorStatus}");
    }

    void OnDownloadStarting(object? sender, CoreWebView2DownloadStartingEventArgs e)
    {
        _downloadAt = DateTime.UtcNow;
        var op = e.DownloadOperation;
        var name = Path.GetFileName(e.ResultFilePath);
        Log.Write($"Завантаження: {e.DownloadOperation.Uri} → {e.ResultFilePath}");
        ShowNotice($"Зберігаю «{name}»…", null);
        op.StateChanged += (_, _) =>
        {
            // Шлях міг змінитися (файл із такою назвою вже був) — беремо остаточний.
            var path = op.ResultFilePath;
            var file = Path.GetFileName(path);
            switch (op.State)
            {
                case CoreWebView2DownloadState.Completed:
                    Log.Write($"Збережено: {path} ({op.TotalBytesToReceive?.ToString() ?? "?"} байт)");
                    ShowNotice($"Збережено в «Завантаження»: {file} — показати", () => ShowInFolder(path));
                    break;
                case CoreWebView2DownloadState.Interrupted:
                    Log.Write($"Завантаження перервано: {path} — {op.InterruptReason}");
                    ShowNotice($"Не вдалося зберегти «{file}»", null);
                    break;
            }
        };
    }

    static void ShowInFolder(string path)
    {
        try
        {
            System.Diagnostics.Process.Start("explorer.exe", $"/select,\"{path}\"");
        }
        catch (Exception ex)
        {
            Log.Write($"Провідник: {ex.Message}");
        }
    }

    void OnProcessFailed(object? sender, CoreWebView2ProcessFailedEventArgs e)
    {
        Log.Write($"WebView2: процес упав — {e.ProcessFailedKind}");
        if (_radio == null || _web?.CoreWebView2 == null) return;
        if (e.ProcessFailedKind is CoreWebView2ProcessFailedKind.RenderProcessExited or CoreWebView2ProcessFailedKind.RenderProcessUnresponsive)
        {
            try { _web.CoreWebView2.Navigate(_radio.BaseUrl); }
            catch (Exception ex) { Log.Write($"Перезапуск сторінки: {ex.Message}"); }
        }
        else if (e.ProcessFailedKind == CoreWebView2ProcessFailedKind.BrowserProcessExited)
        {
            // Увесь браузер закрився — створюємо його наново.
            var radio = _radio;
            _host.Controls.Remove(_web);
            _web.Dispose();
            _web = null;
            _init = null;
            Open(radio);
        }
    }

    // ---------------------------------------------------------------------
    //  Сповіщення на панелі
    // ---------------------------------------------------------------------

    void ShowNotice(string text, Action? onClick)
    {
        if (InvokeRequired) { BeginInvoke(() => ShowNotice(text, onClick)); return; }
        _notice.Text = text;
        _noticeClick = onClick;
        _notice.Cursor = onClick != null ? Cursors.Hand : Cursors.Default;
        _notice.Font = Theme.F(DeviceDpi, 13f, onClick != null ? FontStyle.Underline : FontStyle.Regular);
        _notice.Visible = true;
        _noticeTimer.Stop();
        _noticeTimer.Start();
        _bar.PerformLayout();
    }

    void HideNotice()
    {
        _noticeTimer.Stop();
        _notice.Visible = false;
        _noticeClick = null;
        _bar.PerformLayout();
    }

    // ---------------------------------------------------------------------
    //  Немає WebView2 Runtime
    // ---------------------------------------------------------------------

    string? _noRuntimeReason;

    void ShowNoRuntime(string? reason)
    {
        _noRuntimeReason = reason;
        BuildNoRuntimePanel(reason);
        _loading.Visible = false;
        _noRuntime.Visible = true;
        _noRuntime.BringToFront();
    }

    void BuildNoRuntimePanel(string? reason)
    {
        _noRuntime.SuspendLayout();
        foreach (Control c in _noRuntime.Controls.Cast<Control>().ToList()) c.Dispose();
        _noRuntime.Controls.Clear();

        var title = new Label
        {
            AutoSize = false, UseMnemonic = false, ForeColor = Theme.Text, BackColor = Theme.Bg,
            Font = Theme.F(DeviceDpi, 20f, FontStyle.Bold), TextAlign = ContentAlignment.MiddleCenter,
            Text = "Потрібен компонент WebView2",
        };
        var text = new Label
        {
            AutoSize = false, UseMnemonic = false, ForeColor = Theme.Muted, BackColor = Theme.Bg,
            Font = Theme.F(DeviceDpi, 14f), TextAlign = ContentAlignment.TopCenter,
            Text = "Щоб показувати сторінку радіо у своєму вікні, програмі потрібен Microsoft Edge WebView2 Runtime. " +
                   "У Windows 11 він є завжди, у Windows 10 його іноді треба встановити — це безкоштовно й займає хвилину.\n\n" +
                   "Поки що радіо можна відкрити у звичайному браузері." +
                   (reason != null ? $"\n\nПричина: {reason}" : ""),
        };
        var link = new LinkLabel
        {
            AutoSize = false, UseMnemonic = false, BackColor = Theme.Bg, Font = Theme.F(DeviceDpi, 14f),
            LinkColor = Theme.Accent, ActiveLinkColor = Theme.AccentHover, VisitedLinkColor = Theme.Accent,
            TextAlign = ContentAlignment.MiddleCenter, Text = "Завантажити WebView2 Runtime з сайту Microsoft",
        };
        link.LinkClicked += (_, _) => Theme.OpenExternal(RuntimeUrl);
        var open = new FlatButton { Text = "Відкрити в браузері", Accent = true };
        open.Click += (_, _) => { if (_radio != null) Theme.OpenExternal(_radio.BaseUrl); };
        var retry = new FlatButton { Text = "Спробувати ще раз" };
        retry.Click += (_, _) =>
        {
            _noRuntime.Visible = false;
            _init = null;
            if (_radio != null) Open(_radio);
        };
        _noRuntime.Controls.AddRange(new Control[] { title, text, link, open, retry });

        void Place()
        {
            var w = Math.Min(S(620), _noRuntime.Width - S(48));
            var x = (_noRuntime.Width - w) / 2;
            var th = TextRenderer.MeasureText(text.Text, text.Font, new Size(w, 0), TextFormatFlags.WordBreak).Height + S(8);
            var total = S(40) + S(12) + th + S(12) + S(28) + S(24) + S(40);
            var y = Math.Max(S(24), (_noRuntime.Height - total) / 2);
            title.SetBounds(x, y, w, S(40)); y += S(40) + S(12);
            text.SetBounds(x, y, w, th); y += th + S(12);
            link.SetBounds(x, y, w, S(28)); y += S(28) + S(24);
            var w1 = open.PreferredWidthFor(); var w2 = retry.PreferredWidthFor();
            var bx = (_noRuntime.Width - (w1 + S(10) + w2)) / 2;
            open.SetBounds(bx, y, w1, S(40));
            retry.SetBounds(bx + w1 + S(10), y, w2, S(40));
        }
        _noRuntime.Layout -= _noRuntimeLayout;
        _noRuntimeLayout = (_, _) => Place();
        _noRuntime.Layout += _noRuntimeLayout;
        _noRuntime.ResumeLayout();
        Place();
    }

    LayoutEventHandler? _noRuntimeLayout;

    protected override void Dispose(bool disposing)
    {
        if (disposing) _noticeTimer.Dispose();
        base.Dispose(disposing);
    }
}
