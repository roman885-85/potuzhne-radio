// Пошук ПОТУЖНОГО РАДІО в локальній мережі — спільне для клієнта й програми збірки.
//
// Два шляхи одночасно:
//   1. Bonjour: радіо оголошує себе як «_potuzhne._tcp» (прошивка 1.0 і новіші);
//   2. перебір своєї підмережі /24: GET http://A.B.C.N/api/hello.
// Наше радіо відповідає на /api/hello JSON-ом із "potuzhne":1. Старіша прошивка
// /api/hello не знає — тоді дивимось /api/state: поле fw починається з
// «POTUZHNE-RADIO-FW|».

import Foundation
import Darwin
import AppKit

struct Radio: Identifiable, Hashable {
    var id: String { ip }
    let ip: String
    var host: String
    var version: String
    var build: String
    var station: String
    var playing: Bool

    var title: String { "ПОТУЖНЕ РАДІО" }
    var subtitle: String { host.isEmpty ? ip : "\(host) · \(ip)" }
}

enum RadioProbe {
    nonisolated(unsafe) static var lanBlocked = false

    /*  Запити до радіо йдуть через системний /usr/bin/curl, а не URLSession.
        macOS 15 не пускає в локальну мережу програми без Team ID Apple
        (як наші — підписані власним сертифікатом) і навіть не питає дозволу:
        URLSession одразу дає -1009, а в налаштуваннях «Локальна мережа»
        програми немає. Системні програми, як curl, ця заборона не стосується;
        сторінку ж показує WebKit, у нього свій мережевий процес — теж можна.  */
    static func get(_ url: String, timeout: TimeInterval) async -> (code: Int, body: Data)? {
        await withCheckedContinuation { cont in
            let p = Process()
            p.executableURL = URL(fileURLWithPath: "/usr/bin/curl")
            p.arguments = ["-s", "-m", String(format: "%.1f", timeout), "--connect-timeout", String(format: "%.1f", timeout),
                           "-H", "Connection: close", "-w", "\n%{http_code}", url]
            let out = Pipe(); p.standardOutput = out; p.standardError = FileHandle.nullDevice
            p.terminationHandler = { _ in
                let d = out.fileHandleForReading.readDataToEndOfFile()
                guard let nl = d.lastIndex(of: 10), let code = Int(String(decoding: d[(nl + 1)...], as: UTF8.self)), code > 0 else {
                    cont.resume(returning: nil); return
                }
                cont.resume(returning: (code, d[..<nl]))
            }
            do { try p.run() } catch { cont.resume(returning: nil) }
        }
    }

    /// Чи відповідає за цією адресою наше радіо. `timeout` — для перебору підмережі.
    static func hello(_ address: String, timeout: TimeInterval = 1.2) async -> Radio? {
        guard let r = await get("http://\(address)/api/hello", timeout: timeout) else { return nil }
        if r.code == 200, let j = try? JSONSerialization.jsonObject(with: r.body) as? [String: Any],
           (j["potuzhne"] as? Int) == 1 {
            return Radio(ip: (j["ip"] as? String).flatMap { $0.isEmpty || $0 == "0.0.0.0" ? nil : $0 } ?? address,
                         host: j["host"] as? String ?? "",
                         version: j["v"] as? String ?? "",
                         build: j["build"] as? String ?? "",
                         station: j["station"] as? String ?? "",
                         playing: (j["play"] as? Int ?? 0) == 1)
        }
        // стара прошивка: /api/hello ще немає
        if r.code == 404, let r2 = await get("http://\(address)/api/state", timeout: max(timeout, 2)),
           let j = try? JSONSerialization.jsonObject(with: r2.body) as? [String: Any],
           let fw = j["fw"] as? String, fw.hasPrefix("POTUZHNE-RADIO-FW|") {
            return Radio(ip: address, host: "", version: j["v"] as? String ?? "",
                         build: j["build"] as? String ?? "", station: j["name"] as? String ?? "",
                         playing: (j["play"] as? Int ?? 0) == 1)
        }
        return nil
    }

