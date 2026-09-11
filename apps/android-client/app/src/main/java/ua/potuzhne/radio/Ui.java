package ua.potuzhne.radio;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Вигляд — той самий, що на екрані радіо й на його сторінці: чорне тло,
 * панелі #111/#1b1b1b, текст #f2f2f2, приглушений #8c8c8c, жовтий акцент.
 */
final class Ui {
    private Ui() {}

    static final int BG = 0xff000000;
    static final int PAN = 0xff111111;
    static final int PAN2 = 0xff1b1b1b;
    static final int LINE = 0xff262626;
    static final int LINE2 = 0xff333333;
    static final int TXT = 0xfff2f2f2;
    static final int DIM = 0xff8c8c8c;
    static final int DIM2 = 0xff5e5e5e;
    static final int ACC = 0xffe6d25a;
    static final int ACC_SOFT = 0x24e6d25a;
    static final int WARN = 0xfff0a53a;

    static final Typeface MEDIUM = Typeface.create("sans-serif-medium", Typeface.NORMAL);

    static int dp(Context c, float v) {
        return Math.round(v * c.getResources().getDisplayMetrics().density);
    }

    static TextView text(Context c, String s, float sp, int color) {
        TextView t = new TextView(c);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color);
        return t;
    }

    static GradientDrawable panel(Context c, int fill, int stroke, float radiusDp) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        g.setCornerRadius(dp(c, radiusDp));
        if (stroke != 0) g.setStroke(Math.max(1, dp(c, 1)), stroke);
        return g;
    }

    /**
     * Кнопка: жовта з чорним текстом (головна дія) або темна з рамкою.
     * Натискання — хвиля, як у системних кнопках; тінь прибрано, бо на
     * чорному тлі вона лише бруднить край.
     */
    static Button button(Context c, String label, boolean accent) {
        Button b = new Button(c);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextSize(15);
        b.setTypeface(MEDIUM);
        b.setTextColor(accent ? 0xff000000 : TXT);
        b.setStateListAnimator(null);
        b.setMinHeight(dp(c, 48));
        b.setMinimumHeight(dp(c, 48));
        b.setPadding(dp(c, 16), 0, dp(c, 16), 0);
        GradientDrawable shape = accent
                ? panel(c, ACC, 0, 10)
                : panel(c, PAN2, LINE2, 10);
        GradientDrawable mask = panel(c, 0xffffffff, 0, 10);
        b.setBackground(new RippleDrawable(
                ColorStateList.valueOf(accent ? 0x40000000 : 0x30ffffff), shape, mask));
        return b;
    }

    /** Вимкнене — напівпрозоре, щоб було видно, що кнопка є, але зараз не діє. */
    static void enable(View v, boolean on) {
        v.setEnabled(on);
        v.setAlpha(on ? 1f : 0.4f);
    }

    /** Стовпчик, що на планшеті не розтягується ширше за maxDp — посередині екрана. */
    static final class Column extends LinearLayout {
        private final int max;

        Column(Context c, float maxDp) {
            super(c);
            setOrientation(VERTICAL);
            max = dp(c, maxDp);
        }

        @Override
        protected void onMeasure(int widthSpec, int heightSpec) {
            int w = MeasureSpec.getSize(widthSpec);
            if (w > max) widthSpec = MeasureSpec.makeMeasureSpec(max, MeasureSpec.EXACTLY);
            super.onMeasure(widthSpec, heightSpec);
        }
    }

    // ---- малюнки ---------------------------------------------------------------

    /**
     * Антена радіо — логотип зі сторінки радіо (viewBox 24×24, лінія 2,
     * круглі кінці), намальований кодом, а не векторним ресурсом: дуги в
     * pathData старі Android 5 розуміють не всюди однаково, а тут — гарантовано.
     */
    static final class Antenna extends Drawable {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path = new Path();

        Antenna(int color) {
            paint.setColor(color);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(2f);
            paint.setStrokeCap(Paint.Cap.ROUND);
            paint.setStrokeJoin(Paint.Join.ROUND);
            path.addCircle(12, 11, 2, Path.Direction.CW);
            arc(path, 8.5f, 7.5f, 5, false, 8.5f, 14.5f);
            arc(path, 15.5f, 7.5f, 5, true, 15.5f, 14.5f);
            arc(path, 5.6f, 4.6f, 9, false, 5.6f, 17.4f);
            arc(path, 18.4f, 4.6f, 9, true, 18.4f, 17.4f);
            path.moveTo(12, 13);
            path.lineTo(12, 21);
        }

        private final Path scaled = new Path();

        /**
         * Малюнок перераховується під розмір, а не збільшується через
         * canvas.scale: Android 5–8 раструє контур у текстуру за його
         * власним розміром (24 точки) і потім розтягує — логотип виходив
         * розмитим, «пікселями».
         */
        @Override
        protected void onBoundsChange(android.graphics.Rect b) {
            float s = Math.min(b.width(), b.height()) / 24f;
            android.graphics.Matrix m = new android.graphics.Matrix();
            m.setScale(s, s);
            m.postTranslate(b.left + (b.width() - 24 * s) / 2f, b.top + (b.height() - 24 * s) / 2f);
            path.transform(m, scaled);
            paint.setStrokeWidth(2f * s);
        }

        @Override
        public void draw(Canvas canvas) {
            canvas.drawPath(scaled, paint);
        }

        /** Мала дуга кола з SVG («a r r 0 0 sweep …») між двома точками. */
        private static void arc(Path p, float x1, float y1, float r, boolean sweep, float x2, float y2) {
            double mx = (x1 + x2) / 2.0, my = (y1 + y2) / 2.0;
            double dx = x2 - x1, dy = y2 - y1, len = Math.hypot(dx, dy);
            double d = Math.sqrt(Math.max(0, r * r - len * len / 4));
            double px = -dy / len, py = dx / len;
            double cx = sweep ? mx + d * px : mx - d * px;
            double cy = sweep ? my + d * py : my - d * py;
            double a1 = Math.toDegrees(Math.atan2(y1 - cy, x1 - cx));
            double a2 = Math.toDegrees(Math.atan2(y2 - cy, x2 - cx));
            double sw = a2 - a1;
            if (sweep) { while (sw <= 0) sw += 360; } else { while (sw >= 0) sw -= 360; }
            p.moveTo(x1, y1);
            p.arcTo(new RectF((float) (cx - r), (float) (cy - r), (float) (cx + r), (float) (cy + r)),
                    (float) a1, (float) sw, false);
        }

        @Override public void setAlpha(int alpha) { paint.setAlpha(alpha); }
        @Override public void setColorFilter(ColorFilter cf) { paint.setColorFilter(cf); }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    /** Три крапки «⋮» — кнопка меню. */
    static final class Dots extends Drawable {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

        Dots(int color) { paint.setColor(color); }

        @Override
        public void draw(Canvas canvas) {
            android.graphics.Rect b = getBounds();
            float s = Math.min(b.width(), b.height()) / 24f;
            float cx = b.exactCenterX(), cy = b.exactCenterY();
            for (int i = -1; i <= 1; i++) canvas.drawCircle(cx, cy + i * 7 * s, 2 * s, paint);
        }

        @Override public void setAlpha(int alpha) { paint.setAlpha(alpha); }
        @Override public void setColorFilter(ColorFilter cf) { paint.setColorFilter(cf); }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }
}
