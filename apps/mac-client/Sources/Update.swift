//  Оновлення самої програми з GitHub.
//
//  Раніше програми доводилось завантажувати вручну: радіо оновлювалось саме, а
//  Mac, Windows і Android — ні. Тепер програма дивиться у випуск на GitHub і,
//  якщо там свіжіша за неї, пропонує оновитись і робить це сама.
//
//  Порівнюємо не з номером випуску, а з файлом PotuzhneRadio-clients.json —
//  там записано, якої версії програма реально лежить у цьому випуску. Якщо
//  програму не перезбирали, вона переноситься з минулого разу зі своїм
//  номером, і оновлення дарма не пропонується.

import Foundation
import AppKit

enum Updater {
    static let repo = "roman885-85/potuzhne-radio"

    static var myVersion: String {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0"
    }

    struct Found {
        let version: String
        let zip: URL
        let notes: String
    }

    /// «1.4.38» новіша за «1.4.9» — порівнюємо числами, не рядками.
    static func isNewer(_ a: String, than b: String) -> Bool {
        let pa = a.split(separator: ".").map { Int($0) ?? 0 }
        let pb = b.split(separator: ".").map { Int($0) ?? 0 }
        for i in 0..<max(pa.count, pb.count) {
            let x = i < pa.count ? pa[i] : 0
            let y = i < pb.count ? pb[i] : 0
            if x != y { return x > y }
        }
        return false
    }

    private static func get(_ url: URL, json: Bool) async throws -> Data {
        var r = URLRequest(url: url, timeoutInterval: 20)
        r.setValue("potuzhne-radio-mac", forHTTPHeaderField: "User-Agent")
        if json { r.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept") }
        let (d, _) = try await URLSession.shared.data(for: r)
        return d
    }

    /// Що лежить у свіжому випуску. nil — якщо нового немає або не достукались.
    static func check() async -> Found? {
        guard let api = URL(string: "https://api.github.com/repos/\(repo)/releases/latest") else { return nil }
        guard let data = try? await get(api, json: true),
              let top = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let assets = top["assets"] as? [[String: Any]] else { return nil }

        func url(_ name: String) -> URL? {
            for a in assets where (a["name"] as? String) == name {
                if let s = a["browser_download_url"] as? String { return URL(string: s) }
            }
            return nil
        }
        guard let zip = url("PotuzhneRadio-Mac.zip") else { return nil }

        // версія програми у випуску
        var there = (top["tag_name"] as? String) ?? ""
        if there.hasPrefix("v") { there.removeFirst() }
        if let man = url("PotuzhneRadio-clients.json"),
           let d = try? await get(man, json: false),
           let m = try? JSONSerialization.jsonObject(with: d) as? [String: Any],
           let v = m["mac"] as? String, !v.isEmpty {
            there = v
        }
        guard !there.isEmpty, isNewer(there, than: myVersion) else { return nil }
        let notes = (top["body"] as? String) ?? ""
        return Found(version: there, zip: zip, notes: notes)
    }

    /// Завантажити, підмінити себе й перезапуститись.
    static func install(_ f: Found) async throws {
        let fm = FileManager.default
        let tmp = fm.temporaryDirectory.appendingPathComponent("potuzhne-update-\(UUID().uuidString)")
        try fm.createDirectory(at: tmp, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: tmp) }

        let zipData = try await get(f.zip, json: false)
        let zipFile = tmp.appendingPathComponent("app.zip")
        try zipData.write(to: zipFile)

        let out = tmp.appendingPathComponent("out")
        try fm.createDirectory(at: out, withIntermediateDirectories: true)
        let un = Process()
        un.executableURL = URL(fileURLWithPath: "/usr/bin/ditto")
        un.arguments = ["-x", "-k", zipFile.path, out.path]
        try un.run(); un.waitUntilExit()
        guard un.terminationStatus == 0,
              let got = try? fm.contentsOfDirectory(at: out, includingPropertiesForKeys: nil)
                  .first(where: { $0.pathExtension == "app" })
        else { throw NSError(domain: "update", code: 1,
                             userInfo: [NSLocalizedDescriptionKey: "Не вдалося розпакувати завантажене"]) }

        //  Себе на ходу не підміниш: лишаємо коротку вказівку, яка дочекається
        //  виходу програми, підмінить теку й запустить нову.
        let here = Bundle.main.bundleURL.path
        let keep = tmp.deletingLastPathComponent().appendingPathComponent("potuzhne-new.app")
        try? fm.removeItem(at: keep)
        try fm.moveItem(at: got, to: keep)

        let pid = ProcessInfo.processInfo.processIdentifier
        let sh = """
        while kill -0 \(pid) 2>/dev/null; do sleep 0.2; done
        rm -rf '\(here)'
        /usr/bin/ditto '\(keep.path)' '\(here)'
        rm -rf '\(keep.path)'
        sleep 0.3
        /usr/bin/open '\(here)'
        """
        let swap = Process()
        swap.executableURL = URL(fileURLWithPath: "/bin/sh")
        swap.arguments = ["-c", sh]
        try swap.run()
        await MainActor.run { NSApp.terminate(nil) }
    }

    /// Питаємо людину й оновлюємось. `silent` — мовчати, коли нового немає.
    @MainActor
    static func run(silent: Bool) {
        Task {
            guard let f = await check() else {
                if !silent {
                    let a = NSAlert()
                    a.messageText = "Оновлення не потрібне"
                    a.informativeText = "У вас найсвіжіша версія — \(myVersion)."
                    a.addButton(withTitle: "Гаразд")
                    a.runModal()
                }
                return
            }
            let a = NSAlert()
            a.messageText = "Є нова версія — \(f.version)"
            let first = f.notes.split(separator: "\n").first.map(String.init) ?? ""
            a.informativeText = (first.isEmpty ? "" : first + "\n\n")
                + "Зараз у вас \(myVersion). Оновити зараз? Програма закриється й відкриється знову."
            a.addButton(withTitle: "Оновити")
            a.addButton(withTitle: "Пізніше")
            guard a.runModal() == .alertFirstButtonReturn else { return }

            do { try await install(f) }
            catch {
                let e = NSAlert()
                e.messageText = "Не вдалося оновити"
                e.informativeText = error.localizedDescription
                e.addButton(withTitle: "Гаразд")
                e.runModal()
            }
        }
    }
}
