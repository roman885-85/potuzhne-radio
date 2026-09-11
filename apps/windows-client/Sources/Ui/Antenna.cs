using System.Drawing.Drawing2D;

namespace PotuzhneRadio;

/// <summary>
/// Значок антени — той самий, що в іконці програми й на сторінці радіо.
/// Геометрія дослівно з SVG (поле 24×24, лінія 2, круглі кінці):
///   circle cx=12 cy=11 r=2
///   M8.5 7.5 a5 5 0 0 0 0 7    M15.5 7.5 a5 5 0 0 1 0 7
///   M5.6 4.6 a9 9 0 0 0 0 12.8 M18.4 4.6 a9 9 0 0 1 0 12.8
///   M12 13 v8
/// Той самий розрахунок дуг повторює make-ico.swift — малюнки мають збігатися.
/// </summary>
static class Antenna
{
    /// <summary>
    /// Дуга SVG з точки (x, y) до (x, y + h) радіусом r: хорда вертикальна,
    /// дуга менша за півколо. bulgeLeft — опукла ліворуч, тобто «(».
    /// Повертає прямокутник кола й кути для Graphics.DrawArc (за годинниковою
    /// стрілкою від осі X, вісь Y — вниз).
    /// </summary>
    static (RectangleF rect, float start, float sweep) SvgArc(float x, float y, float h, float r, bool bulgeLeft)
    {
        var half = h / 2f;
        var d = MathF.Sqrt(r * r - half * half);   // від хорди до центру
        var cx = bulgeLeft ? x + d : x - d;
        var cy = y + half;
        var a = MathF.Atan2(half, d) * 180f / MathF.PI;
        var start = bulgeLeft ? 180f - a : -a;
        return (new RectangleF(cx - r, cy - r, 2 * r, 2 * r), start, 2 * a);
    }

    static readonly (float x, float y, float h, float r, bool left)[] Arcs =
    {
        (8.5f, 7.5f, 7f, 5f, true),
        (15.5f, 7.5f, 7f, 5f, false),
        (5.6f, 4.6f, 12.8f, 9f, true),
        (18.4f, 4.6f, 12.8f, 9f, false),
    };

    /// <summary>
    /// Малює значок у квадраті box кольором color. Межі самого малюнка —
    /// x 4.6…19.4, y 3.6…22 (з товщиною лінії), тож він зсунутий на 0,8 вгору,
    /// щоб стояти по центру поля.
    /// </summary>
    public static void Draw(Graphics g, RectangleF box, Color color, float stroke = 2f)
    {
        var s = Math.Min(box.Width, box.Height) / 24f;
        var ox = box.X + (box.Width - 24 * s) / 2f;
        var oy = box.Y + (box.Height - 24 * s) / 2f - 0.8f * s;

        var state = g.Save();
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.TranslateTransform(ox, oy);
        g.ScaleTransform(s, s);
        using var pen = new Pen(color, stroke)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round,
            LineJoin = LineJoin.Round,
        };
        g.DrawEllipse(pen, 10f, 9f, 4f, 4f);
        foreach (var (x, y, h, r, left) in Arcs)
        {
            var (rect, start, sweep) = SvgArc(x, y, h, r, left);
            g.DrawArc(pen, rect, start, sweep);
        }
        g.DrawLine(pen, 12f, 13f, 12f, 21f);
        g.Restore(state);
    }
}
