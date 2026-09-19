namespace PotuzhneRadio;

/// <summary>
/// Головне вікно. Тримає два екрани — пошук і сторінку радіо — і всю логіку
/// переходів між ними: перевірку збереженої адреси при старті, пошук,
/// автопідключення, стеження за зв'язком.
/// </summary>
sealed class MainForm : Form
{
    const string AppTitle = "ПОТУЖНЕ РАДІО";
    const string LostMessage = "Зв'язок з радіо втрачено";
    const string NotFound = "Радіо не знайдено. Перевірте, що комп'ютер і радіо в одній мережі Wi-Fi";

    /// Перевірка зв'язку: раз на 10 с, три невдалі поспіль — радіо зникло.
    static readonly TimeSpan HealthInterval = TimeSpan.FromSeconds(10);
    const int HealthFailLimit = 3;

    readonly Settings _settings = Settings.Load();
    readonly DiscoveryOptions _discovery;
    readonly SearchView _search = new() { Dock = DockStyle.Fill };
    readonly BrowserView _browser = new() { Dock = DockStyle.Fill, Visible = false };
    readonly System.Windows.Forms.Timer _health = new();

    CancellationTokenSource? _searchCts;
    RadioInfo? _current;
    int _healthFails;
    bool _healthBusy;
    /// Людина сама попросила «Знайти інше радіо» — тоді одне знайдене не
    /// підключаємо автоматично, інакше вона одразу опинилась би там, звідки пішла.
    bool _userWantsOther;

    public MainForm(DiscoveryOptions discovery)
    {
        _discovery = discovery;
        Text = AppTitle;
        AutoScaleMode = AutoScaleMode.None;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        KeyPreview = true;
        Font = Theme.F(DeviceDpi, 14f);
        try
        {
            using var s = typeof(MainForm).Assembly.GetManifestResourceStream("AppIcon.ico");
            if (s != null) Icon = new Icon(s);
        }
        catch (Exception ex)
        {
            Log.Write($"Іконка вікна: {ex.Message}");
        }

        MinimumSize = new Size(S(640), S(520));
        RestoreWindow();

        Controls.Add(_browser);
        Controls.Add(_search);

        _search.ConnectRequested += r => Connect(r, "вибрано зі списку");
        _search.SearchRequested += () => StartSearch(message: null, auto: !_userWantsOther);
        _search.ManualRequested += AskAddress;
        _browser.FindOtherRequested += FindOther;
        _browser.PageFailed += reason => _ = OnPageFailedAsync(reason);

        _health.Interval = (int)HealthInterval.TotalMilliseconds;
        _health.Tick += async (_, _) => await HealthTickAsync();
    }

