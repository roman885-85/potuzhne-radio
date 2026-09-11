using System.ComponentModel;
using System.Drawing.Drawing2D;

namespace PotuzhneRadio;

/// <summary>
/// Картка з заголовком великими літерами — як у Mac-версії: панель #111,
/// тонка рамка, скруглення 12. Вміст розкладає підклас у LayoutContent.
/// </summary>
abstract class Card : Panel
{
    protected Card(string title)
    {
        Title = title;
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Panel;
        ForeColor = Theme.Text;
    }

    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    public string Title { get; set; }

    protected int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);
    protected int Pad => S(16);
    protected int Top0 => S(42);

    /// Розкласти вміст на ширину width; повернути потрібну висоту.
    protected abstract int LayoutContent(int width);

    public void Relayout(int width)
    {
        SuspendLayout();
        var h = LayoutContent(width);
        SetBounds(Left, Top, width, h);
        ResumeLayout(false);
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Theme.Bg);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var r = new RectangleF(0.5f, 0.5f, Width - 1f, Height - 1f);
        using (var path = Theme.Rounded(r, S(12)))
        {
            using var b = new SolidBrush(Theme.Panel);
            g.FillPath(b, path);
            using var p = new Pen(Theme.Line, 1f);
            g.DrawPath(p, path);
        }
        TextRenderer.DrawText(g, Title.ToUpperInvariant(), Theme.F(DeviceDpi, 12f, FontStyle.Bold),
            new Point(Pad, S(15)), Theme.Muted, TextFormatFlags.NoPrefix);
    }

    // Помічники для підкласів.

    protected Label MakeLabel(float px, Color color, FontStyle style = FontStyle.Regular) => new()
    {
        AutoSize = false,
        UseMnemonic = false,
        ForeColor = color,
        BackColor = Theme.Panel,
        Font = Theme.F(DeviceDpi, px, style),
    };

    protected int TextHeight(Label l, int width) =>
        string.IsNullOrEmpty(l.Text) ? 0 :
        TextRenderer.MeasureText(l.Text, l.Font, new Size(width, 0), TextFormatFlags.WordBreak | TextFormatFlags.NoPrefix).Height + S(2);

    protected static FlatButton MakeButton(string text, bool accent = false, float px = 13.5f) => new()
    {
        Text = text,
        Accent = accent,
        FontPx = px,
        BackColor = Theme.Panel,
    };
}

/// <summary>
/// Жовта смужка прогресу. Value = null — «невідомо скільки»: по смужці
/// бігає відрізок, як у macOS.
/// </summary>
sealed class ProgressLine : Control
{
    readonly System.Windows.Forms.Timer _timer = new() { Interval = 30 };
    double? _value;
    float _phase;

    public ProgressLine()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Panel;
        _timer.Tick += (_, _) => { _phase = (_phase + 0.012f) % 1.4f; Invalidate(); };
    }

    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    public double? Value
    {
        get => _value;
        set
        {
            _value = value is null ? null : Math.Clamp(value.Value, 0, 1);
            _timer.Enabled = _value == null && Visible;
            Invalidate();
        }
    }

    protected override void OnVisibleChanged(EventArgs e)
    {
        base.OnVisibleChanged(e);
        _timer.Enabled = _value == null && Visible;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(BackColor);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var r = new RectangleF(0, 0, Width, Height);
        var rad = Height / 2f;
        using (var track = Theme.Rounded(r, rad))
        using (var b = new SolidBrush(Theme.Panel2))
            g.FillPath(b, track);
        RectangleF fill;
        if (_value is double v)
            fill = new RectangleF(0, 0, (float)(Width * v), Height);
        else
        {
            var w = Width * 0.3f;
            fill = new RectangleF(Width * (_phase - 0.3f), 0, w, Height);
            fill.Intersect(r);
        }
        if (fill.Width > 0.5f)
        {
            using var path = Theme.Rounded(fill, rad);
            using var b = new SolidBrush(Theme.Accent);
            g.FillPath(b, path);
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _timer.Dispose();
        base.Dispose(disposing);
    }
}

/// <summary>Підтвердження в стилі програми: заголовок, текст, «Оновити» / «Скасувати».</summary>
sealed class ConfirmDialog : Form
{
    public ConfirmDialog(string title, string text, string ok, string cancel = "Скасувати")
    {
        Text = "Збірка ПОТУЖНОГО РАДІО";
        AutoScaleMode = AutoScaleMode.None;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterParent;
        MinimizeBox = MaximizeBox = false;
        ShowInTaskbar = false;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;

        int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);
        var w = S(460);
        var pad = S(22);
        var head = new Label { AutoSize = false, UseMnemonic = false, ForeColor = Theme.Accent, BackColor = Theme.Bg, Font = Theme.F(DeviceDpi, 17f, FontStyle.Bold), Text = title };
        var body = new Label { AutoSize = false, UseMnemonic = false, ForeColor = Theme.Text, BackColor = Theme.Bg, Font = Theme.F(DeviceDpi, 14f), Text = text };
        var okBtn = new FlatButton { Text = ok, Accent = true, DialogResult = DialogResult.OK };
        var noBtn = new FlatButton { Text = cancel, DialogResult = DialogResult.Cancel };
        Controls.AddRange(new Control[] { head, body, okBtn, noBtn });
        // AcceptButton свідомо не задаємо: випадковий Enter не має заливати
        // прошивку. Esc — «Скасувати».
        CancelButton = noBtn;

        var y = pad;
        var hh = TextRenderer.MeasureText(title, head.Font, new Size(w, 0), TextFormatFlags.WordBreak).Height + S(2);
        head.SetBounds(pad, y, w, hh); y += hh + S(10);
        var bh = TextRenderer.MeasureText(text, body.Font, new Size(w, 0), TextFormatFlags.WordBreak).Height + S(4);
        body.SetBounds(pad, y, w, bh); y += bh + S(20);
        var w1 = okBtn.PreferredWidthFor();
        var w2 = noBtn.PreferredWidthFor();
        noBtn.SetBounds(pad + w - w2, y, w2, S(38));
        okBtn.SetBounds(pad + w - w2 - S(10) - w1, y, w1, S(38));
        ClientSize = new Size(w + 2 * pad, y + S(38) + pad);
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        Theme.DarkTitleBar(this);
    }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        // Фокус — на «Скасувати»: випадковий Enter не має заливати прошивку.
        (CancelButton as Control)?.Focus();
    }
}
