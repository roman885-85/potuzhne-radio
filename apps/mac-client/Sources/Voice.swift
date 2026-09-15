// Голосові команди. Мову розпізнає сам Mac (Speech, українська), текст іде в
// сторінку радіо — а що з ним робити, вирішує вона, однаково з телефоном.
// Сторінка по http до мікрофона доступу не має, тому слухає застосунок.

import AVFoundation
import Speech
import WebKit

/// Що потрібно голосовим командам на цьому Mac і чого бракує.
/// Дозволи macOS питає лише раз: відмовили (чи запит загубився) — далі мовчки
/// «ні», і людина не розуміє, чому програма не чує. Тому стан видно й у вікні
/// перевірки, і на сторінці радіо, з кнопками, що ведуть просто в налаштування.
struct VoiceCheck: Equatable {
    enum Access: String { case granted, denied, restricted, ask }
    var mic: Access
    var speech: Access
    var device: String          // назва мікрофона; "" — мікрофона немає
    var language: Bool          // Mac уміє розпізнавати українську
    var available: Bool         // розпізнавання зараз працює (потрібен інтернет)

    var ready: Bool { mic == .granted && speech == .granted && !device.isEmpty && language && available }

    static func now() -> VoiceCheck {
        let mic: Access
        switch AVCaptureDevice.authorizationStatus(for: .audio) {
        case .authorized: mic = .granted
        case .denied: mic = .denied
        case .restricted: mic = .restricted
        default: mic = .ask
        }
        let speech: Access
        switch SFSpeechRecognizer.authorizationStatus() {
        case .authorized: speech = .granted
        case .denied: speech = .denied
        case .restricted: speech = .restricted
        default: speech = .ask
        }
        let rec = SFSpeechRecognizer(locale: Locale(identifier: "uk-UA"))
        return VoiceCheck(mic: mic, speech: speech,
                          device: AVCaptureDevice.default(for: .audio)?.localizedName ?? "",
                          language: rec != nil, available: rec?.isAvailable ?? false)
    }

    var json: [String: Any] {
        ["mic": mic.rawValue, "speech": speech.rawValue, "device": device, "language": language,
         "available": available, "ready": ready]
    }

    /// Спитати те, чого ще не питали (macOS покаже свої вікна). Відмовлене раніше
    /// macOS більше не питає — тоді лише налаштування або «Спитати знову».
    static func request(_ done: @escaping (VoiceCheck) -> Void) {
        SFSpeechRecognizer.requestAuthorization { _ in
            AVCaptureDevice.requestAccess(for: .audio) { _ in
                DispatchQueue.main.async { done(.now()) }
            }
        }
    }

    /// Забути рішення про цю програму (свою, не чужу — sudo не треба) і спитати знову.
    /// Рятує, коли запит колись загубився: програми немає в списку налаштувань,
    /// а додати її туди вручну macOS не дає.
    static func askAgain(_ done: @escaping (VoiceCheck) -> Void) {
        let id = Bundle.main.bundleIdentifier ?? "ua.potuzhne.macos.radio"
        DispatchQueue.global().async {
            for service in ["Microphone", "SpeechRecognition"] {
                let p = Process()
                p.executableURL = URL(fileURLWithPath: "/usr/bin/tccutil")
                p.arguments = ["reset", service, id]
                p.standardOutput = FileHandle.nullDevice; p.standardError = FileHandle.nullDevice
                try? p.run(); p.waitUntilExit()
            }
            DispatchQueue.main.async { request(done) }
        }
    }

    enum Pane: String { case mic, speech, sound, dictation }

    static func open(_ pane: Pane) {
        // macOS 13+ відкриває розділ за новою адресою; стара — запасна
        let urls: [String]
        switch pane {
        case .mic: urls = ["x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_Microphone",
                           "x-apple.systempreferences:com.apple.preference.security?Privacy_Microphone"]
        case .speech: urls = ["x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_SpeechRecognition",
                              "x-apple.systempreferences:com.apple.preference.security?Privacy_SpeechRecognition"]
        case .sound: urls = ["x-apple.systempreferences:com.apple.Sound-Settings.extension?input",
                             "x-apple.systempreferences:com.apple.preference.sound?input"]
        case .dictation: urls = ["x-apple.systempreferences:com.apple.Keyboard-Settings.extension?Dictation",
                                 "x-apple.systempreferences:com.apple.preference.keyboard?Dictation"]
        }
        for s in urls { if let u = URL(string: s), NSWorkspace.shared.open(u) { return } }
    }
}

extension VoiceCheck.Access {
    var title: String {
        switch self {
        case .granted: return "дозволено"
        case .denied: return "заборонено"
        case .restricted: return "заборонено політикою Mac"
        case .ask: return "ще не питали"
        }
    }
}

/// Рівень звуку з мікрофона — щоб у вікні перевірки було видно, що програма чує.
@MainActor
final class MicMeter: ObservableObject {
    @Published var level: Float = 0
    @Published var running = false
    private var engine: AVAudioEngine?

