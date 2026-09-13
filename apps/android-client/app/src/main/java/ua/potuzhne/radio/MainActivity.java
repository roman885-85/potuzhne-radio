package ua.potuzhne.radio;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.speech.RecognizerIntent;
import android.text.InputType;
import android.util.Base64;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.webkit.ConsoleMessage;
import android.webkit.JavascriptInterface;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.URLUtil;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Застосунок — це два екрани в одному вікні: пошук радіо (свій, рідний) і
 * сторінка радіо (у WebView, як є).
 *
 * Свого пульта застосунок не малює навмисно: усе керування вже є на сторінці
 * радіо, і вона підлаштована під телефон. Друга копія того самого рідною
 * розміткою означала б дві версії однієї роботи. Застосунок бере на себе те,
 * чого браузер не вміє: сам знаходить радіо, пам'ятає його, помічає, що
 * радіо зникло, і рятує завантаження файлів.
 *
 * До радіо — лише GET (/api/hello, /api/state і сама сторінка). Усе, що
 * змінює щось на радіо, робить сама сторінка, коли людина натискає її кнопки.
 */
public class MainActivity extends Activity {

    private static final String TAG = Net.TAG;
    private static final int REQUEST_FILE = 11;
    private static final int REQUEST_VOICE = 12;
    private static final long CHECK_EVERY_MS = 10_000;
    private static final int CHECK_FAILS = 3;
    private static final long PAGE_TIMEOUT_MS = 25_000;
    /** Сторінка радіо потребує Chrome 87+ (replaceChildren, inset у CSS, gap у flex). */
    private static final int WEBVIEW_MIN = 87;

    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService io = Executors.newCachedThreadPool();
    private SharedPreferences prefs;
    private Saver saver;
    private boolean started;

    private FrameLayout root;

    // ---- екран пошуку ----
    private View searchScreen;
    private ProgressBar spinner;
    private TextView status;
    private TextView note;
    private TextView hint;
    private LinearLayout list;
    private Button connectButton;
    private Button againButton;
    private Button manualButton;

    private final List<Radio> found = new ArrayList<>();
    private String selected;
    private Finder finder;
    private boolean searching;
    private boolean autoOpen;
    /** Росте з кожним новим пошуком чи перевіркою — застарілі відповіді відкидаються. */
    private int searchGen;
    /** Радіо, чия сторінка не відкрилася, хоч саме воно відповідало: самі туди не повертаємось. */
    private final Set<String> brokenPages = new HashSet<>();

    // ---- екран радіо ----
    private FrameLayout radioScreen;
    private WebView web;
    private View cover;
    private TextView coverText;
    private View menuButton;
    private Radio current;
    /** Росте з кожним відкриттям і закриттям сторінки — те саме, що searchGen, для неї. */
    private int openGen;
    private int checkFails;
    private boolean pageShown;
    private int loadRetries;
    private ValueCallback<Uri[]> fileCallback;
    private Object backCallback;
    private boolean webViewWarned;

    // =========================================================================

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = getSharedPreferences("radio", MODE_PRIVATE);
        saver = new Saver(this);

        root = new FrameLayout(this);
        root.setBackgroundColor(Ui.BG);
        searchScreen = buildSearchScreen();
        root.addView(searchScreen, new FrameLayout.LayoutParams(-1, -1));
        setContentView(root);
        SystemBars.keepClear(root);

