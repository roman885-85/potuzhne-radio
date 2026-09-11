using System.Diagnostics;

namespace PotuzhneRadio;

/// <summary>
/// Вікно «Збірка ПОТУЖНОГО РАДІО»: шапка з текою проєкту, картки «Інструменти»
/// (лише поки їх немає), «Прошивка», «Журнал збірки» й «Оновити радіо по Wi-Fi».
/// Поведінка й тексти — з Mac-версії (apps/mac-builder).
/// </summary>
sealed class BuilderForm : Form
{
    enum State { Idle, Installing, Building, Ok, Failed, Cancelled }

    readonly BuilderSettings _settings = BuilderSettings.Load();
    readonly Runner _runner = new();
    readonly Panel _scroll = new() { AutoScroll = true, Dock = DockStyle.Fill, BackColor = Theme.Bg };
    readonly Header _header;
    readonly ToolsCard _tools;
    readonly FirmwareCard _firmware;
    readonly LogCard _log;
    readonly RadioCard _radio;
    readonly System.Windows.Forms.Timer _tick = new() { Interval = 1000 };

    Project? _project;
    FwInfo? _fw;
    State _state = State.Idle;
    string _phase = "";
    string _failText = "";
    string _lastError = "";
    DateTime _started;
    TimeSpan _took;
    bool _showLog;
    CancellationTokenSource? _workCts;
    Mutex? _lock;
    StreamWriter? _buildLog;

    // Радіо
    CancellationTokenSource? _searchCts;
    bool _searching;
    bool _uploading;

    public BuilderForm()
    {
        Text = "Збірка ПОТУЖНОГО РАДІО";
        AutoScaleMode = AutoScaleMode.None;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        Font = Theme.F(DeviceDpi, 14f);
        try
        {
            using var s = typeof(BuilderForm).Assembly.GetManifestResourceStream("AppIcon.ico");
            if (s != null) Icon = new Icon(s);
        }
        catch { }

        _header = new Header(this);
        _tools = new ToolsCard(this);
        _firmware = new FirmwareCard(this);
        _log = new LogCard();
        _radio = new RadioCard(this);
        _scroll.Controls.AddRange(new Control[] { _header, _tools, _firmware, _log, _radio });
        Controls.Add(_scroll);
        _scroll.Layout += (_, _) => LayoutAll();
        _scroll.Resize += (_, _) => LayoutAll();

        _runner.Line += OnLine;
        _tick.Tick += (_, _) => { if (_state is State.Building or State.Installing) _firmware.UpdateTimer(); };

        MinimumSize = new Size(S(640), S(640));
        RestoreWindow();

        _project = FindProject();
        LoadProject();
    }

