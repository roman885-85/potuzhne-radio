package ua.potuzhne.radio;

/**
 * Одне знайдене радіо — те, що воно саме про себе сказало.
 *
 * Адреса — та, з якої прийшла відповідь, а не поле «ip» у відповіді: радіо
 * знає свою адресу у своїй мережі, а телефон міг дістатися до нього інакше
 * (через емулятор, через іншу підмережу). Перевірено ж саме те, за якою
 * адресою розмова відбулася.
 */
final class Radio {

    /** «192.168.1.32» або «192.168.1.32:8080» — без http:// і без скісної риски. */
    final String address;
    /** Мережеве ім'я на кшталт potuzhne-c447d4; у старої прошивки порожнє. */
    final String host;
    /** Станція, що грає або стоїть обраною. */
    final String station;
    final String version;
    final String build;
    final boolean playing;
    /**
     * Чи знає прошивка /api/hello. Стара — ні; тоді живість перевіряється
     * через /api/state, щоб не робити щоразу два запити замість одного.
     */
    final boolean hello;

    Radio(String address, String host, String station, String version, String build,
          boolean playing, boolean hello) {
        this.address = address;
        this.host = host == null ? "" : host;
        this.station = station == null ? "" : station;
        this.version = version == null ? "" : version;
        this.build = build == null ? "" : build;
        this.playing = playing;
        this.hello = hello;
    }

    String url() {
        return "http://" + address + "/";
    }

    /** «potuzhne-c447d4 · 192.168.1.32» — або сама адреса, якщо імені немає. */
    String subtitle() {
        return host.isEmpty() ? address : host + " · " + address;
    }

    /** Те саме радіо, але з ім'ям, яке підказав пошук за іменем (стара прошивка свого не каже). */
    Radio withHost(String name) {
        if (!host.isEmpty() || name == null || name.isEmpty()) return this;
        return new Radio(address, name, station, version, build, playing, hello);
    }

    /** Для впорядкування списку: 192.168.1.9 раніше за 192.168.1.10. */
    long sortKey() {
        String ip = address;
        int colon = ip.indexOf(':');
        if (colon >= 0) ip = ip.substring(0, colon);
        String[] p = ip.split("\\.");
        if (p.length != 4) return Long.MAX_VALUE;
        long key = 0;
        try {
            for (String part : p) key = key * 256 + Integer.parseInt(part);
        } catch (NumberFormatException e) {
            return Long.MAX_VALUE;
        }
        return key;
    }
}
