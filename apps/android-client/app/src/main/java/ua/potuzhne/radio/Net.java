package ua.potuzhne.radio;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.DhcpInfo;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.util.Log;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Де телефон у мережі й як туди достукатися.
 */
final class Net {

    private Net() {}

    static final String TAG = "Potuzhne";

    /**
     * Мережа Wi-Fi телефона (або дротова на планшеті з адаптером), якщо є.
     */
    static Network localNetwork(Context ctx) {
        ConnectivityManager cm = connectivity(ctx);
        if (cm == null) return null;
        Network ethernet = null;
        try {
            for (Network n : cm.getAllNetworks()) {
                NetworkCapabilities caps = cm.getNetworkCapabilities(n);
                if (caps == null) continue;
                if (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return n;
                if (ethernet == null && caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) ethernet = n;
            }
        } catch (Exception e) {
            Log.w(TAG, "перелік мереж недоступний", e);
        }
        return ethernet;
    }

    /**
     * Пустити всі з'єднання застосунку через Wi-Fi.
     *
     * Коли радіо працює точкою доступу (перше налаштування — телефон
     * під'єднано просто до радіо), в такому Wi-Fi немає інтернету, і Android
     * тихо веде всі з'єднання мобільним інтернетом. До 192.168.4.1 звідти
     * не дістатися — ні пошуку, ні сторінки. Прив'язка до Wi-Fi це знімає;
     * коли ж інтернет у Wi-Fi є, вона нічого не змінює.
     * WebView ходить у мережу з того самого процесу, тож прив'язка діє й на
     * сторінку радіо.
     */
    static void bindTo(Context ctx, Network network) {
        ConnectivityManager cm = connectivity(ctx);
        if (cm == null) return;
        try {
            if (Build.VERSION.SDK_INT >= 23) cm.bindProcessToNetwork(network);
            else bindLegacy(network);
        } catch (Exception e) {
            Log.w(TAG, "не вдалося прив'язатися до Wi-Fi", e);
        }
    }

    @SuppressWarnings("deprecation")
    private static void bindLegacy(Network network) {
        ConnectivityManager.setProcessDefaultNetwork(network);
    }

    /**
     * Власні адреси IPv4 у локальній мережі.
     *
     * Три джерела по черзі, бо кожне на якомусь Android підводить: властивості
     * самої Wi-Fi-мережі (найточніше), перелік мережевих інтерфейсів, і
     * наостанок DHCP із WifiManager.
     */
    static List<Inet4Address> ownAddresses(Context ctx, Network network) {
        Set<Inet4Address> out = new LinkedHashSet<>();

        ConnectivityManager cm = connectivity(ctx);
        if (cm != null && network != null) {
            try {
                LinkProperties lp = cm.getLinkProperties(network);
                if (lp != null) {
                    for (LinkAddress la : lp.getLinkAddresses()) {
                        InetAddress a = la.getAddress();
                        if (usable(a)) out.add((Inet4Address) a);
                    }
                }
            } catch (Exception e) {
                Log.w(TAG, "LinkProperties недоступні", e);
            }
        }

        if (out.isEmpty()) {
            try {
                for (NetworkInterface ni : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                    if (!ni.isUp() || ni.isLoopback() || mobileOrTunnel(ni.getName())) continue;
                    for (InterfaceAddress ia : ni.getInterfaceAddresses()) {
                        InetAddress a = ia.getAddress();
                        if (usable(a) && a.isSiteLocalAddress()) out.add((Inet4Address) a);
                    }
                }
            } catch (Exception e) {
                Log.w(TAG, "перелік інтерфейсів недоступний", e);
            }
        }

        if (out.isEmpty()) {
            Inet4Address dhcp = fromDhcp(ctx);
            if (dhcp != null) out.add(dhcp);
        }
        return new ArrayList<>(out);
    }

    /** «192.168.1.» для 192.168.1.57 — перебираємо саме цю сотню адрес (/24). */
    static String base24(Inet4Address a) {
        byte[] b = a.getAddress();
        return (b[0] & 255) + "." + (b[1] & 255) + "." + (b[2] & 255) + ".";
    }

    static int last(Inet4Address a) {
        return a.getAddress()[3] & 255;
    }

    // ------------------------------------------------------------------------

    private static boolean usable(InetAddress a) {
        if (!(a instanceof Inet4Address)) return false;
        if (a.isLoopbackAddress() || a.isLinkLocalAddress() || a.isAnyLocalAddress()) return false;
        return true;
    }

    /** Мобільний інтернет, VPN і службові інтерфейси — у них радіо не буває. */
    private static boolean mobileOrTunnel(String name) {
        if (name == null) return true;
        String[] skip = {"rmnet", "ccmni", "pdp", "tun", "ppp", "dummy", "ip6tnl", "sit", "v4-", "clat", "p2p"};
        for (String s : skip) if (name.startsWith(s)) return true;
        return false;
    }

    @SuppressWarnings("deprecation")
    private static Inet4Address fromDhcp(Context ctx) {
        try {
            WifiManager wm = (WifiManager) ctx.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            if (wm == null) return null;
            DhcpInfo info = wm.getDhcpInfo();
            if (info == null || info.ipAddress == 0) return null;
            int ip = info.ipAddress;          // байти у зворотному порядку
            byte[] b = {(byte) ip, (byte) (ip >> 8), (byte) (ip >> 16), (byte) (ip >> 24)};
            InetAddress a = InetAddress.getByAddress(b);
            return usable(a) ? (Inet4Address) a : null;
        } catch (Exception e) {
            return null;
        }
    }

    private static ConnectivityManager connectivity(Context ctx) {
        return (ConnectivityManager) ctx.getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
    }
}