        begin();
    }

    @Override
    protected void onStart() {
        super.onStart();
        started = true;
        // Повернулися до застосунку — одразу глянути, чи радіо ще тут.
        if (current != null) {
            main.removeCallbacks(check);
            main.post(check);
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        started = false;
        // Перевірку живості зупиняємо, а сторінку — ні: якщо людина слухає
        // радіо в самій сторінці, звук має жити й за згорнутим застосунком
        // (скільки дозволить Android). Тому web.onPause() тут не кличемо.
        main.removeCallbacks(check);
    }

    @Override
    protected void onDestroy() {
        if (finder != null) finder.cancel();
        closeRadio();
        io.shutdownNow();
        main.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    /**
     * Старт: спершу — радіо, до якого під'єднувалися минулого разу. Відповідає —
     * відкриваємо без пошуку. Ні — шукаємо в мережі.
     */
    private void begin() {
        String saved = prefs.getString("address", null);
        if (saved == null) {
            startSearch(true, null);
            return;
        }
        String host = prefs.getString("host", "");
        String label = host.isEmpty() ? saved : host + " · " + saved;
        showSearchScreen();
        found.clear();
        renderList();
        setNote(null, false);
        setBusy("Підключаюся до " + label + "…");
        hint.setText("");
        int gen = ++searchGen;
        updateButtons();
        io.execute(() -> {
            Net.bindTo(this, Net.localNetwork(this));
            Radio r = Probe.find(saved, 1500, 3000);
            main.post(() -> {
                if (gen != searchGen || isFinishing()) return;
                if (r != null) openRadio(r);
                else startSearch(true, "Радіо " + label + " не відповідає — шукаю в мережі.");
            });
        });
    }

    // =========================================================================
    // Пошук
    // =========================================================================

    private View buildSearchScreen() {
        LinearLayout screen = new LinearLayout(this);
        screen.setOrientation(LinearLayout.VERTICAL);
        screen.setBackgroundColor(Ui.BG);

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        FrameLayout center = new FrameLayout(this);
        Ui.Column col = new Ui.Column(this, 520);
        int pad = dp(24);
        col.setPadding(pad, dp(40), pad, pad);
        col.setGravity(Gravity.CENTER_HORIZONTAL);

        View logo = new View(this);
        logo.setBackground(new Ui.Antenna(Ui.ACC));
        logo.setContentDescription("ПОТУЖНЕ РАДІО");
        col.addView(logo, new LinearLayout.LayoutParams(dp(88), dp(88)));

        TextView title = Ui.text(this, "ПОТУЖНЕ РАДІО", 24, Ui.ACC);
        title.setTypeface(Ui.MEDIUM);
        title.setLetterSpacing(0.08f);
        title.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams tl = new LinearLayout.LayoutParams(-2, -2);
        tl.topMargin = dp(12);
        col.addView(title, tl);

        LinearLayout statusRow = new LinearLayout(this);
        statusRow.setOrientation(LinearLayout.HORIZONTAL);
        statusRow.setGravity(Gravity.CENTER);
        spinner = new ProgressBar(this);
        spinner.setIndeterminate(true);
        spinner.setIndeterminateTintList(android.content.res.ColorStateList.valueOf(Ui.ACC));
        LinearLayout.LayoutParams sl = new LinearLayout.LayoutParams(dp(20), dp(20));
        sl.rightMargin = dp(10);
        statusRow.addView(spinner, sl);
        status = Ui.text(this, "", 16, Ui.TXT);
        status.setGravity(Gravity.CENTER);
        statusRow.addView(status, new LinearLayout.LayoutParams(-2, -2));
        LinearLayout.LayoutParams srl = new LinearLayout.LayoutParams(-1, -2);
        srl.topMargin = dp(24);
        col.addView(statusRow, srl);

        note = Ui.text(this, "", 15, Ui.WARN);
        note.setGravity(Gravity.CENTER);
        note.setPadding(dp(14), dp(10), dp(14), dp(10));
        note.setBackground(Ui.panel(this, Ui.PAN2, Ui.LINE2, 10));
        note.setVisibility(View.GONE);
        LinearLayout.LayoutParams nl = new LinearLayout.LayoutParams(-1, -2);
        nl.topMargin = dp(16);
        col.addView(note, nl);

        hint = Ui.text(this, "", 13, Ui.DIM);
        hint.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams hl = new LinearLayout.LayoutParams(-1, -2);
        hl.topMargin = dp(8);
        col.addView(hint, hl);

        list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams ll = new LinearLayout.LayoutParams(-1, -2);
        ll.topMargin = dp(20);
        col.addView(list, ll);

        center.addView(col, new FrameLayout.LayoutParams(-1, -2, Gravity.CENTER_HORIZONTAL));
        scroll.addView(center, new ScrollView.LayoutParams(-1, -2));
        screen.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1f));

        // Кнопки — унизу, під пальцем, на панелі #111 з лінією зверху.
        FrameLayout bar = new FrameLayout(this);
        bar.setBackground(topLine());
        Ui.Column buttons = new Ui.Column(this, 520);
        buttons.setPadding(dp(16), dp(12), dp(16), dp(12));

        connectButton = Ui.button(this, "Підключитися", true);
        connectButton.setOnClickListener(v -> {
            Radio r = selectedRadio();
            if (r != null) openRadio(r);
        });
        buttons.addView(connectButton, new LinearLayout.LayoutParams(-1, dp(50)));

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        againButton = Ui.button(this, "Шукати ще раз", false);
        againButton.setOnClickListener(v -> startSearch(true, null));
        manualButton = Ui.button(this, "Ввести адресу…", false);
        manualButton.setOnClickListener(v -> askAddress(null));
        LinearLayout.LayoutParams a = new LinearLayout.LayoutParams(0, dp(48), 1f);
        a.rightMargin = dp(5);
        LinearLayout.LayoutParams m = new LinearLayout.LayoutParams(0, dp(48), 1f);
        m.leftMargin = dp(5);
        row.addView(againButton, a);
        row.addView(manualButton, m);
        LinearLayout.LayoutParams rl = new LinearLayout.LayoutParams(-1, -2);
        rl.topMargin = dp(10);
        buttons.addView(row, rl);

        bar.addView(buttons, new FrameLayout.LayoutParams(-1, -2, Gravity.CENTER_HORIZONTAL));
        screen.addView(bar, new LinearLayout.LayoutParams(-1, -2));
        return screen;
    }

    /** Тло нижньої панелі: #111 з лінією #262626 по верхньому краю. */
    private android.graphics.drawable.Drawable topLine() {
        android.graphics.drawable.ColorDrawable line = new android.graphics.drawable.ColorDrawable(Ui.LINE);
        android.graphics.drawable.ColorDrawable fill = new android.graphics.drawable.ColorDrawable(Ui.PAN);
        android.graphics.drawable.LayerDrawable ld =
                new android.graphics.drawable.LayerDrawable(new android.graphics.drawable.Drawable[]{line, fill});
        ld.setLayerInset(1, 0, Math.max(1, dp(1)), 0, 0);
        return ld;
    }

    /**
     * @param auto    відкрити самому, якщо знайдеться рівно одне радіо. Не
     *                тоді, коли людина сама пішла шукати інше: інакше «Знайти
     *                інше радіо» чи «Назад» одразу ж повертали б її туди, звідки
     *                вона щойно вийшла.
     * @param message що сказати над пошуком (наприклад, що зв'язок втрачено)
     */
    private void startSearch(boolean auto, String message) {
        if (finder != null) finder.cancel();
        showSearchScreen();
        searchGen++;
        autoOpen = auto;
        found.clear();
        selected = null;
        renderList();
        setNote(message, true);
        setBusy("Шукаю радіо в мережі…");
        searching = true;

        android.net.Network network = Net.localNetwork(this);
        Net.bindTo(this, network);
        final Finder[] self = new Finder[1];
        self[0] = new Finder(this, new Finder.Listener() {
            @Override
            public void onFound(Radio radio) {
                if (finder != self[0]) return;
                addFound(radio);
            }

            @Override
            public void onDone(List<String> networks) {
                if (finder != self[0]) return;
                finder = null;
                searchDone(networks);
            }
        });
        finder = self[0];
        finder.start(network);
        List<String> nets = finder.networks();
        hint.setText(nets.isEmpty() ? "Телефон зараз не в мережі Wi-Fi"
                : "Мережа " + nets.get(0) + "0/24" + (nets.size() > 1 ? " і ще " + (nets.size() - 1) : ""));
        updateButtons();
    }

    private void addFound(Radio r) {
        for (int i = 0; i < found.size(); i++) {
            Radio f = found.get(i);
            if (f.address.equals(r.address)) {
                found.set(i, r);
                renderList();
                return;
            }
            // Ім'я (potuzhne-c447d4) у кожного радіо своє — з адреси плати.
            // Те саме ім'я за другою адресою — те саме радіо, не друге.
            if (!r.host.isEmpty() && r.host.equals(f.host)) return;
        }
        found.add(r);
        Collections.sort(found, (x, y) -> Long.compare(x.sortKey(), y.sortKey()));
        if (selected == null) selected = r.address;
        renderList();
        updateButtons();
    }

    private void searchDone(List<String> networks) {
        searching = false;
        spinner.setVisibility(View.GONE);
        if (found.size() == 1 && autoOpen && !brokenPages.contains(found.get(0).address)) {
            openRadio(found.get(0));
            return;
        }
        if (found.isEmpty()) {
            status.setText("Радіо не знайдено. Перевірте, що телефон і радіо в одній мережі Wi-Fi");
            if (networks.isEmpty()) {
                hint.setText("Телефон зараз не в мережі Wi-Fi");
            } else {
                StringBuilder sb = new StringBuilder("Перевірено адреси ");
                for (int i = 0; i < networks.size(); i++) {
                    if (i > 0) sb.append(", ");
                    sb.append(networks.get(i)).append("1–254");
                }
                hint.setText(sb.toString());
            }
        } else if (found.size() == 1) {
            status.setText("Знайдено радіо");
            hint.setText("");
        } else {
            status.setText("Знайдено радіо: " + found.size() + ". Виберіть потрібне");
            hint.setText("");
        }
        updateButtons();
    }

    private void renderList() {
        list.removeAllViews();
        for (Radio r : found) list.addView(card(r));
    }

    /**
     * Картка знайденого радіо. Дотик вибирає (жовта рамка), дотик по вже
     * вибраній — під'єднує; те саме робить кнопка «Підключитися» знизу.
     */
    private View card(Radio r) {
        boolean on = r.address.equals(selected);
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.HORIZONTAL);
        c.setGravity(Gravity.CENTER_VERTICAL);
        c.setPadding(dp(14), dp(12), dp(14), dp(12));
        android.graphics.drawable.GradientDrawable bg = Ui.panel(this, on ? 0xff15140c : Ui.PAN, on ? Ui.ACC : Ui.LINE, 12);
        if (on) bg.setStroke(dp(2), Ui.ACC);
        c.setBackground(new android.graphics.drawable.RippleDrawable(
                android.content.res.ColorStateList.valueOf(0x30ffffff), bg, Ui.panel(this, 0xffffffff, 0, 12)));
        c.setClickable(true);
        c.setFocusable(true);

        View icon = new View(this);
        icon.setBackground(new Ui.Antenna(on ? Ui.ACC : Ui.DIM));
        LinearLayout.LayoutParams il = new LinearLayout.LayoutParams(dp(36), dp(36));
        il.rightMargin = dp(14);
        c.addView(icon, il);

        LinearLayout texts = new LinearLayout(this);
        texts.setOrientation(LinearLayout.VERTICAL);
        TextView name = Ui.text(this, "ПОТУЖНЕ РАДІО", 15, Ui.TXT);
        name.setTypeface(Ui.MEDIUM);
        name.setLetterSpacing(0.04f);
        texts.addView(name);
        TextView sub = Ui.text(this, r.subtitle(), 13, Ui.DIM);
        texts.addView(sub);
        if (!r.station.isEmpty()) {
            TextView st = Ui.text(this, (r.playing ? "▶ " : "") + r.station, 14, r.playing ? Ui.ACC : Ui.TXT);
            st.setSingleLine(true);
            st.setEllipsize(android.text.TextUtils.TruncateAt.END);
            LinearLayout.LayoutParams stl = new LinearLayout.LayoutParams(-1, -2);
            stl.topMargin = dp(4);
            texts.addView(st, stl);
        }
        c.addView(texts, new LinearLayout.LayoutParams(0, -2, 1f));

        c.setContentDescription("ПОТУЖНЕ РАДІО, " + r.subtitle()
                + (r.station.isEmpty() ? "" : ", " + r.station) + (on ? ", вибрано" : ""));
        c.setOnClickListener(v -> {
            if (r.address.equals(selected)) {
                openRadio(r);
            } else {
                selected = r.address;
                renderList();
            }
        });

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = dp(10);
        c.setLayoutParams(lp);
        return c;
    }

    private Radio selectedRadio() {
        for (Radio r : found) if (r.address.equals(selected)) return r;
        return found.isEmpty() ? null : found.get(0);
    }

    private void setBusy(String text) {
        spinner.setVisibility(View.VISIBLE);
        status.setText(text);
    }

    private void setIdle(String text) {
        spinner.setVisibility(View.GONE);
        status.setText(text);
    }

    private void setNote(String text, boolean warn) {
        if (text == null || text.isEmpty()) {
            note.setVisibility(View.GONE);
            return;
        }
        note.setText(text);
        note.setTextColor(warn ? Ui.WARN : Ui.TXT);
        note.setVisibility(View.VISIBLE);
    }

    private void updateButtons() {
        Ui.enable(connectButton, !found.isEmpty());
        Ui.enable(againButton, !searching);
    }

    private void showSearchScreen() {
        closeRadio();
        searchScreen.setVisibility(View.VISIBLE);
        backToSearchOnBack(false);
    }

    // ---- адреса вручну ----------------------------------------------------------

    private void askAddress(String preset) {
        EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        input.setImeOptions(EditorInfo.IME_ACTION_GO);
        input.setHint("192.168.1.32");
        input.setTextColor(Ui.TXT);
        input.setHintTextColor(Ui.DIM2);
        String start = preset != null ? preset : prefs.getString("manual", prefs.getString("address", ""));
        input.setText(start);
        input.setSelection(input.getText().length());

        FrameLayout box = new FrameLayout(this);
        box.setPadding(dp(22), dp(4), dp(22), 0);
        box.addView(input, new FrameLayout.LayoutParams(-1, -2));

        AlertDialog d = new AlertDialog.Builder(this)
                .setTitle("Адреса радіо")
                .setMessage("IP-адресу видно на екрані радіо, якщо в його налаштуваннях "
                        + "увімкнено «IP-адреса на екрані». Можна й ім'я, наприклад "
                        + "potuzhne-c447d4.local.")
                .setView(box)
                .setPositiveButton("Підключитися", null)
                .setNegativeButton("Скасувати", null)
                .create();
        Runnable go = () -> {
            String address = Probe.normalise(input.getText().toString());
            if (address == null) {
                input.setError("Введіть адресу, наприклад 192.168.1.32");
                return;
            }
            d.dismiss();
            connectManual(address);
        };
        d.setOnShowListener(x -> d.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> go.run()));
        input.setOnEditorActionListener((v, action, ev) -> {
            if (action == EditorInfo.IME_ACTION_GO
                    || (ev != null && ev.getKeyCode() == KeyEvent.KEYCODE_ENTER && ev.getAction() == KeyEvent.ACTION_DOWN)) {
                go.run();
                return true;
            }
            return false;
        });
        if (d.getWindow() != null) {
            d.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
        }
        d.show();
    }

    private void connectManual(String address) {
        if (finder != null) {
            finder.cancel();
            finder = null;
        }
        searching = false;
        int gen = ++searchGen;
        showSearchScreen();
        setNote(null, false);
        setBusy("Перевіряю " + address + "…");
        hint.setText("");
        updateButtons();
        io.execute(() -> {
            Net.bindTo(this, Net.localNetwork(this));
            Radio r = null;
            String problem;
            try {
                r = Probe.check(address, 2500, 4000);
                problem = null;
            } catch (Probe.NotRadio e) {
                problem = "За адресою " + address + " відповідає не ПОТУЖНЕ РАДІО.";
            } catch (UnknownHostException e) {
                problem = "Не вдалося знайти ім'я " + address + ". Спробуйте IP-адресу.";
            } catch (IOException e) {
                problem = "За адресою " + address + " ніхто не відповідає. Перевірте адресу "
                        + "і те, що телефон і радіо в одній мережі Wi-Fi.";
            }
            final Radio radio = r;
            final String why = problem;
            main.post(() -> {
                if (gen != searchGen || isFinishing()) return;
                if (radio != null) {
                    prefs.edit().putString("manual", address).apply();
                    openRadio(radio);
                    return;
                }
                setIdle(found.isEmpty() ? "Радіо не під'єднано" : "Виберіть радіо");
                new AlertDialog.Builder(this)
                        .setTitle("Не вдалося")
                        .setMessage(why)
                        .setPositiveButton("Змінити адресу", (dd, w) -> askAddress(address))
                        .setNegativeButton("Закрити", null)
                        .show();
            });
        });
    }

    // =========================================================================
    // Сторінка радіо
    // =========================================================================

    private void openRadio(Radio r) {
        if (finder != null) {
            finder.cancel();
            finder = null;
        }
        searching = false;
        searchGen++;
        closeRadio();

        current = r;
        openGen++;
        checkFails = 0;
        pageShown = false;
        loadRetries = 0;
        Log.i(TAG, "відкриваю " + r.url());

        buildRadioScreen();
        searchScreen.setVisibility(View.GONE);
        backToSearchOnBack(true);
        warnOldWebView();

        web.loadUrl(r.url());
        main.postDelayed(pageTimeout, PAGE_TIMEOUT_MS);
        main.removeCallbacks(check);
        main.postDelayed(check, CHECK_EVERY_MS);
    }

    /**
     * Прибрати сторінку зовсім — разом зі звуком, історією переходів і
     * станом скриптів. Щоразу новий WebView чистіший за «очистити старий»:
     * clearHistory, наприклад, спрацьовує лише після наступного завантаження.
     */
    private void closeRadio() {
        main.removeCallbacks(check);
        main.removeCallbacks(pageTimeout);
        main.removeCallbacks(menuCheck);
        if (fileCallback != null) {
            fileCallback.onReceiveValue(null);
            fileCallback = null;
        }
        if (web != null) {
            WebView w = web;
            web = null;
            w.stopLoading();
            w.setWebViewClient(new WebViewClient());
            w.setWebChromeClient(null);
            if (radioScreen != null) radioScreen.removeView(w);
            w.destroy();
        }
        if (radioScreen != null) {
            root.removeView(radioScreen);
            radioScreen = null;
        }
        if (current != null) openGen++;
        current = null;
    }

    private void buildRadioScreen() {
        radioScreen = new FrameLayout(this);
        radioScreen.setBackgroundColor(Ui.BG);

        web = new WebView(this);
        web.setBackgroundColor(Ui.BG);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        // Сторінка тримає свої налаштування вигляду в localStorage — без
        // цього рядка вона щоразу починала б наче вперше.
        s.setDomStorageEnabled(true);
        s.setUseWideViewPort(true);
        s.setLoadWithOverviewMode(true);
        s.setBuiltInZoomControls(true);
        s.setDisplayZoomControls(false);
        s.setAllowFileAccess(false);
        s.setUserAgentString(s.getUserAgentString() + " PotuzhneRadioApp/1.0");
        web.addJavascriptInterface(new Bridge(openGen), "PotuzhneApp");
        web.setWebViewClient(new PageClient());
        web.setWebChromeClient(new PageChrome());
        web.setDownloadListener((url, agent, disposition, mime, length) -> {
            // blob: і data: сторінка віддає сама через PotuzhneApp.save —
            // WebView такі посилання зберігати не вміє.
            if (url == null || url.startsWith("blob:") || url.startsWith("data:")) return;
            startDownload(url, "", mime, disposition, false);
        });
        radioScreen.addView(web, new FrameLayout.LayoutParams(-1, -1));

        cover = buildCover();
        radioScreen.addView(cover, new FrameLayout.LayoutParams(-1, -1));

        // Запасна кнопка меню — на випадок, якщо у сторінці не знайдеться
        // шапки, куди покласти свою (див. HELPERS).
        FrameLayout fab = new FrameLayout(this);
        fab.setBackground(new android.graphics.drawable.RippleDrawable(
                android.content.res.ColorStateList.valueOf(0x30ffffff),
                ovalPanel(), ovalPanel()));
        View dots = new View(this);
        dots.setBackground(new Ui.Dots(Ui.TXT));
        fab.addView(dots, new FrameLayout.LayoutParams(dp(22), dp(22), Gravity.CENTER));
        fab.setContentDescription("Меню застосунку");
        fab.setOnClickListener(v -> showMenu());
        fab.setVisibility(View.GONE);
        FrameLayout.LayoutParams fl = new FrameLayout.LayoutParams(dp(42), dp(42), Gravity.TOP | Gravity.END);
        fl.topMargin = dp(7);
        fl.rightMargin = dp(8);
        radioScreen.addView(fab, fl);
        menuButton = fab;

        root.addView(radioScreen, new FrameLayout.LayoutParams(-1, -1));
    }

    private android.graphics.drawable.GradientDrawable ovalPanel() {
        android.graphics.drawable.GradientDrawable g = new android.graphics.drawable.GradientDrawable();
        g.setShape(android.graphics.drawable.GradientDrawable.OVAL);
        g.setColor(0xe6111111);
        g.setStroke(Math.max(1, dp(1)), Ui.LINE2);
        return g;
    }

    /** Заставка, поки сторінка радіо вантажиться (радіо віддає її секунду-дві). */
    private View buildCover() {
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setGravity(Gravity.CENTER);
        c.setBackgroundColor(Ui.BG);
        c.setPadding(dp(32), dp(32), dp(32), dp(32));
        c.setClickable(true);   // дотики не мають проходити до сторінки під заставкою

        View logo = new View(this);
        logo.setBackground(new Ui.Antenna(Ui.ACC));
        c.addView(logo, new LinearLayout.LayoutParams(dp(64), dp(64)));

        coverText = Ui.text(this, "", 16, Ui.TXT);
        coverText.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams tl = new LinearLayout.LayoutParams(-2, -2);
        tl.topMargin = dp(18);
        c.addView(coverText, tl);
        setCover("Відкриваю сторінку радіо…\n" + (current != null ? current.subtitle() : ""));

        ProgressBar p = new ProgressBar(this);
        p.setIndeterminate(true);
        p.setIndeterminateTintList(android.content.res.ColorStateList.valueOf(Ui.ACC));
        LinearLayout.LayoutParams pl = new LinearLayout.LayoutParams(dp(28), dp(28));
        pl.topMargin = dp(18);
        c.addView(p, pl);

        Button back = Ui.button(this, "До пошуку", false);
        back.setOnClickListener(v -> startSearch(false, null));
        LinearLayout.LayoutParams bl = new LinearLayout.LayoutParams(-2, dp(44));
        bl.topMargin = dp(28);
        c.addView(back, bl);
        return c;
    }

    private void setCover(String text) {
        if (coverText != null) coverText.setText(text);
    }

    /** Сторінка так і не прийшла — вважаємо помилкою завантаження. */
    private final Runnable pageTimeout = () -> {
        if (web != null && !pageShown) {
            web.stopLoading();
            pageFailed("радіо не віддало сторінку");
        }
    };

    /**
     * Сторінка не завантажилась. Спершу питаємо саме радіо: живе — пробуємо
     * ще раз (радіо буває зайняте звуком і не встигає відповісти); мовчить —
     * повертаємось до пошуку зі словами, що сталося.
     */
    private void pageFailed(String why) {
        if (current == null) return;
        Radio r = current;
        int gen = openGen;
        main.removeCallbacks(pageTimeout);
        cover.setVisibility(View.VISIBLE);
        setCover("Сторінка не відкрилася (" + why + ").\nПеревіряю радіо…");
        io.execute(() -> {
            boolean alive = Probe.alive(r, 2000, 3000);
            main.post(() -> {
                if (gen != openGen || web == null) return;
                if (alive && loadRetries < 2) {
                    loadRetries++;
                    pageShown = false;
                    setCover("Відкриваю сторінку радіо ще раз…\n" + r.subtitle());
                    web.loadUrl(r.url());
                    main.postDelayed(pageTimeout, PAGE_TIMEOUT_MS);
                } else if (alive) {
                    brokenPages.add(r.address);
                    lost("Радіо відповідає, але його сторінка не відкривається. Спробуйте трохи згодом.");
                } else {
                    lost("Зв'язок з радіо втрачено");
                }
            });
        });
    }

    private void lost(String message) {
        Log.i(TAG, "повертаюсь до пошуку: " + message);
        closeRadio();
        startSearch(true, message);
    }

    /**
     * Перевірка раз на 10 секунд: радіо відповідає на /api/hello? Три відмови
     * поспіль — зв'язок втрачено. Одну-дві пропускаємо: радіо, зайняте
     * звуком чи перебудовою списку станцій, іноді відповідає із запізненням.
     */
    private final Runnable check = new Runnable() {
        @Override
        public void run() {
            if (current == null) return;
            Radio r = current;
            int gen = openGen;
            io.execute(() -> {
                boolean ok = Probe.alive(r, 2000, 3000);
                main.post(() -> {
                    if (gen != openGen || current == null) return;
                    checkFails = ok ? 0 : checkFails + 1;
                    if (!ok) Log.i(TAG, "радіо не відповіло (" + checkFails + " з " + CHECK_FAILS + ")");
                    if (checkFails >= CHECK_FAILS) {
                        lost("Зв'язок з радіо втрачено");
                        return;
                    }
                    main.removeCallbacks(check);
                    if (started) main.postDelayed(check, CHECK_EVERY_MS);
                });
            });
        }
    };

    private void remember(Radio r) {
        prefs.edit().putString("address", r.address).putString("host", r.host).apply();
    }

    // ---- меню -------------------------------------------------------------------

    private void showMenu() {
        if (current == null) return;
        Radio r = current;
        new AlertDialog.Builder(this)
                .setTitle(r.subtitle())
                .setItems(new String[]{"Голосові команди", "Знайти інше радіо", "Оновити", "Відкрити в браузері"}, (d, which) -> {
                    if (current != r) return;
                    if (which == 0) {
                        showVoiceHelp();
                    } else if (which == 1) {
                        startSearch(false, null);
                    } else if (which == 2) {
                        reload();
                    } else {
                        openInBrowser(r);
                    }
                })
                .show();
    }

    /**
     * Інструкція з голосових команд — розділ самої сторінки радіо: там і
     * приклади, і перевірка фрази, однакові для всіх програм.
     */
    private void showVoiceHelp() {
        if (web == null || current == null) return;
        web.evaluateJavascript("location.hash = '#/voice'", null);
    }

    /**
     * Оновити — з очищенням кешу. Старі прошивки віддавали сторінку з
     * дозволом кешувати її рік, і після оновлення радіо WebView показував би
     * стару версію; звичайне «перезавантажити» цього не знімає.
     */
    private void reload() {
        if (web == null || current == null) return;
        web.clearCache(true);
        pageShown = false;
        loadRetries = 0;
        cover.setVisibility(View.VISIBLE);
        setCover("Оновлюю сторінку радіо…\n" + current.subtitle());
        web.loadUrl(current.url());
        main.removeCallbacks(pageTimeout);
        main.postDelayed(pageTimeout, PAGE_TIMEOUT_MS);
    }

    private void openInBrowser(Radio r) {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(r.url())).addCategory(Intent.CATEGORY_BROWSABLE));
        } catch (ActivityNotFoundException e) {
            toast("На телефоні немає браузера");
        }
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        // Апаратна кнопка «Меню» на старих телефонах — те саме меню.
        if (keyCode == KeyEvent.KEYCODE_MENU && current != null) {
            showMenu();
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    // ---- «Назад» ------------------------------------------------------------------

    /**
     * «Назад» на сторінці радіо: спершу гортає її власну історію (розділи
     * сторінки — це переходи за #), потім повертає до пошуку. На екрані
     * пошуку «Назад» — звичайний вихід.
     */
    private boolean handleBack() {
        if (web == null || current == null) return false;
        if (web.canGoBack()) web.goBack();
        else startSearch(false, null);
        return true;
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        // Android до 13. На новіших сюди вже не доходить — див. Back33.
        if (!handleBack()) super.onBackPressed();
    }

    private void backToSearchOnBack(boolean on) {
        if (Build.VERSION.SDK_INT < 33) return;
        backCallback = Back33.set(this, backCallback, on, this::handleBack);
    }

    /**
     * Новий спосіб слухати «Назад» (Android 13+). Окремий клас, щоб старі
     * Android його навіть не завантажували.
     */
    private static final class Back33 {
        static Object set(Activity a, Object existing, boolean on, Runnable action) {
            android.window.OnBackInvokedDispatcher d = a.getOnBackInvokedDispatcher();
            if (on && existing == null) {
                android.window.OnBackInvokedCallback cb = action::run;
                d.registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT, cb);
                return cb;
            }
            if (!on && existing != null) {
                d.unregisterOnBackInvokedCallback((android.window.OnBackInvokedCallback) existing);
                return null;
            }
            return existing;
        }
    }

    // ---- WebView: сторінка --------------------------------------------------------

    private final class PageClient extends WebViewClient {

        @Override
        public void onPageStarted(WebView view, String url, Bitmap favicon) {
            if (view != web) return;
            Log.d(TAG, "вантажу " + url);
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            if (view != web || current == null || url == null || !url.startsWith("http")) return;
            view.evaluateJavascript(HELPERS, null);
            if (!pageShown) {
                pageShown = true;
                main.removeCallbacks(pageTimeout);
                cover.setVisibility(View.GONE);
                remember(current);
                brokenPages.remove(current.address);
            }
            main.removeCallbacks(menuCheck);
            main.postDelayed(menuCheck, 2500);
        }

        /** Android 5–5.1: інших повідомлень про помилку там немає. */
        @Override
        @SuppressWarnings("deprecation")
        public void onReceivedError(WebView view, int code, String description, String failingUrl) {
            if (Build.VERSION.SDK_INT >= 23 || view != web) return;
            pageFailed(description);
        }

        /** Android 6+: цікавить лише сама сторінка, а не картинка чи запит зі сторінки. */
        @Override
        public void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error) {
            if (view != web || request == null || !request.isForMainFrame()) return;
            pageFailed(String.valueOf(error.getDescription()));
        }

        @Override
        @SuppressWarnings("deprecation")
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
            return leavesRadio(url);
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
            return leavesRadio(request.getUrl().toString());
        }

        /**
         * Процес сторінки впав (буває на слабких телефонах при нестачі
         * пам'яті). Без цього Android закрив би й застосунок; так —
         * відкриваємо сторінку наново.
         */
        @Override
        public boolean onRenderProcessGone(WebView view, RenderProcessGoneDetail detail) {
            if (view == web && current != null) {
                Radio r = current;
                main.post(() -> openRadio(r));
            }
            return true;
        }
    }

    /**
     * Посилання на саме радіо лишаються в сторінці; решта (сайти станцій,
     * каталог radio-browser, пошта) — зовнішньому браузеру: всередині
     * застосунку з них не було б дороги назад.
     */
    private boolean leavesRadio(String url) {
        if (url == null) return false;
        Uri u = Uri.parse(url);
        String scheme = u.getScheme() == null ? "" : u.getScheme().toLowerCase(java.util.Locale.ROOT);
        if (scheme.equals("about") || scheme.equals("blob") || scheme.equals("data") || scheme.equals("javascript")) {
            return false;
        }
        if ((scheme.equals("http") || scheme.equals("https")) && sameRadio(u)) return false;
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, u).addCategory(Intent.CATEGORY_BROWSABLE));
        } catch (Exception e) {
            toast("Немає програми, щоб відкрити це посилання");
        }
        return true;
    }

    private boolean sameRadio(Uri u) {
        if (current == null || u.getHost() == null) return false;
        int port = u.getPort();
        String hp = u.getHost() + (port > 0 && port != 80 ? ":" + port : "");
        return hp.equalsIgnoreCase(current.address);
    }

    private final class PageChrome extends WebChromeClient {

        /**
         * <input type=file> — вибір файла системним вікном. Без цього поле
         * на сторінці (логотип станції, імпорт списку, файли прошивки)
         * просто не реагує на дотик.
         */
        @Override
        public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback, FileChooserParams params) {
            if (fileCallback != null) fileCallback.onReceiveValue(null);
            fileCallback = callback;
            Intent pick = new Intent(Intent.ACTION_GET_CONTENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            String[] types = mimeTypes(params.getAcceptTypes());
            if (types.length == 1) {
                pick.setType(types[0]);
            } else {
                pick.setType("*/*");
                if (types.length > 1) pick.putExtra(Intent.EXTRA_MIME_TYPES, types);
            }
            if (params.getMode() == FileChooserParams.MODE_OPEN_MULTIPLE) {
                pick.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
            }
            try {
                startActivityForResult(pick, REQUEST_FILE);
            } catch (ActivityNotFoundException e) {
                try {
                    startActivityForResult(Intent.createChooser(pick, "Виберіть файл"), REQUEST_FILE);
                } catch (Exception e2) {
                    fileCallback = null;
                    toast("На телефоні немає програми для вибору файлів");
                    return false;
                }
            }
            return true;
        }

        @Override
        public boolean onConsoleMessage(ConsoleMessage m) {
            Log.d(TAG, "сторінка: " + m.message() + " (" + m.sourceId() + ":" + m.lineNumber() + ")");
            return true;
        }
    }

    /**
     * Типи файлів з accept — лише ті, що мають вигляд MIME. Розширення на
     * кшталт «.csv» система вибору файлів не розуміє: якщо серед accept є
     * хоч одне розширення, показуємо всі файли, а відбір лишаємо сторінці.
     */
    private static String[] mimeTypes(String[] accept) {
        List<String> out = new ArrayList<>();
        if (accept != null) {
            for (String a : accept) {
                if (a == null) continue;
                for (String part : a.split(",")) {
                    String t = part.trim().toLowerCase(java.util.Locale.ROOT);
                    if (t.isEmpty()) continue;
                    if (!t.contains("/")) return new String[0];
                    if (!out.contains(t)) out.add(t);
                }
            }
        }
        return out.toArray(new String[0]);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == REQUEST_VOICE) { voiceAnswer(result, data); return; }
        if (request != REQUEST_FILE) return;
        Uri[] picked = null;
        if (result == RESULT_OK && data != null) {
            ClipData clip = data.getClipData();
            if (clip != null && clip.getItemCount() > 0) {
                picked = new Uri[clip.getItemCount()];
                for (int i = 0; i < picked.length; i++) picked[i] = clip.getItemAt(i).getUri();
            } else if (data.getData() != null) {
                picked = new Uri[]{data.getData()};
            }
        }
        // Відповісти треба завжди, і «нічого» теж: інакше поле вибору файла
        // на сторінці більше не відкриється до перезавантаження.
        if (fileCallback != null) fileCallback.onReceiveValue(picked);
        fileCallback = null;
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(request, permissions, results);
        if (request == Saver.REQUEST) saver.onPermissionAnswer();
    }

    // ---- WebView: файли й меню з боку сторінки ---------------------------------------

    /**
     * Що підкладається в кожну сторінку радіо після завантаження:
     *
     * 1. Кнопка меню застосунку «⋮» — у шапку самої сторінки, праворуч. Не
     *    плаваюча поверх: плаваюча закривала б або значки стану в шапці, або
     *    нижній програвач. Якщо шапки немає — з'явиться рідна запасна кнопка.
     *
     * 2. Збереження файлів. Експорт сторінка збирає в пам'яті (blob:) і
     *    «натискає» невидиме посилання з download — WebView на таке не
     *    реагує взагалі. Перехоплюємо натискання, читаємо зібраний файл і
     *    віддаємо застосунку. Звичайні посилання з download (записи, список
     *    станцій) — теж через застосунок: так файл отримує ім'я з атрибута,
     *    а не «rec.bin» з адреси.
     *
     *    Межа 25 МБ: щоб передати файл сюди, сторінка тримає його в пам'яті
     *    ще раз текстом (base64); експорт важить кілобайти, а записи ефіру
     *    ідуть другим шляхом.
     *
     * Клавіші з кнопки меню далі не пропускаємо: сторінка слухає пробіл як
     * «пауза», а натиснута кнопка має фокус.
     */
    private static final String HELPERS = """
        (function () {
          if (window.__potuzhneApp) return;
          window.__potuzhneApp = true;
          var LIMIT = 25 * 1024 * 1024;

          // XMLHttpRequest, а не fetch: у WebView старих Android fetch немає.
          function saveBlob(href, name) {
            var x = new XMLHttpRequest();
            x.open('GET', href);
            x.responseType = 'blob';
            x.onload = function () {
              var b = x.response;
              if (!b) { PotuzhneApp.failed(name); return; }
              if (b.size > LIMIT) { PotuzhneApp.tooBig(name, b.size / 1048576); return; }
              var fr = new FileReader();
              fr.onloadend = function () {
                var s = String(fr.result || '');
                PotuzhneApp.save(name, b.type || '', s.slice(s.indexOf(',') + 1));
              };
              fr.onerror = function () { PotuzhneApp.failed(name); };
              fr.readAsDataURL(b);
            };
            x.onerror = function () { PotuzhneApp.failed(name); };
            x.send();
          }
          function nameOf(a) { return a.getAttribute('download') || ''; }
          function linkOf(el) {
            for (; el && el !== document; el = el.parentNode) {
              if (el.tagName === 'A' && el.hasAttribute('download')) return el;
            }
            return null;
          }

          var click = HTMLAnchorElement.prototype.click;
          HTMLAnchorElement.prototype.click = function () {
            var href = this.href || '';
            if (this.hasAttribute('download') && /^(blob|data):/i.test(href)) {
              saveBlob(href, nameOf(this) || 'файл');
              return;
            }
            return click.apply(this, arguments);
          };
          document.addEventListener('click', function (e) {
            var a = linkOf(e.target);
            if (!a) return;
            var href = a.href || '';
            if (/^(blob|data):/i.test(href)) {
              e.preventDefault(); e.stopPropagation();
              saveBlob(href, nameOf(a) || 'файл');
            } else if (/^https?:/i.test(href)) {
              e.preventDefault(); e.stopPropagation();
              PotuzhneApp.download(href, nameOf(a));
            }
          }, true);

          function place() {
            if (document.getElementById('__potuzhne_menu')) return true;
            var top = document.querySelector('header.top') || document.querySelector('.top');
            if (!top) return false;
            var b = document.createElement('button');
            b.id = '__potuzhne_menu';
            b.type = 'button';
            b.title = 'Меню застосунку';
            b.setAttribute('aria-label', 'Меню застосунку');
            b.style.cssText = 'flex:none;width:40px;height:40px;margin:0 -8px 0 0;padding:0;border:0;'
              + 'border-radius:10px;background:transparent;color:#8c8c8c;display:inline-flex;'
              + 'align-items:center;justify-content:center;cursor:pointer;-webkit-tap-highlight-color:transparent';
            b.innerHTML = '<svg width="22" height="22" viewBox="0 0 24 24" fill="currentColor">'
              + '<circle cx="12" cy="5" r="2"/><circle cx="12" cy="12" r="2"/><circle cx="12" cy="19" r="2"/></svg>';
            b.addEventListener('click', function (e) { e.preventDefault(); e.stopPropagation(); PotuzhneApp.menu(); });
            b.addEventListener('keydown', function (e) { e.stopPropagation(); });
            top.appendChild(b);
            return true;
          }
          if (!place()) {
            var mo = new MutationObserver(function () { if (place()) mo.disconnect(); });
            mo.observe(document.documentElement, { childList: true, subtree: true });
            setTimeout(function () { mo.disconnect(); }, 10000);
          }
        })();
        """;

    /** Чи стала своя кнопка меню в шапку сторінки; якщо ні — показати запасну. */
    private final Runnable menuCheck = () -> {
        if (web == null) return;
        web.evaluateJavascript("!!document.getElementById('__potuzhne_menu')", value -> {
            if (menuButton == null) return;
            menuButton.setVisibility("true".equals(value) ? View.GONE : View.VISIBLE);
        });
    };

    /**
     * Те, що сторінка кличе як window.PotuzhneApp. Виклики приходять не з
     * головного потоку — усе, що чіпає екран, переносимо туди.
     */
    private final class Bridge {
        private final int gen;

        Bridge(int gen) { this.gen = gen; }

        private boolean mine() { return gen == openGen && current != null; }

        /** Голосова команда: сторінка просить послухати. */
        @JavascriptInterface
        public void listen() {
            main.post(() -> { if (mine()) startVoice(); });
        }

        @JavascriptInterface
        public void menu() {
            main.post(() -> { if (mine()) showMenu(); });
        }

        @JavascriptInterface
        public void download(String url, String name) {
            main.post(() -> { if (mine()) startDownload(url, name, null, null, true); });
        }

        @JavascriptInterface
        public void save(String name, String mime, String base64) {
            final byte[] bytes;
            try {
                bytes = Base64.decode(base64, Base64.DEFAULT);
            } catch (Exception e) {
                failed(name);
                return;
            }
            String clean = Saver.cleanName(name, "файл");
            main.post(() -> saver.withStorage(() -> io.execute(() -> {
                String msg = saver.saveBytes(clean, mime, bytes);
                main.post(() -> toast(msg));
            })));
        }

        @JavascriptInterface
        public void tooBig(String name, double megabytes) {
            main.post(() -> toast("«" + name + "» — " + Math.round(megabytes)
                    + " МБ, завеликий для збереження з телефона. Збережіть його з комп'ютера."));
        }

        @JavascriptInterface
        public void failed(String name) {
            main.post(() -> toast("Не вдалося зберегти «" + name + "»"));
        }
    }

    // ---- голос ----------------------------------------------------------------------

    /**
     * Розпізнавання мови — системним вікном Google українською. Текст (до п'яти
     * варіантів) іде в сторінку радіо, а що з ним робити, вирішує вона сама —
     * однаково для телефона, Mac і браузера.
     */
    private void startVoice() {
        Intent i = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        i.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
        i.putExtra(RecognizerIntent.EXTRA_LANGUAGE, "uk-UA");
        i.putExtra(RecognizerIntent.EXTRA_LANGUAGE_PREFERENCE, "uk-UA");
        i.putExtra(RecognizerIntent.EXTRA_PROMPT, "Скажіть команду радіо");
        i.putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 5);
        if (getPackageManager().queryIntentActivities(i, 0).isEmpty()) {
            voiceJs("potuzhneVoiceError", "На телефоні немає розпізнавання мови. Поставте або ввімкніть застосунок «Google».");
            return;
        }
        try {
            startActivityForResult(i, REQUEST_VOICE);
        } catch (ActivityNotFoundException e) {
            voiceJs("potuzhneVoiceError", "Розпізнавання мови не відкрилось.");
        }
    }

    private void voiceAnswer(int result, Intent data) {
        java.util.ArrayList<String> said = (result == RESULT_OK && data != null)
                ? data.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS) : null;
        org.json.JSONArray arr = new org.json.JSONArray();
        if (said != null) for (String t : said) arr.put(t);
        if (web != null) web.evaluateJavascript("window.potuzhneVoiceResult && potuzhneVoiceResult(" + arr + ")", null);
    }

    private void voiceJs(String fn, String msg) {
        if (web == null) return;
        web.evaluateJavascript("window." + fn + " && " + fn + "(" + org.json.JSONObject.quote(msg) + ")", null);
    }

    /**
     * Файл за посиланням — системному завантажувачу.
     *
     * @param fromPage посилання прийшло з перехоплення в сторінці; таким
     *                 довіряємо лише адреси самого радіо — сторінка з
     *                 локальної мережі по http могла б бути й підміненою.
     */
    private void startDownload(String url, String name, String mime, String disposition, boolean fromPage) {
        if (url == null) return;
        Uri u = Uri.parse(url);
        String scheme = u.getScheme() == null ? "" : u.getScheme();
        if (!scheme.equals("http") && !scheme.equals("https")) return;
        if (fromPage && !sameRadio(u)) {
            leavesRadio(url);
            return;
        }
        String file = Saver.cleanName(
                name == null || name.isEmpty() ? URLUtil.guessFileName(url, disposition, mime) : name,
                "файл");
        String agent = web != null ? web.getSettings().getUserAgentString() : Probe.AGENT;
        saver.withStorage(() -> toast(saver.download(url, file, mime, agent)));
    }

    /**
     * Сторінка радіо написана під сучасний браузер. На Android 5–7 з
     * неоновленим «Android System WebView» вона може лишитися чорною —
     * тоді краще сказати, що оновити, ніж мовчки показувати порожнечу.
     */
    private void warnOldWebView() {
        if (webViewWarned) return;
        webViewWarned = true;
        try {
            Matcher m = Pattern.compile("Chrome/(\\d+)").matcher(WebSettings.getDefaultUserAgent(this));
            if (m.find()) {
                int v = Integer.parseInt(m.group(1));
                Log.i(TAG, "WebView " + v);
                if (v < WEBVIEW_MIN) {
                    new AlertDialog.Builder(this)
                            .setTitle("Застарілий WebView")
                            .setMessage("Сторінці радіо потрібен новіший «Android System WebView» "
                                    + "(зараз стоїть версія " + v + ", треба " + WEBVIEW_MIN
                                    + " або новіша). Оновіть його в Google Play — інакше сторінка "
                                    + "може не показатися.")
                            .setPositiveButton("Зрозуміло", null)
                            .show();
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "версія WebView невідома", e);
        }
    }

    // =========================================================================

    private void toast(String text) {
        Toast.makeText(this, text, Toast.LENGTH_LONG).show();
    }

    private int dp(float v) {
        return Ui.dp(this, v);
    }
}
