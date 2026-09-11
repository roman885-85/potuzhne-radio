package ua.potuzhne.radio;

import android.Manifest;
import android.app.Activity;
import android.app.DownloadManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Файли зі сторінки радіо — у «Завантаження» телефона.
 *
 * Два різні шляхи, бо й файли різні:
 *  - записи ефіру, список станцій, wifi.csv — звичайні посилання на радіо;
 *    їх забирає системний завантажувач (DownloadManager): великий файл іде
 *    у фоні, хід видно в сповіщеннях;
 *  - експорт (CSV, M3U, обране.json) сторінка збирає в себе в пам'яті
 *    (blob:) — такого файла ніде в мережі немає, завантажувачу нема чого
 *    качати. Байти приходять зі сторінки й пишуться тут.
 *
 * Куди писати, залежить від Android. З 10-го «Завантаження» доступні без
 * жодного дозволу (через MediaStore). На 5–9 — лише з дозволом на пам'ять;
 * якщо людина відмовила, файл кладеться у власну теку застосунку
 * (Android/data/ua.potuzhne.radio/files/Download) — теж без дозволу, і
 * шлях показуємо словами.
 */
final class Saver {

    static final int REQUEST = 12;

    private final Activity activity;
    private final List<Runnable> waiting = new ArrayList<>();
    private boolean asking;

    Saver(Activity activity) {
        this.activity = activity;
    }

    /** Чи можна писати просто в загальні «Завантаження». */
    boolean publicAllowed() {
        if (Build.VERSION.SDK_INT >= 29 || Build.VERSION.SDK_INT < 23) return true;
        return activity.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Виконати збереження, за потреби спершу спитавши дозволу. Відмова не
     * скасовує збереження — лише змінює теку.
     */
    void withStorage(Runnable job) {
        if (publicAllowed()) {
            job.run();
            return;
        }
        waiting.add(job);
        if (!asking) {
            asking = true;
            activity.requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, REQUEST);
        }
    }

    /** Відповідь на запит дозволу — з onRequestPermissionsResult. */
    void onPermissionAnswer() {
        asking = false;
        List<Runnable> jobs = new ArrayList<>(waiting);
        waiting.clear();
        for (Runnable r : jobs) r.run();
    }

