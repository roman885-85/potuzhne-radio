namespace PotuzhneRadio;

/// <summary>
/// «Ввести адресу…»: IP або ім'я радіо, коли саме воно не знаходиться
/// (буває в мережах, де роутер відділяє Wi-Fi-пристрої один від одного).
/// </summary>
sealed class AddressDialog : Form
{
    readonly Label _prompt = new() { AutoSize = false, UseMnemonic = false, ForeColor = Theme.Text, BackColor = Theme.Bg };
    readonly TextBox _input = new() { BorderStyle = BorderStyle.FixedSingle, BackColor = Theme.Panel, ForeColor = Theme.Text };
    readonly Label _hint = new() { AutoSize = false, UseMnemonic = false, ForeColor = Theme.Muted, BackColor = Theme.Bg };
    readonly FlatButton _ok = new() { Text = "Підключитися", Accent = true };
    readonly FlatButton _cancel = new() { Text = "Скасувати" };

    public string Address => _input.Text.Trim();

    public AddressDialog(string? initial)
    {
        Text = "Адреса радіо";
        AutoScaleMode = AutoScaleMode.None;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterParent;
        MinimizeBox = false;
        MaximizeBox = false;
        ShowInTaskbar = false;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        try
        {
            using var s = typeof(AddressDialog).Assembly.GetManifestResourceStream("AppIcon.ico");
            if (s != null) Icon = new Icon(s);
        }
        catch { }

        _prompt.Text = "IP-адреса або ім'я радіо в мережі:";
        _hint.Text = "Наприклад: 192.168.1.32 або potuzhne-c447d4. Адресу видно в меню самого радіо, на сторінці «інформація», рядок «ip».";
        _input.Text = initial ?? "";

        Controls.AddRange(new Control[] { _prompt, _input, _hint, _ok, _cancel });
        AcceptButton = _ok;
        CancelButton = _cancel;
        _ok.Click += (_, _) =>
        {
            if (Address.Length == 0) { _input.Focus(); return; }
            DialogResult = DialogResult.OK;
        };
        _cancel.Click += (_, _) => DialogResult = DialogResult.Cancel;

        Relayout();
    }

    int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);

    void Relayout()
    {
        _prompt.Font = Theme.F(DeviceDpi, 14f, FontStyle.Bold);
        _input.Font = Theme.F(DeviceDpi, 16f);
        _hint.Font = Theme.F(DeviceDpi, 12.5f);
        _ok.FontPx = _cancel.FontPx = 14f;

        var w = S(440);
        var pad = S(22);
        var y = pad;
        var ph = TextRenderer.MeasureText(_prompt.Text, _prompt.Font).Height;
        _prompt.SetBounds(pad, y, w, ph); y += ph + S(8);
        _input.SetBounds(pad, y, w, _input.PreferredHeight); y += _input.PreferredHeight + S(8);
        var hh = TextRenderer.MeasureText(_hint.Text, _hint.Font, new Size(w, 0), TextFormatFlags.WordBreak).Height + S(4);
        _hint.SetBounds(pad, y, w, hh); y += hh + S(18);
        var w1 = _ok.PreferredWidthFor();
        var w2 = _cancel.PreferredWidthFor();
        _cancel.SetBounds(pad + w - w2, y, w2, S(38));
        _ok.SetBounds(pad + w - w2 - S(10) - w1, y, w1, S(38));
        y += S(38) + pad;
        ClientSize = new Size(w + 2 * pad, y);
    }

    protected override void OnDpiChanged(DpiChangedEventArgs e)
    {
        base.OnDpiChanged(e);
        Relayout();
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        Theme.DarkTitleBar(this);
    }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        _input.Focus();
        _input.SelectAll();
    }
}
