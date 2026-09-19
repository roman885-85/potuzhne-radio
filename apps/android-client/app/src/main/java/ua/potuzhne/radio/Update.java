package ua.potuzhne.radio;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Оновлення самого застосунку з GitHub.
 *
 * Раніше програму доводилось завантажувати вручну: радіо оновлювалось саме, а
 * телефон — ні. Тепер застосунок дивиться у випуск на GitHub і, якщо там
 * свіжіший за нього, пропонує оновитись.
 *
 * Порівнюємо не з номером випуску, а з файлом PotuzhneRadio-clients.json —
 * там записано, якої версії програма справді лежить у цьому випуску. Якщо
 * застосунок не перезбирали, він переноситься з минулого разу зі своїм
 * номером, і оновлення дарма не пропонується.
 *
 * Сам файл качає системний завантажувач (DownloadManager): він дає готову
 * адресу content://, яку можна віддати встановлювачу, тож свій ContentProvider
 * не потрібен — а сторонніх бібліотек у цьому застосунку немає навмисно.
 */
final class Update {

    private Update() {}

    private static final String REPO = "roman885-85/potuzhne-radio";
    private static final String APK_ASSET = "PotuzhneRadio-Android.apk";
    private static final String MANIFEST_ASSET = "PotuzhneRadio-clients.json";
    private static final Handler MAIN = new Handler(Looper.getMainLooper());

    private static boolean busy;