    func start() {
        guard !running, AVCaptureDevice.authorizationStatus(for: .audio) == .authorized else { return }
        let e = AVAudioEngine()
        let input = e.inputNode
        let format = input.outputFormat(forBus: 0)
        guard format.channelCount > 0, format.sampleRate > 0 else { return }
        input.installTap(onBus: 0, bufferSize: 1024, format: format) { [weak self] buf, _ in
            let v = MicMeter.level(buf)
            Task { @MainActor in guard let self else { return }; self.level = max(v, self.level * 0.8) }
        }
        do { try e.start(); engine = e; running = true } catch { input.removeTap(onBus: 0) }
    }

    func stop() {
        guard let e = engine else { return }
        e.stop(); e.inputNode.removeTap(onBus: 0)
        engine = nil; running = false; level = 0
    }

    /// Шкала в децибелах: −60 дБ — нуль, 0 дБ — повна.
    nonisolated static func level(_ buf: AVAudioPCMBuffer) -> Float {
        guard let ch = buf.floatChannelData?[0] else { return 0 }
        let n = Int(buf.frameLength)
        var sum: Float = 0
        for i in 0..<n { sum += ch[i] * ch[i] }
        let rms = n > 0 ? (sum / Float(n)).squareRoot() : 0
        return max(0, min(1, (20 * log10(max(rms, 1e-6)) + 60) / 60))
    }
}

/// Один канал для розпізнавання. Вбудований мікрофон Mac віддає кілька каналів
/// (на Intel-MacBook — 4), а SFSpeechAudioBufferRecognitionRequest такі буфери
/// мовчки ігнорує: ні тексту, ні помилки — «нічого не почуто».
enum MonoBuffer {
    static func from(_ buf: AVAudioPCMBuffer) -> AVAudioPCMBuffer? {
        let f = buf.format
        if f.channelCount == 1 && f.commonFormat == .pcmFormatFloat32 { return buf }
        guard let src = buf.floatChannelData,
              let mono = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: f.sampleRate, channels: 1, interleaved: false),
              let out = AVAudioPCMBuffer(pcmFormat: mono, frameCapacity: buf.frameLength),
              let dst = out.floatChannelData?[0] else { return nil }
        // перший канал: канали мікрофонної решітки зсунуті за фазою, середнє між ними глушить голос
        dst.update(from: src[0], count: Int(buf.frameLength))
        out.frameLength = buf.frameLength
        return out
    }
}

@MainActor
final class VoiceListener: NSObject {
    static let shared = VoiceListener()

    /// Показати вікно перевірки дозволів (ставить AppModel).
    var showCheck: (() -> Void)?

    private let engine = AVAudioEngine()
    private var request: SFSpeechAudioBufferRecognitionRequest?
    private var task: SFSpeechRecognitionTask?
    private weak var web: WKWebView?
    private var best = ""
    private var alts: [String] = []
    private var active = false
    private var quiet: DispatchWorkItem?
    private var limit: DispatchWorkItem?

    /// Сторінка попросила послухати.
    func start(_ wv: WKWebView) {
        web = wv
        if active { finish(send: false) }
        guard let rec = SFSpeechRecognizer(locale: Locale(identifier: "uk-UA")) else {
            fail("Цей Mac не розпізнає українську мову", check: true); return
        }
        VoiceCheck.request { c in
            self.sendCheck(c)
            guard c.speech == .granted else {
                self.fail("Програмі не дозволено розпізнавання мовлення — відкриваю перевірку дозволів", check: true); return
            }
            guard c.mic == .granted else {
                self.fail("Програмі не дозволено мікрофон — відкриваю перевірку дозволів", check: true); return
            }
            guard !c.device.isEmpty else { self.fail("Мікрофона не знайдено — під'єднайте його чи виберіть у налаштуваннях звуку", check: true); return }
            guard rec.isAvailable else { self.fail("Розпізнавання мови зараз недоступне — перевірте інтернет"); return }
            self.run(rec)
        }
    }

    private func run(_ rec: SFSpeechRecognizer) {
        let req = SFSpeechAudioBufferRecognitionRequest()
        req.shouldReportPartialResults = true
        let input = engine.inputNode
        let format = input.outputFormat(forBus: 0)
        guard format.channelCount > 0, format.sampleRate > 0 else { fail("Мікрофона не знайдено", check: true); return }
        input.removeTap(onBus: 0)
        input.installTap(onBus: 0, bufferSize: 1024, format: format) { buffer, _ in if let m = MonoBuffer.from(buffer) { req.append(m) } }
        engine.prepare()
        do { try engine.start() } catch { input.removeTap(onBus: 0); fail("Мікрофон не вмикається: \(error.localizedDescription)", check: true); return }
        request = req
        best = ""; alts = []; active = true
        task = rec.recognitionTask(with: req) { [weak self] result, error in
            DispatchQueue.main.async { self?.handle(result, error) }
        }
        // більше восьми секунд не слухаємо — людина вже не говорить
        let l = DispatchWorkItem { [weak self] in self?.finish(send: true) }
        limit = l
        DispatchQueue.main.asyncAfter(deadline: .now() + 8, execute: l)
    }