    int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        Theme.DarkTitleBar(this);
    }

    protected override async void OnShown(EventArgs e)
    {
        base.OnShown(e);
        AcceptButton = _search.AcceptButton;
        Log.Write($"Запуск: {Environment.ProcessPath}, Windows {Environment.OSVersion.Version}, DPI {DeviceDpi}");
        await StartupAsync();
        //  Тихо дивимось, чи нема свіжішої версії. Мовчки — щоб не заважати:
        //  питаємо, лише коли справді є що поставити (див. Net/Updater.cs).
        _ = Updater.RunAsync(this, silent: true);
    }

    // ---------------------------------------------------------------------
    //  Старт: спершу збережена адреса, потім пошук
    // ---------------------------------------------------------------------

    async Task StartupAsync()
    {
        var saved = _settings.LastAddress;
        if (!string.IsNullOrWhiteSpace(saved))
        {
            _search.BeginSearch($"Перевіряю останнє радіо ({saved})…");
            var cts = NewSearchToken();
            try
            {
                var r = await RadioProbe.ProbeAsync(saved, TimeSpan.FromSeconds(2), false, cts.Token);
                if (r.Kind == ProbeKind.Radio && r.Radio != null)
                {
                    Log.Write($"Збережена адреса відповідає: {r.Radio}");
                    Connect(r.Radio with { Via = "збережена адреса" }, "збережена адреса");
                    return;
                }
                Log.Write($"Збережена адреса {saved} не відповідає: {r.Detail}");
            }
            catch (OperationCanceledException)
            {
                return;
            }
        }
        StartSearch(message: null, auto: true);
    }

    CancellationTokenSource NewSearchToken()
    {
        _searchCts?.Cancel();
        _searchCts = new CancellationTokenSource();
        return _searchCts;
    }

    /// <summary>
    /// Пошук. auto — якщо знайдеться рівно одне радіо, підключитися самим.
    /// message — повідомлення над списком («Зв'язок з радіо втрачено»).
    /// </summary>
    async void StartSearch(string? message, bool auto)
    {
        ShowSearchScreen();
        _search.SetMessage(message);
        _search.BeginSearch("Шукаю радіо в мережі…");
        var cts = NewSearchToken();

        var progress = new Progress<DiscoveryEvent>(ev =>
        {
            if (cts.IsCancellationRequested) return;
            switch (ev)
            {
                case RadioFoundEvent f:
                    _search.AddRadio(f.Radio, _settings.LastHost);
                    break;
                case ScanProgressEvent p:
                    _search.SetDetail(p.Done < p.Total
                        ? $"Перевіряю мережу {p.Ranges} · {p.Done} з {p.Total}"
                        : $"Мережу {p.Ranges} перевірено");
                    break;
            }
        });

        List<RadioInfo> found;
        try
        {
            found = await Discovery.RunAsync(_discovery, ((IProgress<DiscoveryEvent>)progress).Report, cts.Token);
        }
        catch (OperationCanceledException)
        {
            return;
        }
        catch (Exception ex)
        {
            Log.Write($"Пошук упав: {ex}");
            found = new List<RadioInfo>();
        }
        if (cts.IsCancellationRequested) return;

        // Події Progress могли ще не дійти до вікна — додаємо все знайдене явно.
        foreach (var r in found) _search.AddRadio(r, _settings.LastHost);

        if (found.Count == 0)
        {
            _search.SetStatus(NotFound, busy: false);
            _search.FocusBest();
            return;
        }
        if (found.Count == 1 && auto)
        {
            _search.SetStatus("Знайдено радіо. Підключаюся…", busy: true);
            await Task.Delay(500); // щоб людина встигла побачити, до чого саме
            if (cts.IsCancellationRequested) return;
            Connect(found[0], "єдине знайдене");
            return;
        }
        _search.SetStatus(found.Count == 1
            ? "Знайдено одне радіо"
            : $"Знайдено радіо: {found.Count}. Виберіть, до якого підключитися", busy: false);
        _search.FocusBest();
    }

    async void AskAddress()
    {
        using var dlg = new AddressDialog(_settings.LastAddress);
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        var input = dlg.Address;

        var cts = NewSearchToken();
        ShowSearchScreen();
        _search.SetMessage(null);
        _search.BeginSearch($"Перевіряю «{input}»…");
        ProbeResult r;
        try
        {
            r = await RadioProbe.ProbeManualAsync(input, cts.Token);
        }
        catch (OperationCanceledException)
        {
            return;
        }
        Log.Write($"Адреса вручну «{input}»: {r.Kind} {r.Radio} ({r.Detail})");
        if (r.Kind == ProbeKind.Radio && r.Radio != null)
        {
            Connect(r.Radio with { Via = "вручну" }, "адреса вручну");
            return;
        }
        _search.SetStatus(r.Kind == ProbeKind.NotRadio
            ? $"За адресою «{input}» відповідає не ПОТУЖНЕ РАДІО"
            : $"За адресою «{input}» радіо не відповідає ({r.Detail})", busy: false);
        _search.FocusBest();
    }

    // ---------------------------------------------------------------------
    //  Підключення
    // ---------------------------------------------------------------------

    void Connect(RadioInfo radio, string why)
    {
        _searchCts?.Cancel();
        _current = radio;
        _healthFails = 0;
        _userWantsOther = false;
        Log.Write($"Підключаюсь ({why}): {radio}");

        _settings.LastAddress = radio.Address;
        if (radio.Host.Length > 0) _settings.LastHost = radio.Host;
        _settings.LastStation = radio.Station;
        _settings.Save();

        Text = $"{AppTitle} — {radio.DisplayHost}";
        _search.Visible = false;
        _browser.Visible = true;
        AcceptButton = null;
        _browser.Open(radio);
        _health.Start();
    }

    void Disconnect()
    {
        _health.Stop();
        _current = null;
        _browser.Close();
    }

    void ShowSearchScreen()
    {
        if (_current != null) Disconnect();
        Text = AppTitle;
        _browser.Visible = false;
        _search.Visible = true;
        AcceptButton = _search.AcceptButton;
    }

    void FindOther()
    {
        Log.Write("Знайти інше радіо");
        _userWantsOther = true;
        StartSearch(message: null, auto: false);
    }

    void Lost(string reason)
    {
        Log.Write($"Зв'язок втрачено: {reason}");
        // Радіо могло просто перезавантажитись (скажімо, після прошивки) —
        // тоді пошук знайде його знову й підключиться сам.
        StartSearch(LostMessage, auto: true);
    }

    // ---------------------------------------------------------------------
    //  Стеження за зв'язком
    // ---------------------------------------------------------------------

    async Task HealthTickAsync()
    {
        var radio = _current;
        if (radio == null || _healthBusy) return;
        _healthBusy = true;
        try
        {
            var r = await RadioProbe.ProbeAsync(radio.Address, TimeSpan.FromSeconds(4), false, CancellationToken.None);
            if (_current != radio) return;
            if (r.Kind == ProbeKind.Radio && r.Radio != null)
            {
                if (_healthFails > 0) Log.Write($"Радіо знову відповідає після {_healthFails} невдач");
                _healthFails = 0;
                // Після нової прошивки з'являється ім'я — оновлюємо заголовок.
                if (r.Radio.Host.Length > 0 && r.Radio.Host != radio.Host)
                {
                    _current = radio with { Host = r.Radio.Host, Legacy = r.Radio.Legacy };
                    _settings.LastHost = r.Radio.Host;
                    _settings.Save();
                    Text = $"{AppTitle} — {_current.DisplayHost}";
                    _browser.SetName(_current);
                }
                return;
            }
            _healthFails++;
            Log.Write($"Перевірка зв'язку {_healthFails}/{HealthFailLimit}: {r.Kind} — {r.Detail}");
            if (_healthFails >= HealthFailLimit) Lost($"{HealthFailLimit} перевірки поспіль без відповіді");
        }
        finally
        {
            _healthBusy = false;
        }
    }

    async Task OnPageFailedAsync(string reason)
    {
        var radio = _current;
        if (radio == null) return;
        // Сторінка не завантажилась навіть із другої спроби. Якщо й саме радіо
        // мовчить — зв'язку немає; якщо відповідає, однаково повертаємось до
        // пошуку: показувати сторінку помилки замість радіо нема сенсу.
        var r = await RadioProbe.ProbeAsync(radio.Address, TimeSpan.FromSeconds(3), false, CancellationToken.None);
        if (_current != radio) return;
        Lost($"сторінка не завантажилась ({reason}); /api/hello: {r.Kind} {r.Detail}");
    }

    // ---------------------------------------------------------------------
    //  Клавіші та вікно
    // ---------------------------------------------------------------------

    protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
    {
        if (_current != null && (keyData == Keys.F5 || keyData == (Keys.Control | Keys.R)))
        {
            _browser.Reload();
            return true;
        }
        return base.ProcessCmdKey(ref msg, keyData);
    }

    protected override void OnDpiChanged(DpiChangedEventArgs e)
    {
        base.OnDpiChanged(e);
        Font = Theme.F(DeviceDpi, 14f);
        MinimumSize = new Size(S(640), S(520));
    }

    void RestoreWindow()
    {
        StartPosition = FormStartPosition.CenterScreen;
        ClientSize = new Size(S(1100), S(780));
        var st = _settings;
        if (st.WindowWidth < 200 || st.WindowHeight < 200) return;
        var rect = new Rectangle(st.WindowX, st.WindowY, st.WindowWidth, st.WindowHeight);
        // Лише якщо вікно хоч трохи потрапляє на один із теперішніх моніторів.
        if (!Screen.AllScreens.Any(s => s.WorkingArea.IntersectsWith(Rectangle.Inflate(rect, -40, -40)))) return;
        StartPosition = FormStartPosition.Manual;
        Bounds = rect;
        if (st.WindowMaximized) WindowState = FormWindowState.Maximized;
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _searchCts?.Cancel();
        _health.Stop();
        var b = WindowState == FormWindowState.Normal ? Bounds : RestoreBounds;
        _settings.WindowX = b.X;
        _settings.WindowY = b.Y;
        _settings.WindowWidth = b.Width;
        _settings.WindowHeight = b.Height;
        _settings.WindowMaximized = WindowState == FormWindowState.Maximized;
        _settings.Save();
        Log.Write("Вихід");
        base.OnFormClosing(e);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _health.Dispose();
        base.Dispose(disposing);
    }
}
