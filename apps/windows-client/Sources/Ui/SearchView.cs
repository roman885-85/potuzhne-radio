namespace PotuzhneRadio;

/// <summary>
/// Екран пошуку: логотип, «ПОТУЖНЕ РАДІО», рядок стану, список знайдених і
/// три кнопки. Сам нічого не шукає — лише показує те, що йому передає
/// MainForm, і повідомляє, що натиснули.
/// </summary>
sealed class SearchView : UserControl
{
    readonly LogoBox _logo = new();
    readonly Label _title = MakeLabel(Theme.Text);
    readonly Spinner _spinner = new();
    readonly Label _status = MakeLabel(Theme.Muted);
    readonly Label _detail = MakeLabel(Theme.Dim);
    readonly Banner _banner = new() { Visible = false };
    readonly RadioListBox _list = new() { Visible = false };
    readonly FlatButton _connect = new() { Text = "Підключитися", Accent = true, Enabled = false };
    readonly FlatButton _again = new() { Text = "Шукати ще раз" };
    readonly FlatButton _manual = new() { Text = "Ввести адресу…" };

    bool _busy;

    public event Action<RadioInfo>? ConnectRequested;
    public event Action? SearchRequested;
    public event Action? ManualRequested;

    public SearchView()
    {
        SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint, true);
        AutoScaleMode = AutoScaleMode.None;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;

        _title.Text = "ПОТУЖНЕ РАДІО";
        _title.TextAlign = ContentAlignment.MiddleCenter;
        _status.TextAlign = ContentAlignment.TopCenter;
        _detail.TextAlign = ContentAlignment.TopCenter;

        Controls.AddRange(new Control[] { _logo, _title, _spinner, _status, _detail, _banner, _list, _connect, _again, _manual });

        _list.SelectedIndexChanged += (_, _) => _connect.Enabled = _list.SelectedItem is RadioInfo;
        _list.DoubleClick += (_, _) => { if (_list.SelectedItem is RadioInfo r) ConnectRequested?.Invoke(r); };
        _list.KeyDown += (_, e) =>
        {
            if (e.KeyCode == Keys.Enter && _list.SelectedItem is RadioInfo r)
            {
                e.Handled = e.SuppressKeyPress = true;
                ConnectRequested?.Invoke(r);
            }
        };
        _connect.Click += (_, _) => { if (_list.SelectedItem is RadioInfo r) ConnectRequested?.Invoke(r); };
        _again.Click += (_, _) => SearchRequested?.Invoke();
        _manual.Click += (_, _) => ManualRequested?.Invoke();

