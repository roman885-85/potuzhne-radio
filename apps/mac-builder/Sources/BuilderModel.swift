// Збірка ПОТУЖНОГО РАДІО: що знає й уміє програма.
//
// Збирає не сама — запускає firmware/rebuild.sh проєкту (той самий, що з
// терміналу), тож збірка з програми й вручну однакова. Сюди додано лише
// те, чого в терміналі бракує: номер версії, зрозумілий хід збірки, що
// саме вийшло у файлі — і оновлення радіо по Wi-Fi без браузера.

import Foundation
import AppKit
import Combine

/// Що записано в прошивці: відмітка «POTUZHNE-RADIO-FW|плата|версія|збірка|».
struct FwInfo {
    let version: String
    let build: String
    let size: Int
    let url: URL

    static func read(_ url: URL) -> FwInfo? {
        guard let d = try? Data(contentsOf: url, options: .mappedIfSafe) else { return nil }
        let mark = Data("POTUZHNE-RADIO-FW|".utf8)
        guard let r = d.range(of: mark) else { return nil }
        let tail = d.subdata(in: r.lowerBound..<min(d.count, r.lowerBound + 80))
        let parts = String(decoding: tail, as: UTF8.self).split(separator: "|", omittingEmptySubsequences: false)
        guard parts.count >= 4 else { return nil }
        return FwInfo(version: String(parts[2]), build: String(parts[3]), size: d.count, url: url)
    }
}

func buildDate(_ s: String) -> Date? {
    let f = DateFormatter(); f.dateFormat = "dd.MM.yyyy HH:mm"; f.locale = Locale(identifier: "uk_UA")
    return f.date(from: s)
}

func sizeText(_ b: Int) -> String {
    if b < 1024 { return "\(b) Б" }
    if b < 1_048_576 { return "\(b / 1024) КБ" }
    return String(format: "%.1f МБ", Double(b) / 1_048_576).replacingOccurrences(of: ".", with: ",")
}

@MainActor
final class BuilderModel: NSObject, ObservableObject {
    enum BuildState: Equatable { case idle, running, ok, failed(String), cancelled }

    @Published var project: URL
    @Published var version = "1.0"
    @Published var fw: FwInfo?
    @Published var state: BuildState = .idle
    @Published var phase = ""
    @Published var log: [String] = []
    @Published var started: Date?
    @Published var finishedIn: TimeInterval = 0

    @Published var selected: Radio?
    @Published var uploading = false
    @Published var uploadProgress: Double = 0
    @Published var uploadStatus = ""
    @Published var uploadError = ""

    let finder = RadioFinder()
    private var proc: Process?
    private var watch: AnyCancellable?

    private let kProject = "project"

    override init() {
        let saved = UserDefaults.standard.string(forKey: kProject).map { URL(fileURLWithPath: $0) }
        let def = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Documents/powered radio")
        project = saved ?? def
        super.init()
        if !projectOK { locateNearApp() }
        refresh()
        watch = finder.objectWillChange.sink { [weak self] _ in
            DispatchQueue.main.async {
                guard let self else { return }
                if let s = self.selected, let fresh = self.finder.radios.first(where: { $0.ip == s.ip }) { self.selected = fresh }
                if self.selected == nil { self.selected = self.finder.radios.first }
            }
        }
        finder.start()
    }

    var firmwareDir: URL { project.appendingPathComponent("firmware") }
    var script: URL { firmwareDir.appendingPathComponent("rebuild.sh") }
    var updateBin: URL { firmwareDir.appendingPathComponent("PotuzhneRadio-ES3C28P-update.bin") }
    var projectOK: Bool { FileManager.default.isExecutableFile(atPath: script.path) }

    /// Програма лежить у «Програми» всередині проєкту — тоді проєкт поруч.
    private func locateNearApp() {
        var u = Bundle.main.bundleURL
        for _ in 0..<3 {
            u = u.deletingLastPathComponent()
            if FileManager.default.isExecutableFile(atPath: u.appendingPathComponent("firmware/rebuild.sh").path) { setProject(u); return }
        }
    }

