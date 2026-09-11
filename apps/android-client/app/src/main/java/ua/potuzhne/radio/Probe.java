package ua.potuzhne.radio;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Питання до адреси «ти ПОТУЖНЕ РАДІО?».
 *
 * Лише GET і лише два шляхи: /api/hello (прошивка 1.0 і новіші відповідають
 * JSON-ом із "potuzhne":1) і, коли його ще немає (404), /api/state — у старої
 * прошивки там поле fw, що починається з «POTUZHNE-RADIO-FW|». Нічого на радіо
 * ці запити не змінюють: людина в цей час може його слухати.
 */
final class Probe {

    private Probe() {}

    static final String FW_MARK = "POTUZHNE-RADIO-FW|";
    static final String AGENT = "PotuzhneRadioApp/1.0 (Android)";

    /** Відповів хтось, але не наше радіо (роутер, принтер, чужа сторінка). */
    static final class NotRadio extends Exception {
        NotRadio() { super("не ПОТУЖНЕ РАДІО"); }
    }

    /** Для перебору мережі: радіо або null, без пояснень чому ні. */
    static Radio find(String address, int connectMs, int readMs) {
        try {
            return check(address, connectMs, readMs);
        } catch (IOException | NotRadio e) {
            return null;
        }
    }

    /**
     * Те саме, але з причиною — для адреси, введеної руками, людині треба
     * знати, чи там нікого немає (IOException), чи там щось інше (NotRadio).
     */
    static Radio check(String address, int connectMs, int readMs) throws IOException, NotRadio {
        Answer hello = get(address, "/api/hello", connectMs, readMs);
        if (hello.code == 200) {
            JSONObject j = json(hello.body);
            if (j != null && j.optInt("potuzhne", 0) == 1) {
                return new Radio(address, text(j, "host"), text(j, "station"), text(j, "v"),
                        text(j, "build"), j.optInt("play", 0) == 1, true);
            }
            throw new NotRadio();
        }
        if (hello.code == 404) {
            // Стара прошивка: /api/hello ще не знає. Стан більший і радіо
            // збирає його довше — читаємо з запасом.
            Answer state = get(address, "/api/state", connectMs, Math.max(readMs, 2500));
            if (state.code == 200) {
                JSONObject j = json(state.body);
                if (j != null && text(j, "fw").startsWith(FW_MARK)) {
                    return new Radio(address, "", text(j, "name"), text(j, "v"),
                            text(j, "build"), j.optInt("play", 0) == 1, false);
                }
            }
        }
        throw new NotRadio();
    }

    /** Чи живе радіо — один запит на той шлях, який воно знає. */
    static boolean alive(Radio radio, int connectMs, int readMs) {
        try {
            return get(radio.address, radio.hello ? "/api/hello" : "/api/state",
                    connectMs, readMs).code == 200;
        } catch (IOException e) {
            return false;
        }
    }

    // ------------------------------------------------------------------------

    private static final class Answer {
        final int code;
        final String body;
        Answer(int code, String body) { this.code = code; this.body = body; }
    }

    /**
     * «Connection: close» — радіо тримає лише кілька з'єднань, і кожне
     * зайве, залишене Android'ом у запасі, забирало б місце в його сторінки.
     * Та й Android, узявши застояне з'єднання з запасу, тихо повторює запит —
     * а це вже дві відповіді замість однієї.
     */
    private static Answer get(String address, String path, int connectMs, int readMs) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL("http://" + address + path).openConnection();
        try {
            c.setConnectTimeout(connectMs);
            c.setReadTimeout(readMs);
            c.setInstanceFollowRedirects(false);
            c.setUseCaches(false);
            c.setRequestProperty("Connection", "close");
            c.setRequestProperty("Accept", "application/json");
            c.setRequestProperty("User-Agent", AGENT);
            int code = c.getResponseCode();
            String body = code == 200 ? read(c.getInputStream(), 64 * 1024) : "";
            return new Answer(code, body);
        } finally {
            c.disconnect();
        }
    }

    /** Не більше limit байтів: чужий пристрій може віддати на /api/hello що завгодно. */
    private static String read(InputStream in, int limit) throws IOException {
        try (InputStream s = in) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = s.read(buf)) > 0 && out.size() < limit) out.write(buf, 0, n);
            return out.toString("UTF-8");
        }
    }

    private static JSONObject json(String body) {
        try {
            return new JSONObject(body.trim());
        } catch (Exception e) {
            return null;
        }
    }

    /** optString повертає «null» словом, якщо в JSON стоїть null, — тут порожньо. */
    private static String text(JSONObject j, String key) {
        if (j.isNull(key)) return "";
        return j.optString(key, "");
    }

    /**
     * Адреса, введена людиною, — до вигляду «хост[:порт]».
     * Прибирає http://, шлях і пробіли; повертає null, якщо лишилося порожньо.
     */
    static String normalise(String input) {
        if (input == null) return null;
        String s = input.trim();
        int scheme = s.indexOf("://");
        if (scheme >= 0) s = s.substring(scheme + 3);
        int slash = s.indexOf('/');
        if (slash >= 0) s = s.substring(0, slash);
        int query = s.indexOf('?');
        if (query >= 0) s = s.substring(0, query);
        s = s.trim();
        if (s.endsWith(":80")) s = s.substring(0, s.length() - 3);
        if (s.isEmpty() || s.contains(" ")) return null;
        return s;
    }
}