        ApplyFonts();
    }

    static Label MakeLabel(Color color) => new()
    {
        AutoSize = false,
        ForeColor = color,
        BackColor = Theme.Bg,
        UseMnemonic = false,
    };

    int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);

    void ApplyFonts()
    {
        _title.Font = Theme.F(DeviceDpi, 28f, FontStyle.Bold);
        _status.Font = Theme.F(DeviceDpi, 15f);
        _detail.Font = Theme.F(DeviceDpi, 12.5f);
        foreach (var b in new[] { _connect, _again, _manual }) b.FontPx = 14f;
    }

    protected override void OnDpiChangedAfterParent(EventArgs e)
    {
        base.OnDpiChangedAfterParent(e);
        ApplyFonts();
        PerformLayout();
        Invalidate(true);
    }

    // ---------------------------------------------------------------------
    //  Стан
    // ---------------------------------------------------------------------

    public IReadOnlyList<RadioInfo> Radios => _list.Items.Cast<RadioInfo>().ToList();

    /// Почати показ пошуку: список порожній, кільце крутиться.
    public void BeginSearch(string status)
    {
        _list.Items.Clear();
        _connect.Enabled = false;
        SetStatus(status, busy: true);
        _detail.Text = "";
        PerformLayout();
    }

    public void SetStatus(string text, bool busy)
    {
        _busy = busy;
        _status.Text = text;
        _spinner.Spinning = busy;
        _again.Enabled = true;
        PerformLayout();
    }

    public void SetDetail(string text)
    {
        if (_detail.Text == text) return;
        _detail.Text = text;
        PerformLayout();
    }

    /// Повідомлення над списком; null — сховати.
    public void SetMessage(string? text)
    {
        _banner.Text = text ?? "";
        _banner.Visible = !string.IsNullOrEmpty(text);
        PerformLayout();
    }

    public void AddRadio(RadioInfo r, string? preferredHost)
    {
        foreach (var item in _list.Items)
            if (item is RadioInfo x && x.Address == r.Address) return;
        _list.Items.Add(r);
        // Перше знайдене — виділене одразу; а якщо знайшлось те, до якого
        // підключались минулого разу, — виділяємо саме його.
        if (_list.SelectedIndex < 0 ||
            (!string.IsNullOrEmpty(preferredHost) && r.Host.Equals(preferredHost, StringComparison.OrdinalIgnoreCase)))
            _list.SelectedItem = r;
        PerformLayout();
    }

    public void FocusBest()
    {
        if (_list.Items.Count > 0) _list.Focus();
        else _again.Focus();
    }

    public IButtonControl AcceptButton => _connect;

    // ---------------------------------------------------------------------
    //  Розкладка: одна колонка по центру, вертикально — посередині вікна
    // ---------------------------------------------------------------------

    protected override void OnLayout(LayoutEventArgs e)
    {
        base.OnLayout(e);
        if (Width <= 0 || Height <= 0) return;

        var colW = Math.Min(S(560), Width - S(48));
        var colX = (Width - colW) / 2;

        var logo = S(92);
        var titleH = TextRenderer.MeasureText("ПОТУЖНЕ РАДІО", _title.Font).Height + S(4);

        // Рядок стану може бути довгим («Радіо не знайдено. Перевірте…») — переносимо.
        var spin = S(18);
        var statusMaxW = colW - (_busy ? spin + S(10) : 0);
        var statusSize = TextRenderer.MeasureText(_status.Text.Length > 0 ? _status.Text : " ", _status.Font,
            new Size(statusMaxW, 0), TextFormatFlags.WordBreak | TextFormatFlags.NoPrefix);
        var statusH = Math.Max(statusSize.Height, spin);
        var detailH = _detail.Text.Length > 0 ? TextRenderer.MeasureText(_detail.Text, _detail.Font).Height : 0;
        var bannerH = _banner.Visible ? _banner.HeightFor(colW) : 0;
        var rows = Math.Min(_list.Items.Count, 3);
        // Запас у кілька пікселів: рівно впритул ListBox уже показує смугу прокрутки.
        var listH = rows > 0 ? rows * _list.RowHeight + (_list.Items.Count > 3 ? _list.RowHeight / 2 : S(4)) : 0;
        var btnH = S(40);

        var total = logo + S(18) + titleH + S(6) + statusH + (detailH > 0 ? S(4) + detailH : 0)
                  + (bannerH > 0 ? S(20) + bannerH : 0) + (listH > 0 ? S(18) + listH : 0) + S(24) + btnH;
        var y = Math.Max(S(20), (Height - total) / 2 - S(10));

        _logo.SetBounds((Width - logo) / 2, y, logo, logo);
        y += logo + S(18);
        _title.SetBounds(colX, y, colW, titleH);
        y += titleH + S(6);

        // Кільце — ліворуч від тексту стану, разом вони по центру.
        var textW = Math.Min(statusSize.Width, statusMaxW);
        if (_busy)
        {
            var rowW = spin + S(10) + textW;
            var sx = (Width - rowW) / 2;
            _spinner.SetBounds(sx, y + (Math.Min(statusH, TextRenderer.MeasureText("Ag", _status.Font).Height) - spin) / 2, spin, spin);
            _status.SetBounds(sx + spin + S(10), y, textW + S(4), statusH);
            _status.TextAlign = ContentAlignment.TopLeft;
        }
        else
        {
            _status.SetBounds(colX, y, colW, statusH);
            _status.TextAlign = ContentAlignment.TopCenter;
        }
        y += statusH;

        _detail.Visible = detailH > 0;
        if (detailH > 0)
        {
            y += S(4);
            _detail.SetBounds(colX, y, colW, detailH);
            y += detailH;
        }

        if (bannerH > 0)
        {
            y += S(20);
            _banner.SetBounds(colX, y, colW, bannerH);
            y += bannerH;
        }

        _list.Visible = listH > 0;
        if (listH > 0)
        {
            y += S(18);
            _list.SetBounds(colX, y, colW, listH);
            y += listH;
        }

        y += S(24);
        var buttons = new[] { _connect, _again, _manual };
        var widths = buttons.Select(b => b.PreferredWidthFor()).ToArray();
        var gap = S(10);
        var rowTotal = widths.Sum() + gap * (buttons.Length - 1);
        if (rowTotal > colW)
        {
            // Вузьке вікно — ділимо ширину порівну.
            var w = (colW - gap * (buttons.Length - 1)) / buttons.Length;
            widths = buttons.Select(_ => w).ToArray();
            rowTotal = colW;
        }
        var bx = (Width - rowTotal) / 2;
        for (var i = 0; i < buttons.Length; i++)
        {
            buttons[i].SetBounds(bx, y, widths[i], btnH);
            bx += widths[i] + gap;
        }
    }
}