    func setProject(_ u: URL) {
        project = u
        UserDefaults.standard.set(u.path, forKey: kProject)
        refresh()
    }

    func chooseProject() {
        let p = NSOpenPanel()
        p.canChooseDirectories = true; p.canChooseFiles = false; p.prompt = "Вибрати"
        p.message = "Виберіть теку проєкту «powered radio» (у ній є firmware/rebuild.sh)"
        if p.runModal() == .OK, let u = p.url { setProject(u) }
    }

    func refresh() {
        if let v = try? String(contentsOf: firmwareDir.appendingPathComponent("VERSION"), encoding: .utf8) {
            version = v.trimmingCharacters(in: .whitespacesAndNewlines)
        }
        fw = FwInfo.read(updateBin)
    }

    /// 1.0 → 1.1, 1.9 → 1.10, 2 → 2.1
    func bumpVersion() {
        var p = version.split(separator: ".").map(String.init)
        if p.count < 2 { p.append("0") }
        if let last = Int(p[p.count - 1]) { p[p.count - 1] = String(last + 1) }
        version = p.joined(separator: ".")
    }

    var versionValid: Bool { version.range(of: #"^\d{1,3}(\.\d{1,3}){0,2}$"#, options: .regularExpression) != nil }

    // MARK: збірка

    func build() {
        guard state != .running else { return }
        guard projectOK else { state = .failed("Не знайдено firmware/rebuild.sh. Вкажіть теку проєкту."); return }
        guard versionValid else { state = .failed("Номер версії — цифри через крапку, наприклад 1.1"); return }
        if otherBuildRunning() { state = .failed("Зараз уже йде інша збірка (з терміналу чи ще одного вікна). Дві одночасні псують одна одну — дочекайтесь її."); return }
        try? (version + "\n").write(to: firmwareDir.appendingPathComponent("VERSION"), atomically: true, encoding: .utf8)

        log = []; phase = "Готуюсь…"; started = Date(); state = .running
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/bin/bash")
        p.arguments = [script.path]
        p.currentDirectoryURL = firmwareDir
        var env = ProcessInfo.processInfo.environment
        let home = FileManager.default.homeDirectoryForCurrentUser.path
        env["PATH"] = "\(home)/bin:/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
        env["HOME"] = home
        p.environment = env
        let pipe = Pipe()
        p.standardOutput = pipe; p.standardError = pipe
        pipe.fileHandleForReading.readabilityHandler = { [weak self] h in
            let d = h.availableData
            guard !d.isEmpty else { return }
            let text = String(decoding: d, as: UTF8.self)
            DispatchQueue.main.async { self?.append(text) }
        }
        p.terminationHandler = { [weak self] pr in
            DispatchQueue.main.async {
                pipe.fileHandleForReading.readabilityHandler = nil
                self?.finished(pr.terminationStatus, reason: pr.terminationReason)
            }
        }
        do { try p.run(); proc = p } catch { state = .failed("Не вдалося запустити збірку: \(error.localizedDescription)") }
    }

    private var partial = ""
    private func append(_ text: String) {
        let clean = text.replacingOccurrences(of: #"\u{1B}\[[0-9;]*m"#, with: "", options: .regularExpression)
        let all = partial + clean
        var lines = all.components(separatedBy: "\n")
        partial = lines.removeLast()
        for l in lines where !l.trimmingCharacters(in: .whitespaces).isEmpty {
            log.append(l)
            if l.hasPrefix(">>> ") { phase = describe(String(l.dropFirst(4))) }
        }
        if log.count > 600 { log.removeFirst(log.count - 600) }
    }

    private func describe(_ s: String) -> String {
        if s.hasPrefix("версія") { return "Версія \(version): готую збірку" }
        if s.hasPrefix("компіляція") { return "Компілюю прошивку" }
        if s.hasPrefix("образ файлової") { return "Складаю образ файлів (сторінка, списки)" }
        if s.hasPrefix("склейка") { return "Складаю повний образ" }
        if s.hasPrefix("готово") { return "Готово" }
        return s
    }

    private func finished(_ status: Int32, reason: Process.TerminationReason) {
        proc = nil
        finishedIn = Date().timeIntervalSince(started ?? Date())
        if !partial.isEmpty { log.append(partial); partial = "" }
        refresh()
        if state == .cancelled { return }
        if status == 0, fw != nil { state = .ok; phase = "Готово" }
        else {
            let err = log.last(where: { $0.contains("error") || $0.contains("помилк") || $0.contains("Error") }) ?? "див. журнал"
            state = .failed("Збірка не вдалася: \(err)")
        }
    }

    func cancel() {
        guard let p = proc else { return }
        state = .cancelled; phase = "Зупинено"
        p.terminate()
        let k = Process(); k.executableURL = URL(fileURLWithPath: "/usr/bin/pkill")
        k.arguments = ["-f", "arduino-cli.*powered radio"]
        try? k.run()
    }

    private func otherBuildRunning() -> Bool {
        let p = Process(); p.executableURL = URL(fileURLWithPath: "/usr/bin/pgrep"); p.arguments = ["-x", "arduino-cli"]   // точна назва процесу: «-f» ловив би й будь-яку команду зі словом arduino-cli
        p.standardOutput = Pipe()
        try? p.run(); p.waitUntilExit()
        return p.terminationStatus == 0
    }

    func revealFirmware() {
        if FileManager.default.fileExists(atPath: updateBin.path) { NSWorkspace.shared.activateFileViewerSelecting([updateBin]) }
        else { NSWorkspace.shared.open(firmwareDir) }
    }

    // MARK: оновлення радіо по Wi-Fi

    func compareText(radio: Radio) -> String {
        guard let fw else { return "" }
        guard let a = buildDate(fw.build), let b = buildDate(radio.build) else { return "" }
        if a > b { return "у файлі новіша" }
        if a < b { return "у файлі старіша — радіо повернеться до неї" }
        return "та сама збірка"
    }

    func uploadFirmware(to r: Radio) {
        guard let fw else { uploadError = "Файлу прошивки немає — спершу зберіть."; return }
        let a = NSAlert()
        a.messageText = "Оновити прошивку радіо?"
        a.informativeText = "ПОТУЖНЕ РАДІО \(fw.version), зібрано \(fw.build) → \(r.host.isEmpty ? r.ip : r.host).\nЗвук на радіо зупиниться приблизно на хвилину; не вимикайте його, доки йде оновлення."
        a.addButton(withTitle: "Оновити"); a.addButton(withTitle: "Скасувати")
        guard a.runModal() == .alertFirstButtonReturn else { return }
        send(["updatetarget=fw", "update=@\(fw.url.path);filename=PotuzhneRadio-ES3C28P-update.bin"], size: fw.size,
             to: "http://\(r.ip)/update", title: "Надсилаю прошивку") { [weak self] code, text in
            guard let self else { return }
            if !text.hasPrefix("OK") {
                self.uploading = false
                self.uploadError = "Радіо відхилило прошивку: \(text.replacingOccurrences(of: "<[^>]+>", with: " ", options: .regularExpression))"
                return
            }
            self.waitForReboot(r, expectBuild: fw.build)
        }
    }

    func uploadWeb(to r: Radio) {
        let web = firmwareDir.appendingPathComponent("web")
        let files = ["app.js.gz", "app.css.gz"].map { web.appendingPathComponent($0) }
        guard files.allSatisfy({ FileManager.default.fileExists(atPath: $0.path) }) else { uploadError = "У firmware/web немає app.js.gz і app.css.gz — спершу зберіть."; return }
        let a = NSAlert()
        a.messageText = "Оновити сторінку радіо?"
        a.informativeText = "Файли app.js.gz і app.css.gz → \(r.host.isEmpty ? r.ip : r.host). На час надсилання звук на радіо зупиниться."
        a.addButton(withTitle: "Оновити"); a.addButton(withTitle: "Скасувати")
        guard a.runModal() == .alertFirstButtonReturn else { return }
        let total = files.reduce(0) { $0 + ((try? FileManager.default.attributesOfItem(atPath: $1.path)[.size] as? Int) ?? 0) }
        send(files.map { "www=@\($0.path)" }, size: total, to: "http://\(r.ip)/webboard", title: "Надсилаю сторінку") { [weak self] _, _ in
            self?.uploadStatus = "Сторінку оновлено. Відкриті вікна з нею перезавантажаться самі."
            self?.uploading = false
        }
    }

    /*  Надсилання — системним curl (-F … multipart), а не URLSession: macOS 15
        не пускає нашу програму в локальну мережу напряму (див. Discovery.swift).
        Хід надсилання curl пише смужкою «###  45.2%» — з неї й беремо відсотки.  */
    private func send(_ fields: [String], size: Int, to url: String, title: String, done: @escaping (Int, String) -> Void) {
        uploadError = ""; uploadStatus = title + "…"; uploadProgress = 0; uploading = true
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/usr/bin/curl")
        var args = ["--progress-bar", "-m", "240", "-w", "\n%{http_code}"]
        for f in fields { args += ["-F", f] }
        args.append(url)
        p.arguments = args
        let out = Pipe(), err = Pipe()
        p.standardOutput = out; p.standardError = err
        err.fileHandleForReading.readabilityHandler = { [weak self] h in
            let t = String(decoding: h.availableData, as: UTF8.self)
            guard let m = t.range(of: #"(\d+(?:\.\d)?)%"#, options: [.regularExpression, .backwards]) else { return }
            let pct = (Double(t[m].dropLast()) ?? 0) / 100
            DispatchQueue.main.async {
                guard let self else { return }
                self.uploadProgress = pct
                self.uploadStatus = "Надіслано \(sizeText(Int(Double(size) * pct))) з \(sizeText(size))"
            }
        }
        p.terminationHandler = { [weak self] pr in
            let d = out.fileHandleForReading.readDataToEndOfFile()
            err.fileHandleForReading.readabilityHandler = nil
            let text = String(decoding: d, as: UTF8.self)
            let parts = text.split(separator: "\n", omittingEmptySubsequences: false)
            let code = Int(parts.last ?? "") ?? 0
            let body = parts.dropLast().joined(separator: "\n")
            DispatchQueue.main.async {
                guard let self else { return }
                if code == 0 { self.uploading = false; self.uploadError = "Зв'язок із радіо перервався (curl: \(pr.terminationStatus))."; return }
                if code >= 400 { self.uploading = false; self.uploadError = "Радіо відповіло помилкою \(code)"; return }
                done(code, body)
            }
        }
        do { try p.run() } catch { uploading = false; uploadError = "Не вдалося запустити curl: \(error.localizedDescription)" }
    }

    /// Після прошивки радіо перезавантажується; чекаємо, поки знову відповість,
    /// і перевіряємо, що в ньому саме та збірка, яку залили.
    private func waitForReboot(_ r: Radio, expectBuild: String) {
        uploadStatus = "Прошивку записано. Радіо перезавантажується…"
        Task {
            try? await Task.sleep(nanoseconds: 4_000_000_000)
            for i in 0..<45 {
                if let n = await RadioProbe.hello(r.ip, timeout: 2) {
                    await MainActor.run {
                        self.uploading = false
                        self.selected = n
                        if n.build == expectBuild { self.uploadStatus = "Готово: на радіо ПОТУЖНЕ РАДІО \(n.version), зібрано \(n.build)." }
                        else { self.uploadError = "Радіо знову на зв'язку, але в ньому збірка \(n.build), а не \(expectBuild)." }
                    }
                    self.finder.start()
                    return
                }
                await MainActor.run { self.uploadStatus = "Радіо перезавантажується… \(i * 2 + 4) с" }
                try? await Task.sleep(nanoseconds: 2_000_000_000)
            }
            await MainActor.run { self.uploading = false; self.uploadError = "Радіо не відповідає вже півтори хвилини. Перевірте його екран." }
        }
    }
}