    int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        Theme.DarkTitleBar(this);
    }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        Log.Write($"Запуск: {Environment.ProcessPath}; проєкт: {_project?.Root ?? "не знайдено"}; інструменти: {Tools.Root} " +
                  $"(arduino-cli {(Tools.CliReady ? "є" : "немає")}, ядро {(Tools.CoreReady ? "є" : "немає")})");
        RefreshAll();
        StartSearch();
    }

    // ---------------------------------------------------------------------
    //  Розкладка: картки одна під одною, прокрутка — у всього вікна
    // ---------------------------------------------------------------------

    bool _inLayout;

    void LayoutAll()
    {
        // Зміна розмірів картки знову запитує розкладку в батька — без цього
        // запобіжника вона ходила б по колу.
        if (_inLayout || _scroll.ClientSize.Width <= 0 || _radio == null) return;
        _inLayout = true;
        try { LayoutCore(); }
        finally { _inLayout = false; }
    }

    void LayoutCore()
    {
        var pad = S(20);
        var w = _scroll.ClientSize.Width - 2 * pad;
        var y = pad + _scroll.AutoScrollPosition.Y;
        foreach (var c in new Control[] { _header, _tools, _firmware, _log, _radio })
        {
            if (!c.Visible) continue;
            c.Left = pad;
            c.Top = y;
            if (c is Card card) card.Relayout(w); else ((Header)c).Relayout(w);
            y += c.Height + S(14);
        }
        var total = y - _scroll.AutoScrollPosition.Y + pad - S(14);
        if (_scroll.AutoScrollMinSize.Height != total)
            _scroll.AutoScrollMinSize = new Size(0, total);
    }

    void RefreshAll()
    {
        _header.Sync(_project);
        _tools.Visible = !Tools.CoreReady || _state == State.Installing || (_state == State.Failed && !Tools.CoreReady);
        _tools.Sync(_state == State.Installing);
        _firmware.Sync();
        _log.Visible = _showLog || _state is State.Building or State.Installing or State.Failed;
        _radio.Sync();
        LayoutAll();
    }

    // ---------------------------------------------------------------------
    //  Проєкт
    // ---------------------------------------------------------------------

    Project? FindProject()
    {
        var near = Project.NextToApp();
        if (near != null) return new Project(near);
        if (Project.IsProject(_settings.ProjectPath)) return new Project(_settings.ProjectPath!);
        return null;
    }

    void LoadProject()
    {
        if (_project == null) return;
        _firmware.Version = _project.ReadVersion();
        _fw = FwInfo.Read(_project.UpdateBin);
    }

    public void ChooseProject()
    {
        if (Busy) return;
        using var dlg = new FolderBrowserDialog
        {
            Description = "Виберіть теку «Проєкт» — у ній лежать теки source, build і firmware",
            UseDescriptionForTitle = true,
            ShowNewFolderButton = false,
        };
        if (_project != null) dlg.InitialDirectory = _project.Root;
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        if (!Project.IsProject(dlg.SelectedPath))
        {
            MessageBox.Show(this, "У цій теці немає проєкту ПОТУЖНОГО РАДІО: не знайдено source і build\\sketchbook.",
                Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        _project = new Project(dlg.SelectedPath);
        _settings.ProjectPath = _project.Root;
        _settings.Save();
        Log.Write($"Проєкт: {_project.Root}");
        LoadProject();
        RefreshAll();
    }

    bool Busy => _state is State.Building or State.Installing;

    // ---------------------------------------------------------------------
    //  Журнал
    // ---------------------------------------------------------------------

    void OnLine(string line)
    {
        try { _buildLog?.WriteLine(line); } catch { }
        if (line.Contains("error", StringComparison.OrdinalIgnoreCase) || line.Contains("помилк", StringComparison.OrdinalIgnoreCase))
            _lastError = line.Trim();
        if (IsHandleCreated) BeginInvoke(() => _log.Append(line));
    }

    void OpenBuildLog(string name)
    {
        try
        {
            Directory.CreateDirectory(AppPaths.DataDir);
            _buildLog = new StreamWriter(Path.Combine(AppPaths.DataDir, name), false, new System.Text.UTF8Encoding(true)) { AutoFlush = true };
        }
        catch { _buildLog = null; }
    }

    void CloseBuildLog()
    {
        try { _buildLog?.Dispose(); } catch { }
        _buildLog = null;
    }

    public void ToggleLog()
    {
        _showLog = !_showLog;
        RefreshAll();
    }

    /// Дві збірки одночасно псують одна одну — і в цьому вікні, і з двох вікон.
    bool TakeLock(out string why)
    {
        why = "";
        if (Process.GetProcessesByName("arduino-cli").Length > 0)
        {
            why = "Зараз уже йде інша збірка (ще в одному вікні програми). Дві одночасні псують одна одну — дочекайтесь її.";
            return false;
        }
        _lock ??= new Mutex(false, @"Local\PotuzhneRadio-Builder");
        try
        {
            if (_lock.WaitOne(0)) return true;
        }
        catch (AbandonedMutexException)
        {
            return true; // попереднє вікно впало посеред збірки — замок наш
        }
        why = "Зараз уже йде інша збірка (ще в одному вікні програми). Дві одночасні псують одна одну — дочекайтесь її.";
        return false;
    }

    void ReleaseLock()
    {
        try { _lock?.ReleaseMutex(); } catch { }
    }

    // ---------------------------------------------------------------------
    //  Інструменти
    // ---------------------------------------------------------------------

    public async void InstallTools()
    {
        if (_state == State.Installing) { Stop(); return; }
        if (Busy) return;
        if (!TakeLock(out var why)) { _tools.Error = why; RefreshAll(); return; }

        _state = State.Installing;
        _started = DateTime.Now;
        _tools.Error = "";
        _log.Clear();
        OpenBuildLog("builder-tools.txt");
        _workCts = new CancellationTokenSource();
        _tick.Start();
        RefreshAll();
        var inst = new ToolsInstaller(_runner);
        inst.Progress += (phase, frac, detail) => BeginInvoke(() => _tools.SetProgress(phase, frac, detail));
        try
        {
            await Task.Run(() => inst.InstallAsync(_workCts.Token));
            _state = State.Idle;
            Log.Write($"Інструменти встановлено за {Fmt.Elapsed(DateTime.Now - _started)}");
        }
        catch (OperationCanceledException)
        {
            _state = State.Cancelled;
            _tools.Error = "Установку зупинено. Наступна спроба продовжить з того, що вже завантажено.";
        }
        catch (Exception ex)
        {
            _state = State.Failed;
            _tools.Error = "Не вдалося встановити інструменти: " + (_lastError.Length > 0 && ex is ToolFailedException ? _lastError : ex.Message) +
                           "\nПеревірте інтернет і натисніть «Встановити» ще раз — уже завантажене не пропаде.";
            Log.Write($"Установка інструментів: {ex}");
        }
        finally
        {
            _tick.Stop();
            CloseBuildLog();
            ReleaseLock();
            _workCts = null;
            RefreshAll();
        }
    }

    // ---------------------------------------------------------------------
    //  Збірка
    // ---------------------------------------------------------------------

    public async void Build()
    {
        if (_state == State.Building) { Stop(); return; }
        if (Busy) return;
        var version = _firmware.Version.Trim();
        if (_project == null || !_project.IsValid)
        {
            Fail("Не знайдено теку проєкту. Вкажіть її — «Змінити…» вгорі.");
            return;
        }
        if (!Fmt.VersionValid(version))
        {
            Fail("Номер версії — цифри через крапку, наприклад 1.1");
            return;
        }
        if (!Tools.CoreReady)
        {
            Fail("Спершу встановіть інструменти для збірки — картка вгорі.");
            return;
        }
        if (!TakeLock(out var why)) { Fail(why); return; }

        _state = State.Building;
        _phase = "Готуюсь…";
        _lastError = "";
        _started = DateTime.Now;
        _log.Clear();
        OpenBuildLog("builder-last-build.txt");
        _workCts = new CancellationTokenSource();
        _tick.Start();
        RefreshAll();

        var job = new FirmwareBuild(_runner, _project);
        job.Phase += p => BeginInvoke(() => { _phase = p; _firmware.Sync(); });
        try
        {
            _fw = await Task.Run(() => job.RunAsync(version, _workCts.Token));
            _took = DateTime.Now - _started;
            _state = State.Ok;
            _phase = "Готово";
            Log.Write($"Зібрано за {Fmt.Elapsed(_took)}: {_fw.Summary}");
        }
        catch (OperationCanceledException)
        {
            _state = State.Cancelled;
            _phase = "Зупинено";
            Log.Write("Збірку зупинено");
        }
        catch (Exception ex)
        {
            _took = DateTime.Now - _started;
            _state = State.Failed;
            _failText = "Збірка не вдалася: " + (ex is ToolFailedException && _lastError.Length > 0 ? _lastError : ex.Message);
            Log.Write($"Збірка не вдалася: {ex.Message}; остання помилка: {_lastError}");
        }
        finally
        {
            _tick.Stop();
            CloseBuildLog();
            ReleaseLock();
            _workCts = null;
            _fw = FwInfo.Read(_project.UpdateBin);
            RefreshAll();
        }
    }

    void Fail(string text)
    {
        _state = State.Failed;
        _failText = text;
        RefreshAll();
    }

    void Stop()
    {
        if (_workCts == null) return;
        Log.Write("Зупиняю за проханням");
        _workCts.Cancel();
        _runner.Kill();
    }

    public void BumpVersion()
    {
        _firmware.Version = Fmt.NextVersion(_firmware.Version);
        _firmware.Sync();
    }

    public void RevealFirmware()
    {
        if (_project == null) return;
        try
        {
            if (File.Exists(_project.UpdateBin)) Process.Start("explorer.exe", $"/select,\"{_project.UpdateBin}\"");
            else Process.Start("explorer.exe", $"\"{_project.Firmware}\"");
        }
        catch (Exception ex) { Log.Write($"Провідник: {ex.Message}"); }
    }

    // ---------------------------------------------------------------------
    //  Радіо
    // ---------------------------------------------------------------------

    public async void StartSearch()
    {
        _searchCts?.Cancel();
        var cts = _searchCts = new CancellationTokenSource();
        _searching = true;
        _radio.Sync();
        var progress = new Progress<DiscoveryEvent>(ev =>
        {
            if (!cts.IsCancellationRequested && ev is RadioFoundEvent f) _radio.AddRadio(f.Radio);
        });
        try
        {
            var found = await Discovery.RunAsync(new DiscoveryOptions(), ((IProgress<DiscoveryEvent>)progress).Report, cts.Token);
            foreach (var r in found) _radio.AddRadio(r);
        }
        catch (OperationCanceledException) { return; }
        catch (Exception ex) { Log.Write($"Пошук: {ex}"); }
        if (cts != _searchCts) return;
        _searching = false;
        RefreshAll();
    }

    public async void AskAddress()
    {
        using var dlg = new AddressDialog(_settings.LastAddress);
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        _radio.Status = $"Перевіряю «{dlg.Address}»…";
        _radio.Error = "";
        RefreshAll();
        var r = await RadioProbe.ProbeManualAsync(dlg.Address, CancellationToken.None);
        if (r.Kind == ProbeKind.Radio && r.Radio != null)
        {
            _settings.LastAddress = r.Radio.Address;
            _settings.Save();
            _radio.AddRadio(r.Radio with { Via = "вручну" }, select: true);
            _radio.Status = "";
        }
        else
        {
            _radio.Status = "";
            _radio.Error = r.Kind == ProbeKind.NotRadio
                ? $"За адресою «{dlg.Address}» відповідає не ПОТУЖНЕ РАДІО."
                : $"За адресою «{dlg.Address}» радіо не відповідає ({r.Detail}).";
        }
        RefreshAll();
    }

    public (string, bool) DescribeRadio(RadioInfo r)
    {
        if (r.Build.Length == 0) return ("версія невідома", false);
        var cmp = Fmt.Compare(_fw, r.Build);
        return ($"на радіо: {r.Version}, зібрано {r.Build}" + (cmp.Length > 0 ? " — " + cmp : ""), false);
    }

    public bool CanUploadFirmware => _radio.Selected != null && _fw != null && !_uploading && !Busy;
    public bool CanUploadWeb => _radio.Selected != null && !_uploading && !Busy;

    public async void UploadFirmware()
    {
        var r = _radio.Selected;
        if (r == null || _fw == null || _project == null) return;
        var name = r.Host.Length > 0 ? r.Host : r.Address;
        using (var ask = new ConfirmDialog("Оновити прошивку радіо?",
                   $"ПОТУЖНЕ РАДІО {_fw.Version}, зібрано {_fw.Build} → {name}.\n" +
                   "Звук на радіо зупиниться приблизно на хвилину; не вимикайте його, доки йде оновлення.", "Оновити"))
            if (ask.ShowDialog(this) != DialogResult.OK) return;

        byte[] bin;
        try { bin = await File.ReadAllBytesAsync(_fw.Path); }
        catch (Exception ex) { _radio.Error = $"Файл прошивки не прочитано: {ex.Message}"; RefreshAll(); return; }
        var expect = _fw.Build;
        Log.Write($"Заливаю прошивку {_fw.Summary} → {r.Address}");
        var ok = await SendAsync($"http://{r.Address}/update", Uploader.FirmwareForm(bin), "Надсилаю прошивку", body =>
        {
            if (!body.TrimStart().StartsWith("OK", StringComparison.Ordinal))
                return "Радіо відхилило прошивку: " + System.Text.RegularExpressions.Regex.Replace(body, "<[^>]+>", " ").Trim();
            return null;
        });
        if (!ok) return;

        _uploading = true;
        _radio.Status = "Прошивку записано. Радіо перезавантажується…";
        RefreshAll();
        var n = await Uploader.WaitForRebootAsync(r.Address, s => BeginInvoke(() => { _radio.Status = s; _radio.Sync(); }), CancellationToken.None);
        _uploading = false;
        if (n == null)
            _radio.Error = "Радіо не відповідає вже півтори хвилини. Перевірте його екран.";
        else
        {
            _radio.AddRadio(n, select: true);
            if (n.Build == expect) _radio.Status = $"Готово: на радіо ПОТУЖНЕ РАДІО {n.Version}, зібрано {n.Build}.";
            else _radio.Error = $"Радіо знову на зв'язку, але в ньому збірка {n.Build}, а не {expect}.";
        }
        Log.Write($"Після прошивки: {_radio.Status}{_radio.Error}");
        RefreshAll();
    }

    public async void UploadWeb()
    {
        var r = _radio.Selected;
        if (r == null || _project == null) return;
        var files = new[] { "app.js.gz", "app.css.gz" }.Select(f => Path.Combine(_project.WebDir, f)).ToArray();
        if (!files.All(File.Exists))
        {
            _radio.Error = "У firmware\\web немає app.js.gz і app.css.gz — спершу зберіть.";
            RefreshAll();
            return;
        }
        var name = r.Host.Length > 0 ? r.Host : r.Address;
        using (var ask = new ConfirmDialog("Оновити сторінку радіо?",
                   $"Файли app.js.gz і app.css.gz → {name}. На час надсилання звук на радіо зупиниться.", "Оновити"))
            if (ask.ShowDialog(this) != DialogResult.OK) return;
        Log.Write($"Заливаю сторінку → {r.Address}");
        if (await SendAsync($"http://{r.Address}/webboard", Uploader.WebForm(files), "Надсилаю сторінку", _ => null))
            _radio.Status = "Сторінку оновлено. Відкриті вікна з нею перезавантажаться самі.";
        RefreshAll();
    }

    /// Надіслати форму; check повертає текст помилки або null. true — успіх.
    async Task<bool> SendAsync(string url, System.Net.Http.MultipartFormDataContent form, string title, Func<string, string?> check)
    {
        _uploading = true;
        _radio.Error = "";
        _radio.Status = title + "…";
        _radio.Progress = 0;
        RefreshAll();
        try
        {
            var (code, body) = await Uploader.PostAsync(url, form,
                (sent, total) => BeginInvoke(() =>
                {
                    _radio.Progress = total > 0 ? (double)sent / total : null;
                    _radio.Status = $"Надіслано {Fmt.Size(sent)} з {Fmt.Size(total)}";
                    _radio.Sync();
                }), CancellationToken.None);
            Log.Write($"POST {url}: HTTP {code} «{(body.Length > 120 ? body[..120] : body)}»");
            if (code >= 400) { _radio.Error = $"Радіо відповіло помилкою {code}"; return false; }
            var err = check(body);
            if (err != null) { _radio.Error = err; return false; }
            return true;
        }
        catch (Exception ex)
        {
            _radio.Error = "Зв'язок перервався: " + (ex.InnerException?.Message ?? ex.Message);
            Log.Write($"POST {url}: {ex}");
            return false;
        }
        finally
        {
            _uploading = false;
            _radio.Progress = null;
            RefreshAll();
        }
    }

    // ---------------------------------------------------------------------
    //  Вікно
    // ---------------------------------------------------------------------

    void RestoreWindow()
    {
        StartPosition = FormStartPosition.CenterScreen;
        ClientSize = new Size(S(780), S(820));
        var st = _settings;
        if (st.WindowWidth < 200 || st.WindowHeight < 200) return;
        var rect = new Rectangle(st.WindowX, st.WindowY, st.WindowWidth, st.WindowHeight);
        if (!Screen.AllScreens.Any(s => s.WorkingArea.IntersectsWith(Rectangle.Inflate(rect, -40, -40)))) return;
        StartPosition = FormStartPosition.Manual;
        Bounds = rect;
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        if (Busy)
        {
            using var ask = new ConfirmDialog("Зупинити й закрити?",
                _state == State.Installing
                    ? "Іде установка інструментів. Уже завантажене не пропаде — наступного разу установка продовжиться."
                    : "Іде збірка прошивки. Якщо закрити зараз, наступна збірка піде з нуля — довше, ніж зазвичай.",
                "Зупинити й закрити", "Працювати далі");
            if (ask.ShowDialog(this) != DialogResult.OK) { e.Cancel = true; return; }
            Stop();
        }
        _searchCts?.Cancel();
        var b = WindowState == FormWindowState.Normal ? Bounds : RestoreBounds;
        _settings.WindowX = b.X; _settings.WindowY = b.Y; _settings.WindowWidth = b.Width; _settings.WindowHeight = b.Height;
        _settings.Save();
        Log.Write("Вихід");
        base.OnFormClosing(e);
    }

    protected override void OnDpiChanged(DpiChangedEventArgs e)
    {
        base.OnDpiChanged(e);
        MinimumSize = new Size(S(640), S(640));
        foreach (var c in new Control[] { _header, _tools, _firmware, _log, _radio }) (c as IFonts)?.ApplyFonts();
        LayoutAll();
    }

    // =====================================================================
    //  Картки
    // =====================================================================

    interface IFonts { void ApplyFonts(); }

    /// Шапка: значок, назва, тека проєкту й «Змінити…».
    sealed class Header : Panel, IFonts
    {
        readonly BuilderForm _f;
        readonly PictureBox _icon = new() { SizeMode = PictureBoxSizeMode.Zoom, BackColor = Theme.Bg };
        readonly Label _title = new() { AutoSize = false, UseMnemonic = false, ForeColor = Theme.Accent, BackColor = Theme.Bg, Text = "Збірка ПОТУЖНОГО РАДІО" };
        readonly Label _path = new() { AutoSize = false, UseMnemonic = false, BackColor = Theme.Bg, AutoEllipsis = true };
        readonly LinkLabel _change = new() { AutoSize = true, UseMnemonic = false, BackColor = Theme.Bg, Text = "Змінити…", LinkColor = Theme.Accent, ActiveLinkColor = Theme.AccentHover, VisitedLinkColor = Theme.Accent };

        public Header(BuilderForm f)
        {
            _f = f;
            BackColor = Theme.Bg;
            try
            {
                using var s = typeof(BuilderForm).Assembly.GetManifestResourceStream("AppIcon.ico");
                if (s != null) _icon.Image = new Icon(s, 256, 256).ToBitmap();
            }
            catch { }
            Controls.AddRange(new Control[] { _icon, _title, _path, _change });
            _change.LinkClicked += (_, _) => _f.ChooseProject();
            ApplyFonts();
        }

        int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);

        public void ApplyFonts()
        {
            _title.Font = Theme.F(DeviceDpi, 20f, FontStyle.Bold);
            _path.Font = Theme.F(DeviceDpi, 12.5f);
            _change.Font = Theme.F(DeviceDpi, 12.5f);
        }

        public void Sync(Project? p)
        {
            _path.Text = p != null ? p.Root : "Теку проєкту не знайдено";
            _path.ForeColor = p != null ? Theme.Muted : Theme.Bad;
        }

        public void Relayout(int w)
        {
            var icon = S(56);
            _icon.SetBounds(0, 0, icon, icon);
            var x = icon + S(14);
            var th = TextRenderer.MeasureText("Зб", _title.Font).Height;
            _title.SetBounds(x, S(4), w - x, th);
            var ch = _change.PreferredSize;
            var ph = TextRenderer.MeasureText("Аg", _path.Font).Height;
            var pw = Math.Min(TextRenderer.MeasureText(_path.Text, _path.Font).Width + S(6), w - x - ch.Width - S(8));
            _path.SetBounds(x, S(8) + th, Math.Max(0, pw), ph);
            _change.Location = new Point(x + Math.Max(0, pw) + S(6), S(8) + th + (ph - ch.Height) / 2);
            SetBounds(Left, Top, w, icon);
        }
    }

    sealed class ToolsCard : Card, IFonts
    {
        readonly BuilderForm _f;
        readonly Label _text;
        readonly Label _phase;
        readonly Label _detail;
        readonly ProgressLine _bar = new();
        readonly FlatButton _install;
        readonly Label _error;
        bool _running;

        public string Error { get => _error.Text; set => _error.Text = value; }

        public ToolsCard(BuilderForm f) : base("Інструменти для збірки")
        {
            _f = f;
            _text = MakeLabel(13f, Theme.Muted);
            _text.Text = "Прошивка збирається прямо на цьому комп'ютері. Для цього потрібні arduino-cli і ядро ESP32 — " +
                         "близько 1 ГБ завантаження й 4–5 ГБ на диску. Це робиться один раз; далі кожна збірка бере їх звідси.\n" +
                         $"Куди: {Tools.Root}";
            _phase = MakeLabel(13.5f, Theme.Text, FontStyle.Bold);
            _detail = MakeLabel(12.5f, Theme.Muted);
            _install = MakeButton("Встановити інструменти", accent: true);
            _error = MakeLabel(13f, Theme.Bad);
            _install.Click += (_, _) => _f.InstallTools();
            Controls.AddRange(new Control[] { _text, _phase, _detail, _bar, _install, _error });
        }

        public void ApplyFonts()
        {
            _text.Font = Theme.F(DeviceDpi, 13f);
            _phase.Font = Theme.F(DeviceDpi, 13.5f, FontStyle.Bold);
            _detail.Font = Theme.F(DeviceDpi, 12.5f);
            _error.Font = Theme.F(DeviceDpi, 13f);
        }

        public void Sync(bool running)
        {
            _running = running;
            _install.Text = running ? "Зупинити" : Tools.CliReady ? "Доставити інструменти" : "Встановити інструменти";
            _install.Accent = !running;
            _install.Invalidate();
        }

        public void SetProgress(string phase, double? frac, string detail)
        {
            _phase.Text = phase;
            _detail.Text = detail;
            _bar.Value = frac;
            _f.LayoutAll();
        }

        protected override int LayoutContent(int w)
        {
            var inner = w - 2 * Pad;
            var y = Top0;
            var th = TextHeight(_text, inner);
            _text.SetBounds(Pad, y, inner, th); y += th + S(12);

            _phase.Visible = _detail.Visible = _bar.Visible = _running;
            if (_running)
            {
                var ph = TextHeight(_phase, inner);
                _phase.SetBounds(Pad, y, inner, ph); y += ph + S(6);
                _bar.SetBounds(Pad, y, inner, S(6)); y += S(6) + S(6);
                var dh = Math.Max(TextHeight(_detail, inner), S(16));
                _detail.SetBounds(Pad, y, inner, dh); y += dh + S(10);
            }
            var bw = _install.PreferredWidthFor();
            _install.SetBounds(Pad, y, bw, S(36)); y += S(36);
            _error.Visible = _error.Text.Length > 0;
            if (_error.Visible)
            {
                y += S(10);
                var eh = TextHeight(_error, inner);
                _error.SetBounds(Pad, y, inner, eh); y += eh;
            }
            return y + Pad;
        }
    }

    sealed class FirmwareCard : Card, IFonts
    {
        readonly BuilderForm _f;
        readonly Label _verLabel;
        readonly TextBox _version = new() { BorderStyle = BorderStyle.FixedSingle, BackColor = Theme.Bg, ForeColor = Theme.Text, TextAlign = HorizontalAlignment.Center };
        readonly FlatButton _next = MakeButton("Наступна");
        readonly FlatButton _build = MakeButton("Зібрати", accent: true);
        readonly Label _verWarn;
        readonly Label _phase;
        readonly Label _timer;
        readonly ProgressLine _bar = new();
        readonly Label _hint;
        readonly Label _result;
        readonly Panel _divider = new() { BackColor = Theme.Line };
        readonly Label _fwName;
        readonly Label _fwSummary;
        readonly Label _fwNote;
        readonly FlatButton _reveal = MakeButton("Показати в Провіднику");
        readonly Label _fwNone;
        readonly LinkLabel _toggle = new() { AutoSize = true, UseMnemonic = false, BackColor = Theme.Panel, LinkColor = Theme.Accent, ActiveLinkColor = Theme.AccentHover, VisitedLinkColor = Theme.Accent };

        public string Version { get => _version.Text; set => _version.Text = value; }

        public FirmwareCard(BuilderForm f) : base("Прошивка")
        {
            _f = f;
            _verLabel = MakeLabel(14f, Theme.Muted);
            _verLabel.Text = "Версія";
            _verLabel.TextAlign = ContentAlignment.MiddleLeft;
            _verWarn = MakeLabel(12.5f, Theme.Bad);
            _verWarn.Text = "Номер версії — цифри через крапку, наприклад 1.1";
            _phase = MakeLabel(13.5f, Theme.Text, FontStyle.Bold);
            _timer = MakeLabel(13.5f, Theme.Muted);
            _timer.TextAlign = ContentAlignment.TopRight;
            _hint = MakeLabel(12.5f, Theme.Muted);
            _hint.Text = "Перша збірка — 10–15 хвилин, далі — 3–5; якщо змінено спільні файли, знову довше. Радіо тим часом грає як грало.";
            _result = MakeLabel(13.5f, Theme.Accent, FontStyle.Bold);
            _fwName = MakeLabel(13.5f, Theme.Text, FontStyle.Bold);
            _fwName.Text = "PotuzhneRadio-ES3C28P-update.bin";
            _fwSummary = MakeLabel(12.5f, Theme.Muted);
            _fwNote = MakeLabel(12.5f, Theme.Muted);
            _fwNote.Text = "Цей файл — для оновлення через Wi-Fi: тут нижче, або на сторінці радіо в розділі «Оновлення». " +
                           "Поруч у firmware — повний образ для кабелю й файли сторінки (web).";
            _fwNone = MakeLabel(12.5f, Theme.Muted);
            _fwNone.Text = "Готового файлу ще немає — натисніть «Зібрати».";
            ApplyFonts();

            _version.TextChanged += (_, _) => Sync();
            _next.Click += (_, _) => _f.BumpVersion();
            _build.Click += (_, _) => _f.Build();
            _reveal.Click += (_, _) => _f.RevealFirmware();
            _toggle.LinkClicked += (_, _) => _f.ToggleLog();
            Controls.AddRange(new Control[] { _verLabel, _version, _next, _build, _verWarn, _phase, _timer, _bar, _hint, _result,
                                              _divider, _fwName, _fwSummary, _fwNote, _reveal, _fwNone, _toggle });
        }

        public void ApplyFonts()
        {
            _version.Font = Theme.F(DeviceDpi, 15f);
            _toggle.Font = Theme.F(DeviceDpi, 12.5f);
        }

        public void UpdateTimer()
        {
            _timer.Text = Fmt.Elapsed(DateTime.Now - _f._started);
        }

        public void Sync()
        {
            var st = _f._state;
            var building = st == State.Building;
            _build.Text = building ? "Зупинити" : "Зібрати";
            _build.Accent = !building;
            _build.Enabled = building || (_f._project != null && Fmt.VersionValid(_version.Text.Trim()) && st != State.Installing);
            _build.Invalidate();
            _next.Enabled = _version.Enabled = !_f.Busy;
            _phase.Text = _f._phase;
            UpdateTimer();
            switch (st)
            {
                case State.Ok:
                    _result.Text = $"✓ Зібрано за {Fmt.Elapsed(_f._took)}";
                    _result.ForeColor = Theme.Accent;
                    break;
                case State.Failed:
                    _result.Text = "⚠ " + _f._failText;
                    _result.ForeColor = Theme.Bad;
                    break;
                case State.Cancelled:
                    _result.Text = _f._phase == "Зупинено" ? "Збірку зупинено. Наступна піде з нуля — довше, ніж зазвичай." : "";
                    _result.ForeColor = Theme.Muted;
                    break;
                default:
                    _result.Text = "";
                    break;
            }
            _fwSummary.Text = _f._fw?.Summary ?? "";
            _toggle.Text = _f._showLog ? "Сховати журнал" : "Показати журнал";
            _f.LayoutAll();
        }

        protected override int LayoutContent(int w)
        {
            var inner = w - 2 * Pad;
            var y = Top0;
            var rowH = S(36);
            var vl = TextRenderer.MeasureText("Версія", _verLabel.Font).Width + S(4);
            _verLabel.SetBounds(Pad, y, vl, rowH);
            var tbH = _version.PreferredHeight;
            _version.SetBounds(Pad + vl + S(8), y + (rowH - tbH) / 2, S(90), tbH);
            var nx = Pad + vl + S(8) + S(90) + S(10);
            var nw = _next.PreferredWidthFor();
            _next.SetBounds(nx, y, nw, rowH);
            var bw = Math.Max(_build.PreferredWidthFor(), S(120));
            _build.SetBounds(w - Pad - bw, y, bw, rowH);
            y += rowH;

            _verWarn.Visible = !Fmt.VersionValid(_version.Text.Trim());
            if (_verWarn.Visible)
            {
                y += S(6);
                var h = TextHeight(_verWarn, inner);
                _verWarn.SetBounds(Pad, y, inner, h); y += h;
            }

            var building = _f._state == State.Building;
            _phase.Visible = _timer.Visible = _bar.Visible = _hint.Visible = building;
            _result.Visible = !building && _result.Text.Length > 0;
            if (building)
            {
                y += S(14);
                var ph = TextHeight(_phase, inner - S(60));
                _phase.SetBounds(Pad, y, inner - S(60), Math.Max(ph, S(18)));
                _timer.SetBounds(w - Pad - S(60), y, S(60), Math.Max(ph, S(18)));
                y += Math.Max(ph, S(18)) + S(6);
                _bar.Value = null;
                _bar.SetBounds(Pad, y, inner, S(6)); y += S(12);
                var hh = TextHeight(_hint, inner);
                _hint.SetBounds(Pad, y, inner, hh); y += hh;
            }
            else if (_result.Visible)
            {
                y += S(14);
                var rh = TextHeight(_result, inner);
                _result.SetBounds(Pad, y, inner, rh); y += rh;
            }

            y += S(14);
            _divider.SetBounds(Pad, y, inner, 1);
            y += S(14);

            var has = _f._fw != null;
            _fwName.Visible = _fwSummary.Visible = _fwNote.Visible = _reveal.Visible = has;
            _fwNone.Visible = !has;
            if (has)
            {
                var rw = _reveal.PreferredWidthFor();
                var tw = inner - rw - S(14);
                var h1 = TextHeight(_fwName, tw);
                _fwName.SetBounds(Pad, y, tw, h1);
                _reveal.SetBounds(w - Pad - rw, y, rw, S(34));
                var h2 = TextHeight(_fwSummary, tw);
                _fwSummary.SetBounds(Pad, y + h1 + S(2), tw, h2);
                var h3 = TextHeight(_fwNote, tw);
                _fwNote.SetBounds(Pad, y + h1 + h2 + S(6), tw, h3);
                y += Math.Max(h1 + h2 + h3 + S(6), S(34));
            }
            else
            {
                var h = TextHeight(_fwNone, inner);
                _fwNone.SetBounds(Pad, y, inner, h); y += h;
            }

            y += S(8);
            var ts = _toggle.PreferredSize;
            _toggle.Location = new Point(w - Pad - ts.Width, y);
            y += ts.Height;
            return y + Pad - S(4);
        }
    }

    sealed class LogCard : Card
    {
        readonly RichTextBox _box = new()
        {
            ReadOnly = true,
            BorderStyle = BorderStyle.None,
            BackColor = Theme.Bg,
            ForeColor = Theme.Muted,
            WordWrap = false,
            ScrollBars = RichTextBoxScrollBars.Both,
            DetectUrls = false,
        };
        int _lines;

        public LogCard() : base("Журнал збірки")
        {
            Controls.Add(_box);
            _box.Font = new Font("Consolas", 11f * DeviceDpi / 96f, GraphicsUnit.Pixel);
            _box.HandleCreated += (_, _) => Theme.DarkScrollbars(_box);
        }

        public void Clear()
        {
            _box.Clear();
            _lines = 0;
        }

        public void Append(string line)
        {
            // Не більше 1500 рядків: довший журнал гальмує, а повний лежить у файлі.
            if (_lines >= 1500)
            {
                var cut = _box.GetFirstCharIndexFromLine(300);
                if (cut > 0)
                {
                    _box.Select(0, cut);
                    _box.ReadOnly = false;
                    _box.SelectedText = "";
                    _box.ReadOnly = true;
                    _lines -= 300;
                }
            }
            var color = line.StartsWith(">>>", StringComparison.Ordinal) ? Theme.Accent
                      : line.Contains("error", StringComparison.OrdinalIgnoreCase) || line.Contains("помилк", StringComparison.OrdinalIgnoreCase) ? Theme.Bad
                      : Theme.Muted;
            _box.SelectionStart = _box.TextLength;
            _box.SelectionLength = 0;
            _box.SelectionColor = color;
            _box.AppendText(line + "\n");
            _box.ScrollToCaret();
            _lines++;
        }

        protected override int LayoutContent(int w)
        {
            _box.SetBounds(Pad, Top0, w - 2 * Pad, S(240));
            return Top0 + S(240) + Pad;
        }
    }

    sealed class RadioCard : Card, IFonts
    {
        readonly BuilderForm _f;
        readonly Spinner _spin = new() { BackColor = Theme.Panel };
        readonly Label _search;
        readonly FlatButton _again = MakeButton("Шукати");
        readonly FlatButton _manual = MakeButton("Ввести адресу…");
        readonly RadioListBox _list = new() { BackColor = Theme.Panel };
        readonly FlatButton _upFw = MakeButton("Залити прошивку", accent: true);
        readonly FlatButton _upWeb = MakeButton("Залити сторінку");
        readonly ProgressLine _bar = new();
        readonly Label _status;
        readonly Label _error;
        readonly Label _note;

        public RadioInfo? Selected => _list.SelectedItem as RadioInfo;
        public string Status { get => _status.Text; set => _status.Text = value; }
        public string Error { get => _error.Text; set => _error.Text = value; }
        public double? Progress { get; set; }

        public RadioCard(BuilderForm f) : base("Оновити радіо по Wi-Fi")
        {
            _f = f;
            _search = MakeLabel(13f, Theme.Muted);
            _status = MakeLabel(13f, Theme.Text);
            _error = MakeLabel(13f, Theme.Bad);
            _note = MakeLabel(12.5f, Theme.Muted);
            _note.Text = "На час оновлення звук на радіо зупиниться. Станції, мережі, обране й налаштування лишаються.";
            _list.Details = r => _f.DescribeRadio(r);
            _list.SelectedIndexChanged += (_, _) => Sync();
            _again.Click += (_, _) => _f.StartSearch();
            _manual.Click += (_, _) => _f.AskAddress();
            _upFw.Click += (_, _) => _f.UploadFirmware();
            _upWeb.Click += (_, _) => _f.UploadWeb();
            Controls.AddRange(new Control[] { _spin, _search, _again, _manual, _list, _upFw, _upWeb, _bar, _status, _error, _note });
        }

        public void ApplyFonts() { }

        public void AddRadio(RadioInfo r, bool select = false)
        {
            for (var i = 0; i < _list.Items.Count; i++)
            {
                if (_list.Items[i] is RadioInfo x && x.Address == r.Address)
                {
                    _list.Items[i] = r;
                    if (select) _list.SelectedIndex = i;
                    Sync();
                    return;
                }
            }
            _list.Items.Add(r);
            if (select || _list.SelectedIndex < 0) _list.SelectedItem = r;
            Sync();
        }

        public void Sync()
        {
            _spin.Spinning = _f._searching;
            _search.Text = _f._searching ? "Шукаю радіо в мережі…"
                         : _list.Items.Count == 0 ? "Радіо в мережі не знайдено."
                         : _list.Items.Count == 1 ? "Знайдено одне радіо." : $"Знайдено радіо: {_list.Items.Count}.";
            _again.Enabled = !_f._searching;
            _upFw.Enabled = _f.CanUploadFirmware;
            _upWeb.Enabled = _f.CanUploadWeb;
            _bar.Value = Progress;
            _list.Invalidate();
            _f.LayoutAll();
        }

        protected override int LayoutContent(int w)
        {
            var inner = w - 2 * Pad;
            var y = Top0;
            var rowH = S(34);
            var mw = _manual.PreferredWidthFor();
            var aw = _again.PreferredWidthFor();
            _manual.SetBounds(w - Pad - mw, y, mw, rowH);
            _again.SetBounds(w - Pad - mw - S(8) - aw, y, aw, rowH);
            var sx = Pad;
            if (_spin.Spinning)
            {
                _spin.SetBounds(Pad, y + (rowH - S(16)) / 2, S(16), S(16));
                sx += S(24);
            }
            _search.SetBounds(sx, y, Math.Max(0, w - Pad - mw - S(8) - aw - sx - S(8)), rowH);
            _search.TextAlign = ContentAlignment.MiddleLeft;
            y += rowH;

            var rows = Math.Min(_list.Items.Count, 3);
            _list.Visible = rows > 0;
            if (rows > 0)
            {
                y += S(10);
                var lh = rows * _list.RowHeight + (_list.Items.Count > 3 ? _list.RowHeight / 2 : S(4));
                _list.SetBounds(Pad, y, inner, lh); y += lh;
            }

            y += S(12);
            var w1 = _upFw.PreferredWidthFor();
            var w2 = _upWeb.PreferredWidthFor();
            _upFw.SetBounds(Pad, y, w1, S(36));
            _upWeb.SetBounds(Pad + w1 + S(10), y, w2, S(36));
            y += S(36);

            _bar.Visible = _f._uploading;
            if (_bar.Visible)
            {
                y += S(10);
                _bar.SetBounds(Pad, y, inner, S(6)); y += S(6);
            }
            foreach (var l in new[] { _status, _error, _note })
            {
                l.Visible = l.Text.Length > 0;
                if (!l.Visible) continue;
                y += S(8);
                var h = TextHeight(l, inner);
                l.SetBounds(Pad, y, inner, h); y += h;
            }
            return y + Pad;
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { _tick.Dispose(); _lock?.Dispose(); }
        base.Dispose(disposing);
    }
}
