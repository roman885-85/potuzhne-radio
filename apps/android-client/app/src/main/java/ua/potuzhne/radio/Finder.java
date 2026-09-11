package ua.potuzhne.radio;

import android.content.Context;
import android.net.Network;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Пошук радіо в мережі — двома шляхами одночасно.
 *
 * 1. Пошук за іменем служби (DNS-SD, «_potuzhne._tcp»): радіо з прошивкою 1.0
 *    і новішою саме оголошує себе. Швидко, але не завжди доходить: деякі
 *    роутери глушать групові пакети, а старі Android плутаються з ними.
 * 2. Перебір своєї сотні адрес (/24): кожній GET /api/hello, по 32 водночас,
 *    чекаємо недовго. Повільніше, зате знаходить і стару прошивку, і радіо за
 *    роутером, що групових пакетів не пропускає.
 *
 * Що б не підказав шлях 1, у список потрапляє лише те, що відповіло на
 * /api/hello (або /api/state старої прошивки) — одна перевірка для обох.
 *
 * Усі зворотні виклики — у головному потоці.
 */
final class Finder {

    interface Listener {
        /** Знайдено радіо (одне й те саме може прийти двічі — із різних шляхів). */
        void onFound(Radio radio);

        /**
         * Пошук скінчився. networks — перебрані сотні адрес («192.168.1.»);
         * порожній список означає, що телефон не в Wi-Fi.
         */
        void onDone(List<String> networks);
    }

    private static final String SERVICE = "_potuzhne._tcp";
    private static final int PARALLEL = 32;
    private static final int CONNECT_MS = 800;
    private static final int READ_MS = 1500;
    /** Скільки слухати оголошення радіо. Перебір адрес іде паралельно й займає стільки ж. */
    private static final long NSD_WINDOW_MS = 6000;
    /** Далі не чекаємо ні на що — старі Android подеколи губляться в NSD назавжди. */
    private static final long HARD_STOP_MS = 15000;

    private final Context ctx;
    private final Listener listener;
    private final Handler main = new Handler(Looper.getMainLooper());

    private volatile boolean cancelled;
    private boolean finished;
    private boolean nsdWindowOver;
    private int pending;

    private ExecutorService sweep;
    private ExecutorService extra;
    private final Set<String> asked = new HashSet<>();
    private final List<String> networks = new ArrayList<>();

    private NsdManager nsd;
    private NsdManager.DiscoveryListener discovery;
    private final ArrayDeque<NsdServiceInfo> toResolve = new ArrayDeque<>();
    private final Set<String> retried = new HashSet<>();
    private boolean resolving;
    private WifiManager.MulticastLock multicast;

    Finder(Context ctx, Listener listener) {
        this.ctx = ctx.getApplicationContext();
        this.listener = listener;
    }

    void start(Network network) {
        sweep = Executors.newFixedThreadPool(PARALLEL);
        // Адреси від DNS-SD перевіряються окремо, щоб не стояти в черзі за
        // двома сотнями адрес перебору.
        extra = Executors.newFixedThreadPool(2);

        List<Inet4Address> own = Net.ownAddresses(ctx, network);
        Set<String> ownText = new HashSet<>();
        for (Inet4Address a : own) {
            ownText.add(a.getHostAddress());
            String base = Net.base24(a);
            if (!networks.contains(base)) networks.add(base);
        }
        Log.i(Net.TAG, "пошук: свої адреси " + ownText + ", перебираю " + networks);

        startNsd();

        for (String base : networks) {
            for (int i = 1; i <= 254; i++) {
                String ip = base + i;
                if (!ownText.contains(ip)) probe(ip, sweep);
            }
        }

        main.postDelayed(() -> { nsdWindowOver = true; maybeDone(); }, NSD_WINDOW_MS);
        main.postDelayed(this::finish, HARD_STOP_MS);
    }

    /** Які сотні адрес перебираються («192.168.1.»); порожньо — телефон не в Wi-Fi. */
    List<String> networks() {
        return new ArrayList<>(networks);
    }

    void cancel() {
        cancelled = true;
        finished = true;
        main.removeCallbacksAndMessages(null);
        stopNsd();
        shutdown();
    }

    // ------------------------------------------------------------------------

    private void probe(String address, ExecutorService pool) {
        if (finished || !asked.add(address)) return;
        pending++;
        try {
            pool.execute(() -> {
                Radio r = cancelled ? null : Probe.find(address, CONNECT_MS, READ_MS);
                main.post(() -> {
                    pending--;
                    if (!cancelled && r != null) {
                        Log.i(Net.TAG, "знайдено " + r.subtitle() + " — " + r.station);
                        listener.onFound(r);
                    }
                    maybeDone();
                });
            });
        } catch (Exception e) {       // пул уже зупинено
            pending--;
        }
    }