    /// Адреси IPv4 цього комп'ютера в реальних мережах (Wi-Fi, Ethernet) — без
    /// віртуальних адаптерів Parallels і тунелів VPN.
    static func localNetworks() -> [(ip: UInt32, mask: UInt32)] {
        var out: [(UInt32, UInt32)] = []
        var ifap: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&ifap) == 0, let first = ifap else { return [] }
        defer { freeifaddrs(ifap) }
        var p: UnsafeMutablePointer<ifaddrs>? = first
        while let a = p {
            defer { p = a.pointee.ifa_next }
            let flags = Int32(a.pointee.ifa_flags)
            guard flags & IFF_UP != 0, flags & IFF_LOOPBACK == 0,
                  let sa = a.pointee.ifa_addr, sa.pointee.sa_family == UInt8(AF_INET),
                  let nm = a.pointee.ifa_netmask else { continue }
            let name = String(cString: a.pointee.ifa_name)
            guard name.hasPrefix("en") else { continue }
            let ip = sa.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { UInt32(bigEndian: $0.pointee.sin_addr.s_addr) }
            let mask = nm.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { UInt32(bigEndian: $0.pointee.sin_addr.s_addr) }
            if ip & 0xFFFF0000 == 0xA9FE0000 { continue }          // 169.254 — мережі немає
            out.append((ip, mask))
        }
        return out
    }

    static func dotted(_ v: UInt32) -> String { "\(v >> 24).\((v >> 16) & 255).\((v >> 8) & 255).\(v & 255)" }
}

/// Короткий журнал пошуку: ~/Library/Logs/ПОТУЖНЕ РАДІО/пошук.log — щоб було
/// видно, чому радіо не знайшлось (дозвіл «Локальна мережа», VPN, інша мережа).
enum DiscoveryLog {
    static let url: URL = {
        let d = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Logs/ПОТУЖНЕ РАДІО")
        try? FileManager.default.createDirectory(at: d, withIntermediateDirectories: true)
        return d.appendingPathComponent("пошук.log")
    }()
    static let q = DispatchQueue(label: "discovery.log")
    static func write(_ s: String) {
        let line = "\(Date()) [\(ProcessInfo.processInfo.processName)] \(s)\n"
        q.async {
            if let h = try? FileHandle(forWritingTo: url) { h.seekToEndOfFile(); h.write(Data(line.utf8)); try? h.close() }
            else { try? Data(line.utf8).write(to: url) }
        }
    }
}

/// Пошук: Bonjour і перебір підмережі разом. Знайдені радіо — у `radios`.
@MainActor
final class RadioFinder: NSObject, ObservableObject, NetServiceBrowserDelegate, NetServiceDelegate {
    @Published private(set) var radios: [Radio] = []
    @Published private(set) var searching = false
    /// macOS не пускає програму в локальну мережу — треба дозвіл у налаштуваннях
    @Published private(set) var blocked = false

    private var browser: NetServiceBrowser?
    private var services: [NetService] = []
    private var scanTask: Task<Void, Never>?
    private var retried = false

    func start() {
        stop()
        radios = []
        searching = true
        blocked = false
        RadioProbe.lanBlocked = false
        let b = NetServiceBrowser()
        b.delegate = self
        b.searchForServices(ofType: "_potuzhne._tcp.", inDomain: "local.")
        browser = b
        scanTask = Task { [weak self] in
            await self?.scanSubnets()
            // Bonjour відповідає за секунду-дві; даємо йому ще трохи після перебору
            try? await Task.sleep(nanoseconds: 1_500_000_000)
            // Перший запуск: macOS питає дозвіл на локальну мережу, і поки
            // питання висить, запити не проходять. Не знайшли — ще раз, один.
            if let self, await self.radios.isEmpty, await !self.retried, !Task.isCancelled {
                await MainActor.run { self.retried = true }
                try? await Task.sleep(nanoseconds: 3_000_000_000)
                await self.scanSubnets()
            }
            await MainActor.run {
                self?.blocked = RadioProbe.lanBlocked && (self?.radios.isEmpty ?? true)
                self?.searching = false
            }
        }
    }

