import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.geom.AffineTransform;
import java.awt.geom.Arc2D;
import java.awt.geom.Ellipse2D;
import java.awt.geom.Line2D;
import java.awt.geom.Path2D;
import java.awt.geom.RoundRectangle2D;
import java.awt.image.BufferedImage;
import java.io.File;
import javax.imageio.ImageIO;

/**
 * Значок застосунку — антена радіо, жовта на чорному, у PNG для всіх щільностей.
 *
 * Малюється з того самого опису, що й логотип на сторінці радіо
 * (viewBox 0 0 24 24, товщина лінії 2, круглі кінці):
 *   <circle cx="12" cy="11" r="2"/>
 *   <path d="M8.5 7.5a5 5 0 0 0 0 7 M15.5 7.5a5 5 0 0 1 0 7
 *            M5.6 4.6a9 9 0 0 0 0 12.8 M18.4 4.6a9 9 0 0 1 0 12.8 M12 13v8"/>
 *
 * Картинки не лежать у джерелах: build.sh щоразу малює їх наново в теку
 * збірки. Так значок не може розійтися з описом, а в джерелах немає
 * двійкових файлів, які нічим не перевірити.
 *
 *   java scripts/MakeIcons.java <тека-res>
 */
public class MakeIcons {

    static final Color YELLOW = new Color(0xe6, 0xd2, 0x5a);
    static final Color BLACK = new Color(0, 0, 0);
    static final Color RIM = new Color(0x26, 0x26, 0x26);

    public static void main(String[] args) throws Exception {
        File res = new File(args.length > 0 ? args[0] : "res");
        String[] names = {"mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi"};
        int[] sizes = {48, 72, 96, 144, 192};
        for (int i = 0; i < names.length; i++) {
            File dir = new File(res, "mipmap-" + names[i]);
            dir.mkdirs();
            ImageIO.write(draw(sizes[i], false), "png", new File(dir, "ic_launcher.png"));
            ImageIO.write(draw(sizes[i], true), "png", new File(dir, "ic_launcher_round.png"));
        }
    }

    /**
     * Старі запускачі (до Android 8) показують картинку як є, без власної
     * маски — тому тло тут уже обрізане: заокруглений квадрат або коло.
     */
    static BufferedImage draw(int size, boolean round) {
        BufferedImage img = new BufferedImage(size, size, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = img.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL, RenderingHints.VALUE_STROKE_PURE);
        g.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);

        // Запас по краях — як у системних значків (48 → 44 видимих).
        double inset = size * 0.042;
        double side = size - 2 * inset;
        java.awt.Shape plate = round
                ? new Ellipse2D.Double(inset, inset, side, side)
                : new RoundRectangle2D.Double(inset, inset, side, side, side * 0.36, side * 0.36);
        g.setColor(BLACK);
        g.fill(plate);
        g.setColor(RIM);
        g.setStroke(new BasicStroke((float) Math.max(1, size / 96.0)));
        g.draw(plate);

        // Антена займає ~60 % висоти; центр рамки малюнка (з урахуванням
        // товщини лінії) — у (12, 12.8) одиниць опису.
        double scale = side * 0.60 / 18.4;
        AffineTransform t = new AffineTransform();
        t.translate(size / 2.0, size / 2.0);
        t.scale(scale, scale);
        t.translate(-12, -12.8);
        g.transform(t);

        g.setColor(YELLOW);
        g.setStroke(new BasicStroke(2f, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
        g.draw(new Ellipse2D.Double(10, 9, 4, 4));
        g.draw(arc(8.5, 7.5, 5, false, 8.5, 14.5));
        g.draw(arc(15.5, 7.5, 5, true, 15.5, 14.5));
        g.draw(arc(5.6, 4.6, 9, false, 5.6, 17.4));
        g.draw(arc(18.4, 4.6, 9, true, 18.4, 17.4));
        g.draw(new Line2D.Double(12, 13, 12, 21));
        g.dispose();
        return img;
    }

    /**
     * Мала дуга кола з SVG («a r r 0 0 sweep dx dy») між двома точками.
     * sweep = true — за годинниковою стрілкою на екрані.
     */
    static java.awt.Shape arc(double x1, double y1, double r, boolean sweep, double x2, double y2) {
        double mx = (x1 + x2) / 2, my = (y1 + y2) / 2;
        double dx = x2 - x1, dy = y2 - y1, len = Math.hypot(dx, dy);
        double d = Math.sqrt(Math.max(0, r * r - len * len / 4));
        double px = -dy / len, py = dx / len;
        double cx = sweep ? mx + d * px : mx - d * px;
        double cy = sweep ? my + d * py : my - d * py;
        // Кути на екрані (вісь y донизу), у градусах.
        double a1 = Math.toDegrees(Math.atan2(y1 - cy, x1 - cx));
        double a2 = Math.toDegrees(Math.atan2(y2 - cy, x2 - cx));
        double sweepDeg = a2 - a1;
        if (sweep) { while (sweepDeg <= 0) sweepDeg += 360; }
        else { while (sweepDeg >= 0) sweepDeg -= 360; }
        // Java2D рахує кути проти годинникової стрілки — звідси мінуси.
        Path2D p = new Path2D.Double();
        p.append(new Arc2D.Double(cx - r, cy - r, 2 * r, 2 * r, -a1, -sweepDeg, Arc2D.OPEN), false);
        return p;
    }
}
