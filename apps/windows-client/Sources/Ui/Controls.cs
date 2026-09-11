using System.ComponentModel;
using System.Drawing.Drawing2D;

namespace PotuzhneRadio;

/// <summary>
/// Кнопка в стилі радіо: скруглена, акцентна — жовта з чорним текстом,
/// звичайна — темна панель зі світлим текстом. Похідна від Button, тож Enter,
/// пробіл, Tab, AcceptButton і читачі екрана працюють як у звичайної кнопки.
/// </summary>
sealed class FlatButton : Button
{
    bool _hover, _down;

    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    public bool Accent { get; set; }
    /// Розмір шрифту в пікселях при 100%.
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    public float FontPx { get; set; } = 14f;

    public FlatButton()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        FlatStyle = FlatStyle.Flat;
        FlatAppearance.BorderSize = 0;
        UseVisualStyleBackColor = false;
        UseMnemonic = false;
        Cursor = Cursors.Hand;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
    }

    int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);

    /// Ширина під текст із полями.
    public int PreferredWidthFor()
    {
        var text = TextRenderer.MeasureText(Text, Theme.F(DeviceDpi, FontPx, FontStyle.Bold));
        return Math.Max(S(96), text.Width + S(34));
    }

    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = false; _down = false; Invalidate(); base.OnMouseLeave(e); }
    protected override void OnMouseDown(MouseEventArgs e) { if (e.Button == MouseButtons.Left) { _down = true; Invalidate(); } base.OnMouseDown(e); }
    protected override void OnMouseUp(MouseEventArgs e) { _down = false; Invalidate(); base.OnMouseUp(e); }
    protected override void OnEnabledChanged(EventArgs e) { Invalidate(); base.OnEnabledChanged(e); }
    protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
    protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Parent?.BackColor ?? Theme.Bg);
        g.SmoothingMode = SmoothingMode.AntiAlias;

        Color fill, text, border;
        if (!Enabled)
        {
            fill = Accent ? Color.FromArgb(0x3a, 0x36, 0x1c) : Theme.Panel;
            text = Accent ? Color.FromArgb(0x80, 0x78, 0x40) : Theme.Dim;
            border = Accent ? fill : Theme.Line;
        }
        else if (Accent)
        {
            fill = _down ? Theme.AccentDown : _hover ? Theme.AccentHover : Theme.Accent;
            text = Theme.AccentText;
            border = fill;
        }
        else
        {
            fill = _down ? Color.FromArgb(0x26, 0x26, 0x26) : _hover ? Theme.Panel2 : Theme.Panel;
            text = Theme.Text;
            border = _hover ? Color.FromArgb(0x3a, 0x3a, 0x3a) : Theme.Line;
        }

        var r = new RectangleF(0.5f, 0.5f, Width - 1f, Height - 1f);
        using (var path = Theme.Rounded(r, S(9)))
        {
            using var b = new SolidBrush(fill);
            g.FillPath(b, path);
            using var p = new Pen(border, 1f);
            g.DrawPath(p, path);
        }
        // Фокус із клавіатури — жовта рамка (на жовтій кнопці — біла).
        if (Focused && ShowFocusCues)
        {
            var fr = RectangleF.Inflate(r, -S(2), -S(2));
            using var path = Theme.Rounded(fr, S(7));
            using var p = new Pen(Accent ? Color.White : Theme.Accent, Math.Max(1f, S(1.5f)));
            g.DrawPath(p, path);
        }
        TextRenderer.DrawText(g, Text, Theme.F(DeviceDpi, FontPx, FontStyle.Bold), ClientRectangle, text,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine |
            TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
    }
}

/// <summary>Жовте кільце, що обертається, поки йде пошук.</summary>
sealed class Spinner : Control
{
    readonly System.Windows.Forms.Timer _timer = new() { Interval = 40 };
    float _angle;

    public Spinner()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Bg;
        _timer.Tick += (_, _) => { _angle = (_angle + 12) % 360; Invalidate(); };
    }

    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    public bool Spinning
    {
        get => _timer.Enabled;
        set { _timer.Enabled = value; Visible = value; }
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(BackColor);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var w = Math.Max(2f, Width / 8f);
        var r = new RectangleF(w / 2 + 0.5f, w / 2 + 0.5f, Width - w - 1, Height - w - 1);
        using var track = new Pen(Theme.Panel2, w);
        g.DrawEllipse(track, r);
        using var pen = new Pen(Theme.Accent, w) { StartCap = LineCap.Round, EndCap = LineCap.Round };
        g.DrawArc(pen, r, _angle, 100);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _timer.Dispose();
        base.Dispose(disposing);
    }
}

/// <summary>Логотип на екрані пошуку: жовта антена на темному скругленому квадраті.</summary>
sealed class LogoBox : Control
{
    public LogoBox()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Bg;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(BackColor);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var r = new RectangleF(0.5f, 0.5f, Width - 1f, Height - 1f);
        using (var path = Theme.Rounded(r, Width * 0.22f))
        {
            using var b = new SolidBrush(Theme.Panel);
            g.FillPath(b, path);
            using var p = new Pen(Theme.Panel2, 1f);
            g.DrawPath(p, path);
        }
        var pad = Width * 0.14f;
        Antenna.Draw(g, RectangleF.Inflate(r, -pad, -pad), Theme.Accent);
    }
}