    /// Відкрити «Конфіденційність і безпека → Локальна мережа».
    static func openLocalNetworkSettings() {
        // macOS 13+ відкриває розділ за новою адресою; стара веде лише на загальну сторінку безпеки
        let urls = ["x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_LocalNetwork",
                    "x-apple.systempreferences:com.apple.preference.security?Privacy_LocalNetwork"]
        for s in urls { if let u = URL(string: s), NSWorkspace.shared.open(u) { return } }
    }

    func stop() {
        browser?.stop(); browser = nil
        services.forEach { $0.stop() }; services = []
        scanTask?.cancel(); scanTask = nil
        searching = false
    }

    /// Додати адресу вручну (або збережену) — перевіряємо й кладемо в список.
    func check(_ address: String) async -> Radio? {
        guard let r = await RadioProbe.hello(address, timeout: 3) else { return nil }
        add(r)
        return r
    }

    private func add(_ r: Radio) {
        if let i = radios.firstIndex(where: { $0.ip == r.ip }) { radios[i] = r } else { radios.append(r) }
        radios.sort { $0.ip.compare($1.ip, options: .numeric) == .orderedAscending }
    }

    private func scanSubnets() async {
        DiscoveryLog.write("мережі: " + RadioProbe.localNetworks().map { RadioProbe.dotted($0.ip) + "/" + String($0.mask.nonzeroBitCount) }.joined(separator: ", "))
        var hosts: [String] = []
        for (ip, mask) in RadioProbe.localNetworks() {
            // ширшу за /24 мережу не перебираємо цілком — лише свою сотню адрес
            let m: UInt32 = mask.nonzeroBitCount >= 24 ? mask : 0xFFFFFF00
            let net = ip & m, bcast = net | ~m
            var a = net + 1
            while a < bcast { if a != ip { hosts.append(RadioProbe.dotted(a)) }; a += 1 }
        }
        await withTaskGroup(of: Radio?.self) { g in
            var it = hosts.makeIterator()
            var running = 0
            func next() -> Bool {
                guard let h = it.next() else { return false }
                g.addTask { await RadioProbe.hello(h, timeout: 0.9) }
                return true
            }
            while running < 48, next() { running += 1 }
            while let r = await g.next() {
                if Task.isCancelled { g.cancelAll(); return }
                if let r { await MainActor.run { self.add(r) } }
                _ = next()
            }
        }
    }

    // MARK: Bonjour
    nonisolated func netServiceBrowser(_ browser: NetServiceBrowser, didNotSearch errorDict: [String: NSNumber]) {
        DiscoveryLog.write("Bonjour не шукає: \(errorDict)")
    }

    nonisolated func netServiceBrowser(_ browser: NetServiceBrowser, didFind service: NetService, moreComing: Bool) {
        DiscoveryLog.write("Bonjour знайшов: \(service.name)")
        Task { @MainActor in
            self.services.append(service)
            service.delegate = self
            service.resolve(withTimeout: 4)
        }
    }

    nonisolated func netServiceDidResolveAddress(_ sender: NetService) {
        let ips: [String] = (sender.addresses ?? []).compactMap { d in
            d.withUnsafeBytes { raw -> String? in
                guard let sa = raw.baseAddress?.assumingMemoryBound(to: sockaddr.self), sa.pointee.sa_family == UInt8(AF_INET) else { return nil }
                return raw.baseAddress!.assumingMemoryBound(to: sockaddr_in.self).withMemoryRebound(to: sockaddr_in.self, capacity: 1) {
                    RadioProbe.dotted(UInt32(bigEndian: $0.pointee.sin_addr.s_addr))
                }
            }
        }
        Task {
            for ip in ips { if let r = await RadioProbe.hello(ip, timeout: 3) { await MainActor.run { self.add(r) }; break } }
        }
    }
}
