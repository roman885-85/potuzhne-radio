package ua.potuzhne.radio;

import android.graphics.Insets;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;

/**
 * Відступити від годинника згори, від смуги навігації знизу — і від клавіатури.
 *
 * З Android 15 застосунок (targetSdk 35+) малює на весь екран, під системними
 * смугами включно: без відступів шапка сторінки радіо лізла б під годинник,
 * а її нижній програвач — під смугу навігації. Тло кореня чорне, тож у
 * відступах смуги виглядають чорними, як на старших Android зі statusBarColor.
 *
 * Клавіатура там само більше не стискає вікно сама (adjustResize не діє):
 * поле вводу на сторінці (назва станції, пароль Wi-Fi) опинялося б під нею.
 * Тому знизу відступ — більший із двох: смуга навігації або клавіатура.
 *
 * На Android 5–14 вікно й так стоїть між смугами, а клавіатуру обслуговує
 * adjustResize, тож тут нічого не робимо. Навмисно не «про всяк випадок»:
 * там система віддає вирізи камери окремо від уже врахованих смуг, і
 * відступ удвічі дав би чорну смугу під годинником.
 */
final class SystemBars {
    private SystemBars() {}

    static void keepClear(View view) {
        if (Build.VERSION.SDK_INT < 35) return;
        view.setOnApplyWindowInsetsListener((v, insets) -> {
            Insets bars = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            Insets ime = insets.getInsets(WindowInsets.Type.ime());
            v.setPadding(bars.left, bars.top, bars.right, Math.max(bars.bottom, ime.bottom));
            // Далі не передаємо: WebView, отримавши ті самі відступи, ще раз
            // зменшив би сторінку на висоту клавіатури — вийшло б удвічі.
            return WindowInsets.CONSUMED;
        });
        view.requestApplyInsets();
    }
}