    /** Файл за посиланням — системному завантажувачу. Повертає текст для людини. */
    @SuppressWarnings("deprecation")
    String download(String url, String name, String mime, String userAgent) {
        DownloadManager dm = (DownloadManager) activity.getSystemService(Context.DOWNLOAD_SERVICE);
        if (dm == null) return "На цьому телефоні немає системного завантажувача";
        try {
            DownloadManager.Request req = new DownloadManager.Request(Uri.parse(url));
            req.setTitle(name);
            req.setDescription("ПОТУЖНЕ РАДІО");
            req.setMimeType(mime != null && !mime.isEmpty() ? mime : mimeOf(name));
            if (userAgent != null) req.addRequestHeader("User-Agent", userAgent);
            req.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
            boolean shared = publicAllowed();
            if (shared) {
                req.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS, name);
                if (Build.VERSION.SDK_INT < 29) req.allowScanningByMediaScanner();
            } else {
                req.setDestinationInExternalFilesDir(activity, Environment.DIRECTORY_DOWNLOADS, name);
            }
            dm.enqueue(req);
            return "Завантажую «" + name + "» — хід видно в сповіщеннях";
        } catch (Exception e) {
            return "Не вдалося почати завантаження: " + e.getMessage();
        }
    }

    /** Файл, зібраний сторінкою, — байти вже тут. Повертає текст для людини. */
    String saveBytes(String name, String mime, byte[] bytes) {
        if (mime == null || mime.isEmpty() || mime.startsWith("application/octet")) mime = mimeOf(name);
        try {
            if (Build.VERSION.SDK_INT >= 29) return saveQ(name, mime, bytes);
            return saveLegacy(name, mime, bytes);
        } catch (Exception e) {
            return "Не вдалося зберегти «" + name + "»: " + e.getMessage();
        }
    }

    private String saveQ(String name, String mime, byte[] bytes) throws IOException {
        ContentResolver cr = activity.getContentResolver();
        ContentValues v = new ContentValues();
        v.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
        v.put(MediaStore.MediaColumns.MIME_TYPE, mime);
        v.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
        v.put(MediaStore.MediaColumns.IS_PENDING, 1);
        Uri target = cr.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
        if (target == null) throw new IOException("система не дала місця");
        try (OutputStream out = cr.openOutputStream(target)) {
            if (out == null) throw new IOException("файл не відкрився");
            out.write(bytes);
        }
        v.clear();
        v.put(MediaStore.MediaColumns.IS_PENDING, 0);
        cr.update(target, v, null, null);
        return "Збережено в «Завантаження»: " + name;
    }

    @SuppressWarnings("deprecation")
    private String saveLegacy(String name, String mime, byte[] bytes) throws IOException {
        boolean shared = publicAllowed();
        File dir = shared
                ? Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
                : activity.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        if (dir == null) throw new IOException("пам'ять телефона недоступна");
        if (!dir.exists() && !dir.mkdirs()) throw new IOException("не створюється тека " + dir);
        File file = unique(dir, name);
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        }
        MediaScannerConnection.scanFile(activity, new String[]{file.getAbsolutePath()}, new String[]{mime}, null);
        if (shared) {
            // Показати у «Завантаженнях» і сповіщенням, звідки файл можна відкрити.
            DownloadManager dm = (DownloadManager) activity.getSystemService(Context.DOWNLOAD_SERVICE);
            if (dm != null) {
                try {
                    dm.addCompletedDownload(file.getName(), "ПОТУЖНЕ РАДІО", true, mime,
                            file.getAbsolutePath(), file.length(), true);
                } catch (Exception ignored) {}
            }
            return "Збережено: Завантаження/" + file.getName();
        }
        return "Збережено: " + file.getAbsolutePath();
    }

    /** «playlist.csv» → «playlist (1).csv», якщо такий уже є. */
    private static File unique(File dir, String name) {
        File f = new File(dir, name);
        if (!f.exists()) return f;
        int dot = name.lastIndexOf('.');
        String stem = dot > 0 ? name.substring(0, dot) : name;
        String ext = dot > 0 ? name.substring(dot) : "";
        for (int i = 1; i < 1000; i++) {
            f = new File(dir, stem + " (" + i + ")" + ext);
            if (!f.exists()) return f;
        }
        return new File(dir, stem + "-" + System.currentTimeMillis() + ext);
    }

    /** Ім'я файла без шляху й без символів, яких не терплять файлові системи. */
    static String cleanName(String name, String fallback) {
        String s = name == null ? "" : name.trim();
        int slash = Math.max(s.lastIndexOf('/'), s.lastIndexOf('\\'));
        if (slash >= 0) s = s.substring(slash + 1);
        s = s.replaceAll("[\\u0000-\\u001f:*?\"<>|]", "_");
        while (s.startsWith(".")) s = s.substring(1);
        if (s.length() > 120) s = s.substring(s.length() - 120);
        return s.isEmpty() ? fallback : s;
    }

    /**
     * Тип за розширенням. Свій короткий перелік — для того, що віддає радіо:
     * у MimeTypeMap старих Android немає ні csv, ні json, а MediaStore за
     * невідомого типу дописує до імені «.bin».
     */
    static String mimeOf(String name) {
        String ext = "";
        int dot = name.lastIndexOf('.');
        if (dot >= 0) ext = name.substring(dot + 1).toLowerCase(Locale.ROOT);
        switch (ext) {
            case "csv": return "text/csv";
            case "json": return "application/json";
            case "m3u": case "m3u8": return "audio/x-mpegurl";
            case "txt": return "text/plain";
            case "mp3": return "audio/mpeg";
            case "aac": return "audio/aac";
            case "m4a": return "audio/mp4";
            case "wav": return "audio/wav";
            case "ogg": return "audio/ogg";
            case "png": return "image/png";
            case "jpg": case "jpeg": return "image/jpeg";
            default:
                String m = ext.isEmpty() ? null : MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext);
                return m != null ? m : "application/octet-stream";
        }
    }
}
