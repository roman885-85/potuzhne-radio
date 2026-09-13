// Голосові команди. Мову розпізнає сам Mac (Speech, українська), текст іде в
// сторінку радіо — а що з ним робити, вирішує вона, однаково з телефоном.
// Сторінка по http до мікрофона доступу не має, тому слухає застосунок.

import AVFoundation
import Speech
import WebKit

@MainActor
final class VoiceListener: NSObject {
    static let shared = VoiceListener()

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
            fail("Цей Mac не розпізнає українську мову"); return
        }
        SFSpeechRecognizer.requestAuthorization { status in
            DispatchQueue.main.async {
                guard status == .authorized else {
                    self.fail("Дозвольте розпізнавання мовлення: Системні параметри → Приватність і безпека → Розпізнавання мовлення")
                    return
                }
                AVCaptureDevice.requestAccess(for: .audio) { ok in
                    DispatchQueue.main.async {
                        guard ok else {
                            self.fail("Дозвольте мікрофон: Системні параметри → Приватність і безпека → Мікрофон")
                            return
                        }
                        guard rec.isAvailable else { self.fail("Розпізнавання мови зараз недоступне — перевірте інтернет"); return }
                        self.run(rec)
                    }
                }
            }
        }
    }

    private func run(_ rec: SFSpeechRecognizer) {
        let req = SFSpeechAudioBufferRecognitionRequest()
        req.shouldReportPartialResults = true
        let input = engine.inputNode
        let format = input.outputFormat(forBus: 0)
        guard format.channelCount > 0, format.sampleRate > 0 else { fail("Мікрофона не знайдено"); return }
        input.removeTap(onBus: 0)
        input.installTap(onBus: 0, bufferSize: 1024, format: format) { buffer, _ in req.append(buffer) }
        engine.prepare()
        do { try engine.start() } catch { input.removeTap(onBus: 0); fail("Мікрофон не вмикається: \(error.localizedDescription)"); return }
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
        } else if error != nil {
            if best.isEmpty { finish(send: false); fail("Нічого не почуто") } else { finish(send: true) }
        }
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

    private func fail(_ message: String) {
        js("window.potuzhneVoiceError && potuzhneVoiceError(\(Self.json(message)))")
    }

    private func js(_ code: String) { web?.evaluateJavaScript(code, completionHandler: nil) }

    private static func json(_ v: Any) -> String {
        guard let d = try? JSONSerialization.data(withJSONObject: v is String ? [v] : v), let s = String(data: d, encoding: .utf8) else { return "null" }
        return v is String ? String(s.dropFirst().dropLast()) : s
    }
}

/// Міст «сторінка → застосунок»: window.webkit.messageHandlers.voice.postMessage('listen').
/// Окремий об'єкт — WKUserContentController тримає обробник сильно, і
/// координатор вебвікна через нього не жив би вічно.
final class VoiceBridge: NSObject, WKScriptMessageHandler {
    weak var web: WKWebView?
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard (message.body as? String) == "listen", let wv = web ?? message.webView else { return }
        Task { @MainActor in VoiceListener.shared.start(wv) }
    }
}