/// <summary>
/// Повідомлення над списком («Зв'язок з радіо втрачено» тощо): темна панель із
/// жовтою смужкою ліворуч, текст переноситься.
/// </summary>
sealed class Banner : Control
{
    public Banner()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        BackColor = Theme.Bg;
    }

    int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);
    Font TextFont => Theme.F(DeviceDpi, 14f);
    const TextFormatFlags Flags = TextFormatFlags.WordBreak | TextFormatFlags.NoPrefix | TextFormatFlags.Left | TextFormatFlags.VerticalCenter;

    /// Висота під текст при заданій ширині.
    public int HeightFor(int width)
    {
        var sz = TextRenderer.MeasureText(Text, TextFont, new Size(width - S(40), 0), Flags);
        return sz.Height + S(24);
    }

    protected override void OnTextChanged(EventArgs e) { Invalidate(); base.OnTextChanged(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Theme.Bg);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var r = new RectangleF(0.5f, 0.5f, Width - 1f, Height - 1f);
        using (var path = Theme.Rounded(r, S(10)))
        {
            using var b = new SolidBrush(Theme.Panel2);
            g.FillPath(b, path);
        }
        using (var bar = new SolidBrush(Theme.Accent))
            g.FillRectangle(bar, S(10), S(10), S(4), Height - S(20));
        var tr = new Rectangle(S(26), 0, Width - S(40), Height);
        TextRenderer.DrawText(g, Text, TextFont, tr, Theme.Text, Flags);
    }
}

/// <summary>
/// Список знайдених радіо: картка на кожне — «ПОТУЖНЕ РАДІО», під ним
/// «host · ip» і поточна станція. Звичайний ListBox, лише намальований
/// власноруч: виділення, стрілки, подвійний клац і прокрутка — його власні.
/// </summary>
sealed class RadioListBox : ListBox
{
    /// Третій рядок картки. Без нього — поточна станція (так у клієнті);
    /// програма збірки показує тут версію прошивки на радіо.
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    public Func<RadioInfo, (string text, bool accent)>? Details { get; set; }

    public RadioListBox()
    {
        DrawMode = DrawMode.OwnerDrawFixed;
        BorderStyle = BorderStyle.None;
        BackColor = Theme.Bg;
        ForeColor = Theme.Text;
        IntegralHeight = false;
        SetStyle(ControlStyles.OptimizedDoubleBuffer, true);
        UpdateItemHeight();
    }

    int S(float v) => (int)Math.Round(v * DeviceDpi / 96f);

    public int RowHeight => ItemHeight;

    void UpdateItemHeight() => ItemHeight = Math.Min(255, S(78));

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        Theme.DarkScrollbars(this);
        UpdateItemHeight();
    }

    protected override void OnDpiChangedAfterParent(EventArgs e)
    {
        base.OnDpiChangedAfterParent(e);
        UpdateItemHeight();
        Invalidate();
    }

    protected override void OnDrawItem(DrawItemEventArgs e)
    {
        var g = e.Graphics;
        using (var bg = new SolidBrush(BackColor))
            g.FillRectangle(bg, e.Bounds);
        if (e.Index < 0 || e.Index >= Items.Count || Items[e.Index] is not RadioInfo radio)
            return;

        var selected = (e.State & DrawItemState.Selected) != 0;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var card = new RectangleF(e.Bounds.X + 0.5f, e.Bounds.Y + S(4) + 0.5f, e.Bounds.Width - 1f, e.Bounds.Height - S(8) - 1f);
        using (var path = Theme.Rounded(card, S(10)))
        {
            // На чорному тлі (клієнт) картка — #111, на панелі #111 (програма збірки) — чорна.
            using var b = new SolidBrush(selected ? Theme.Panel2 : BackColor == Theme.Panel ? Theme.Bg : Theme.Panel);
            g.FillPath(b, path);
            using var p = new Pen(selected ? Theme.Accent : Theme.Line, selected ? Math.Max(1.5f, S(1.5f)) : 1f);
            g.DrawPath(p, path);
        }

        // Значок ліворуч.
        var icon = S(34);
        var ix = (int)card.X + S(14);
        var iy = (int)(card.Y + (card.Height - icon) / 2);
        Antenna.Draw(g, new RectangleF(ix, iy, icon, icon), Theme.Accent);

        var tx = ix + icon + S(14);
        var tw = (int)card.Right - tx - S(12);
        var flags = TextFormatFlags.NoPrefix | TextFormatFlags.SingleLine | TextFormatFlags.EndEllipsis | TextFormatFlags.Left;
        var title = Theme.F(DeviceDpi, 15f, FontStyle.Bold);
        var small = Theme.F(DeviceDpi, 13f);
        var line1 = TextRenderer.MeasureText("Ag", title).Height;
        var line2 = TextRenderer.MeasureText("Ag", small).Height;
        var top = (int)(card.Y + (card.Height - line1 - 2 * line2) / 2);

        TextRenderer.DrawText(g, "ПОТУЖНЕ РАДІО", title, new Rectangle(tx, top, tw, line1), Theme.Text, flags);
        var sub = radio.Subtitle + (radio.Legacy ? " · стара прошивка" : "");
        TextRenderer.DrawText(g, sub, small, new Rectangle(tx, top + line1, tw, line2), Theme.Muted, flags);
        var (third, accent) = Details?.Invoke(radio) ??
            (radio.Station.Length == 0 ? "станцію не вибрано" : radio.Playing ? "▶ " + radio.Station : radio.Station, radio.Playing);
        TextRenderer.DrawText(g, third, small, new Rectangle(tx, top + line1 + line2, tw, line2),
            accent ? Theme.Accent : Theme.Text, flags);
    }
}