    static String myVersion(Context ctx) {
        try {
            return ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).versionName;
        } catch (PackageManager.NameNotFoundException e) {
            return "0";
        }
    }

    /** «1.4.38» новіша за «1.4.9» — порівнюємо числами, не рядками. */
    static boolean isNewer(String a, String b) {
        String[] pa = a.split("\\."), pb = b.split("\\.");
        for (int i = 0; i < Math.max(pa.length, pb.length); i++) {
            int x = i < pa.length ? num(pa[i]) : 0;
            int y = i < pb.length ? num(pb[i]) : 0;
            if (x != y) return x > y;
        }
        return false;
    }

    private static int num(String s) {
        int v = 0;
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c < '0' || c > '9') break;
            v = v * 10 + (c - '0');
        }
        return v;
    }

    private static String get(String url) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(8000);
        c.setReadTimeout(15000);
        c.setRequestProperty("User-Agent", "potuzhne-radio-android");
        try {
            if (c.getResponseCode() / 100 != 2) throw new Exception("HTTP " + c.getResponseCode());
            InputStream in = c.getInputStream();
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            for (int n; (n = in.read(buf)) > 0; ) out.write(buf, 0, n);
            return out.toString("UTF-8");
        } finally {
            c.disconnect();
        }
    }

    /** Що лежить у свіжому випуску. */
    private static final class Found {
        final String version, url, notes;
        Found(String version, String url, String notes) {
            this.version = version; this.url = url; this.notes = notes;
        }
    }

    private static Found check(Context ctx) {
        try {
            JSONObject top = new JSONObject(get("https://api.github.com/repos/" + REPO + "/releases/latest"));
            JSONArray assets = top.optJSONArray("assets");
            String apk = null, manifest = null;
            for (int i = 0; assets != null && i < assets.length(); i++) {
                JSONObject a = assets.getJSONObject(i);
                String name = a.optString("name");
                if (APK_ASSET.equals(name)) apk = a.optString("browser_download_url", null);
                else if (MANIFEST_ASSET.equals(name)) manifest = a.optString("browser_download_url", null);
            }
            if (apk == null) return null;

            String there = top.optString("tag_name", "");
            if (there.startsWith("v")) there = there.substring(1);
            if (manifest != null) {
                try {
                    String v = new JSONObject(get(manifest)).optString("android", "");
                    if (v.length() > 0) there = v;
                } catch (Exception e) {
                    Log.i(Net.TAG, "Оновлення: маніфест — " + e);
                }
            }
            if (there.length() == 0 || !isNewer(there, myVersion(ctx))) return null;
            return new Found(there, apk, top.optString("body", ""));
        } catch (Exception e) {
            Log.i(Net.TAG, "Оновлення: перевірка — " + e);
            return null;
        }
    }

    /**
     * Питаємо людину й оновлюємось. silent — мовчати, коли нового немає.
     */
    static void run(final Activity act, final boolean silent) {
        if (busy) return;
        busy = true;
        new Thread(new Runnable() {
            @Override public void run() {
                final Found f = check(act);
                MAIN.post(new Runnable() {
                    @Override public void run() {
                        busy = false;
                        if (act.isFinishing()) return;
                        if (f == null) {
                            if (!silent) {
                                new AlertDialog.Builder(act)
                                        .setTitle("Оновлення не потрібне")
                                        .setMessage("У вас найсвіжіша версія — " + myVersion(act) + ".")
                                        .setPositiveButton("Гаразд", null)
                                        .show();
                            }
                            return;
                        }
                        ask(act, f);
                    }
                });
            }
        }, "update").start();
    }

    private static void ask(final Activity act, final Found f) {
        String first = f.notes.split("\n", 2)[0].trim();
        String text = (first.length() > 0 ? first + "\n\n" : "")
                + "Зараз у вас " + myVersion(act) + ". Завантажити й встановити?";
        new AlertDialog.Builder(act)
                .setTitle("Є нова версія — " + f.version)
                .setMessage(text)
                .setNegativeButton("Пізніше", null)
                .setPositiveButton("Оновити", (d, w) -> download(act, f))
                .show();
    }

    private static void download(final Activity act, final Found f) {
        //  З Android 8 встановлення просить окремий дозвіл. Якщо його ще нема,
        //  система сама покаже своє вікно — ведемо туди, інакше встановлювач
        //  мовчки нічого не зробить.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && !act.getPackageManager().canRequestPackageInstalls()) {
            new AlertDialog.Builder(act)
                    .setTitle("Потрібен дозвіл")
                    .setMessage("Android дозволяє встановлювати оновлення лише з дозволу. "
                            + "Зараз відкриється налаштування — увімкніть його для «ПОТУЖНЕ РАДІО» і поверніться.")
                    .setNegativeButton("Пізніше", null)
                    .setPositiveButton("Відкрити", (d, w) -> {
                        try {
                            act.startActivity(new Intent(
                                    android.provider.Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                                    Uri.parse("package:" + act.getPackageName())));
                        } catch (Exception e) {
                            Log.i(Net.TAG, "Оновлення: дозвіл — " + e);
                        }
                    })
                    .show();
            return;
        }

        final DownloadManager dm = (DownloadManager) act.getSystemService(Context.DOWNLOAD_SERVICE);
        if (dm == null) return;
        DownloadManager.Request req = new DownloadManager.Request(Uri.parse(f.url));
        req.setTitle("ПОТУЖНЕ РАДІО " + f.version);
        req.setDescription("Завантажую оновлення");
        req.setMimeType("application/vnd.android.package-archive");
        req.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
        req.setDestinationInExternalFilesDir(act, null, "PotuzhneRadio-" + f.version + ".apk");

        final long id;
        try {
            id = dm.enqueue(req);
        } catch (Exception e) {
            Log.i(Net.TAG, "Оновлення: завантаження — " + e);
            return;
        }

        final BroadcastReceiver done = new BroadcastReceiver() {
            @Override public void onReceive(Context ctx, Intent intent) {
                if (intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1) != id) return;
                try { act.unregisterReceiver(this); } catch (Exception ignore) {}
                Uri uri = dm.getUriForDownloadedFile(id);
                if (uri == null) return;
                Intent install = new Intent(Intent.ACTION_VIEW);
                install.setDataAndType(uri, "application/vnd.android.package-archive");
                install.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
                try {
                    act.startActivity(install);
                } catch (Exception e) {
                    Log.i(Net.TAG, "Оновлення: встановлювач — " + e);
                }
            }
        };
        IntentFilter filter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
        if (Build.VERSION.SDK_INT >= 33) act.registerReceiver(done, filter, Context.RECEIVER_EXPORTED);
        else act.registerReceiver(done, filter);
    }
}