    private func handle(_ result: SFSpeechRecognitionResult?, _ error: Error?) {
        guard active else { return }
        if let r = result {
            best = r.bestTranscription.formattedString
            alts = r.transcriptions.map { $0.formattedString }
            js("window.potuzhneVoicePartial && potuzhneVoicePartial(\(Self.json(best)))")
            if r.isFinal { finish(send: true); return }
            // пауза в мовленні — фраза скінчилась
            quiet?.cancel()
            let q = DispatchWorkItem { [weak self] in self?.finish(send: true) }
            quiet = q
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.3, execute: q)
        } else if let error {
            if !best.isEmpty { finish(send: true); return }
            finish(send: false)
            let (text, check) = Self.explain(error)
            fail(text, check: check)
        }
    }

    /// Помилка розпізнавання людською мовою. Друге значення — чи відкрити перевірку.
    nonisolated static func explain(_ error: Error) -> (String, Bool) {
        let e = error as NSError
        let msg = e.localizedDescription.lowercased()
        if msg.contains("siri") || msg.contains("dictation") || (e.domain == "kLSRErrorDomain" && e.code == 201) {
            return ("На цьому Mac вимкнено Siri й диктування — без них macOS не розпізнає мову. Увімкніть диктування: Системні параметри → Клавіатура → Диктування", true)
        }
        if e.domain == NSURLErrorDomain || msg.contains("network") || msg.contains("internet") || msg.contains("connection") {
            return ("Розпізнаванню мови потрібен інтернет — перевірте з'єднання", false)
        }
        if e.code == 1110 || msg.contains("no speech") { return ("Нічого не почуто", false) }
        return ("Нічого не почуто (\(e.domain) \(e.code))", false)
    }

    private func finish(send: Bool) {
        guard active else { return }
        active = false
        quiet?.cancel(); limit?.cancel()
        if engine.isRunning { engine.stop() }
        engine.inputNode.removeTap(onBus: 0)
        request?.endAudio()
        task?.cancel()
        request = nil; task = nil
        if send {
            var list = alts.isEmpty ? [best] : alts
            if let i = list.firstIndex(of: best), i > 0 { list.remove(at: i); list.insert(best, at: 0) }
            js("window.potuzhneVoiceResult && potuzhneVoiceResult(\(Self.json(list)))")
        }
    }

    private func fail(_ message: String, check: Bool = false) {
        js("window.potuzhneVoiceError && potuzhneVoiceError(\(Self.json(message)))")
        if check { showCheck?() }
    }

    /// Стан дозволів — у сторінку (картка «Дозволи на цьому Mac» у «Голосових командах»).
    func sendCheck(_ c: VoiceCheck = .now(), to wv: WKWebView? = nil) {
        if let wv { web = wv }
        js("window.potuzhneVoicePerm && potuzhneVoicePerm(\(Self.json(c.json)))")
    }

    private func js(_ code: String) { web?.evaluateJavaScript(code, completionHandler: nil) }

    nonisolated static func json(_ v: Any) -> String {
        guard let d = try? JSONSerialization.data(withJSONObject: v is String ? [v] : v), let s = String(data: d, encoding: .utf8) else { return "null" }
        return v is String ? String(s.dropFirst().dropLast()) : s
    }
}

/// Міст «сторінка → застосунок»: window.webkit.messageHandlers.voice.postMessage(...).
///   'listen'                          — послухати команду;
///   'perm'                            — надіслати стан дозволів (potuzhneVoicePerm);
///   'ask' / 'askAgain'                — спитати дозволи / забути відмову й спитати знову;
///   'open:mic|speech|sound|dictation' — відкрити розділ налаштувань;
///   'window'                          — вікно перевірки в самій програмі.
/// Окремий об'єкт — WKUserContentController тримає обробник сильно, і
/// координатор вебвікна через нього не жив би вічно.
final class VoiceBridge: NSObject, WKScriptMessageHandler {
    weak var web: WKWebView?
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard let cmd = message.body as? String, let wv = web ?? message.webView else { return }
        Task { @MainActor in
            let v = VoiceListener.shared
            switch cmd {
            case "listen": v.start(wv)
            case "perm": v.sendCheck(to: wv)
            case "ask": VoiceCheck.request { v.sendCheck($0, to: wv) }
            case "askAgain": VoiceCheck.askAgain { v.sendCheck($0, to: wv) }
            case "window": v.showCheck?()
            default:
                if cmd.hasPrefix("open:"), let p = VoiceCheck.Pane(rawValue: String(cmd.dropFirst(5))) { VoiceCheck.open(p) }
            }
        }
    }
}