    private void maybeDone() {
        if (finished || pending > 0 || !nsdWindowOver) return;
        if (resolving || !toResolve.isEmpty()) return;
        finish();
    }

    private void finish() {
        if (finished) return;
        finished = true;
        main.removeCallbacksAndMessages(null);
        stopNsd();
        shutdown();
        listener.onDone(new ArrayList<>(networks));
    }

    private void shutdown() {
        if (sweep != null) sweep.shutdownNow();
        if (extra != null) extra.shutdownNow();
    }

    // ---- DNS-SD ----------------------------------------------------------------

    private void startNsd() {
        try {
            WifiManager wm = (WifiManager) ctx.getSystemService(Context.WIFI_SERVICE);
            if (wm != null) {
                multicast = wm.createMulticastLock("potuzhne-search");
                multicast.setReferenceCounted(false);
                multicast.acquire();
            }
        } catch (Exception e) {
            Log.w(Net.TAG, "без MulticastLock", e);
        }

        nsd = (NsdManager) ctx.getSystemService(Context.NSD_SERVICE);
        if (nsd == null) return;
        // Щоразу новий слухач: старі Android не приймають той самий удруге
        // («listener already in use»).
        discovery = new NsdManager.DiscoveryListener() {
            @Override public void onStartDiscoveryFailed(String type, int code) {
                Log.w(Net.TAG, "DNS-SD не стартував: " + code);
                main.post(() -> discovery = null);
            }
            @Override public void onStopDiscoveryFailed(String type, int code) {}
            @Override public void onDiscoveryStarted(String type) {
                Log.i(Net.TAG, "DNS-SD слухає " + type);
            }
            @Override public void onDiscoveryStopped(String type) {}
            @Override public void onServiceFound(NsdServiceInfo info) {
                Log.i(Net.TAG, "DNS-SD: " + info.getServiceName() + " " + info.getServiceType());
                main.post(() -> queueResolve(info));
            }
            @Override public void onServiceLost(NsdServiceInfo info) {}
        };
        try {
            nsd.discoverServices(SERVICE, NsdManager.PROTOCOL_DNS_SD, discovery);
        } catch (Exception e) {
            Log.w(Net.TAG, "DNS-SD недоступний", e);
            discovery = null;
        }
    }

    private void stopNsd() {
        if (nsd != null && discovery != null) {
            try {
                nsd.stopServiceDiscovery(discovery);
            } catch (Exception ignored) {
                // не встиг стартувати — зупиняти нічого
            }
        }
        discovery = null;
        toResolve.clear();
        if (multicast != null) {
            try {
                if (multicast.isHeld()) multicast.release();
            } catch (Exception ignored) {}
            multicast = null;
        }
    }

    private void queueResolve(NsdServiceInfo info) {
        if (finished) return;
        String type = info.getServiceType();
        if (type != null && !type.contains(SERVICE)) return;
        toResolve.add(info);
        resolveNext();
    }

    /**
     * До Android 14 NsdManager з'ясовує адресу лише однієї служби за раз —
     * друга спроба водночас падає з FAILURE_ALREADY_ACTIVE. Тож по черзі.
     */
    @SuppressWarnings("deprecation")
    private void resolveNext() {
        if (resolving || finished || nsd == null || toResolve.isEmpty()) return;
        NsdServiceInfo info = toResolve.poll();
        resolving = true;
        try {
            nsd.resolveService(info, new NsdManager.ResolveListener() {
                @Override public void onResolveFailed(NsdServiceInfo si, int code) {
                    main.post(() -> {
                        resolving = false;
                        if (code == NsdManager.FAILURE_ALREADY_ACTIVE && retried.add(si.getServiceName())) {
                            main.postDelayed(() -> queueResolve(si), 400);
                        }
                        resolveNext();
                        maybeDone();
                    });
                }
                @Override public void onServiceResolved(NsdServiceInfo si) {
                    main.post(() -> {
                        resolving = false;
                        String address = addressOf(si);
                        Log.i(Net.TAG, "DNS-SD: " + si.getServiceName() + " → " + address);
                        if (address != null) probe(address, extra);
                        resolveNext();
                        maybeDone();
                    });
                }
            });
        } catch (Exception e) {
            resolving = false;
        }
    }

    /** IPv4 і порт служби; IPv6 не беремо — сторінка радіо живе на IPv4. */
    @SuppressWarnings("deprecation")
    private static String addressOf(NsdServiceInfo si) {
        List<InetAddress> all = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= 34) {
            all.addAll(si.getHostAddresses());
        } else if (si.getHost() != null) {
            all.add(si.getHost());
        }
        for (InetAddress a : all) {
            if (a instanceof Inet4Address) {
                int port = si.getPort();
                return a.getHostAddress() + (port == 80 || port <= 0 ? "" : ":" + port);
            }
        }
        return null;
    }
}
