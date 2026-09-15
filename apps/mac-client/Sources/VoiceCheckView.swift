// Вікно «Голосові команди на цьому Mac»: що дозволено, чого бракує, і кнопки,
// що ведуть просто в потрібний розділ «Системних параметрів». Відкривається з
// меню «Радіо», з картки на сторінці радіо і саме — коли голос не спрацював
// через дозвіл.

import SwiftUI
import AVFoundation
import Speech
import WebKit

struct VoiceCheckView: View {
    @EnvironmentObject var model: AppModel
    @StateObject private var meter = MicMeter()
    @State private var check: VoiceCheck
    @State private var busy = false
    private let preset: Bool
    var close: () -> Void

    /// preset — показати заданий стан (знімки для перевірки), а не справжній.
    init(preset: VoiceCheck? = nil, close: @escaping () -> Void = {}) {
        _check = State(initialValue: preset ?? .now())
        self.preset = preset != nil
        self.close = close
    }

    private let tick = Timer.publish(every: 1.5, on: .main, in: .common).autoconnect()

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 12) {
                Image(systemName: "mic.fill").font(.system(size: 20)).foregroundColor(Palette.acc)
                    .frame(width: 40, height: 40).background(RoundedRectangle(cornerRadius: 10).fill(Palette.pan2))
                VStack(alignment: .leading, spacing: 2) {
                    Text("Голосові команди на цьому Mac").font(.system(size: 17, weight: .semibold)).foregroundColor(Palette.txt)
                    Text(check.ready ? "Усе готово — натисніть кнопку з мікрофоном угорі сторінки радіо й скажіть команду."
                                     : "Програмі бракує дозволу чи умови — нижче видно, чого саме, і кнопка, щоб виправити.")
                        .font(.system(size: 12)).foregroundColor(check.ready ? Palette.ok : Palette.dim)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }

            VStack(spacing: 0) {
                CheckRow(icon: "mic", title: "Мікрофон", ok: check.mic == .granted,
                         state: check.mic.title,
                         hint: check.mic == .granted ? nil : "Системні параметри → Приватність і безпека → Мікрофон → увімкнути «ПОТУЖНЕ РАДІО»") {
                    accessButton(check.mic, pane: .mic)
                }
                Divider().overlay(Palette.line)
                CheckRow(icon: "waveform", title: "Розпізнавання мовлення", ok: check.speech == .granted,
                         state: check.speech.title,
                         hint: check.speech == .granted ? nil : "Системні параметри → Приватність і безпека → Розпізнавання мовлення → увімкнути «ПОТУЖНЕ РАДІО»") {
                    accessButton(check.speech, pane: .speech)
                }
                Divider().overlay(Palette.line)
                CheckRow(icon: "speaker.wave.2", title: "Пристрій запису", ok: !check.device.isEmpty,
                         state: check.device.isEmpty ? "не знайдено" : check.device,
                         hint: check.device.isEmpty ? "Під'єднайте мікрофон або виберіть його: Системні параметри → Звук → Вхід" : nil) {
                    Button("Налаштування звуку") { VoiceCheck.open(.sound) }.buttonStyle(PillButton(accent: check.device.isEmpty))
                }
                if check.mic == .granted && !check.device.isEmpty {
                    HStack(spacing: 10) {
                        Text("Чути:").font(.system(size: 12)).foregroundColor(Palette.dim).frame(width: 150, alignment: .leading)
                        GeometryReader { g in
                            ZStack(alignment: .leading) {
                                RoundedRectangle(cornerRadius: 3).fill(Palette.pan2)
                                RoundedRectangle(cornerRadius: 3).fill(meter.level > 0.25 ? Palette.acc : Palette.dim)
                                    .frame(width: max(2, g.size.width * CGFloat(meter.level)))
                            }
                        }
                        .frame(height: 8)
                        Text(meter.level > 0.25 ? "звук є" : "скажіть щось").font(.system(size: 12))
                            .foregroundColor(meter.level > 0.25 ? Palette.acc : Palette.dim).frame(width: 90, alignment: .trailing)
                    }
                    .padding(.horizontal, 14).padding(.bottom, 12)
                }
                Divider().overlay(Palette.line)
                CheckRow(icon: "globe", title: "Українська мова", ok: check.language && check.available,
                         state: !check.language ? "цей Mac її не розпізнає" : check.available ? "доступна (через інтернет)" : "зараз недоступна",
                         hint: check.language && !check.available ? "Потрібен інтернет. Якщо він є — увімкніть диктування: Системні параметри → Клавіатура → Диктування" : nil) {
                    if check.language && !check.available {
                        Button("Диктування") { VoiceCheck.open(.dictation) }.buttonStyle(PillButton(accent: true))
                    }
                }
            }
            .background(RoundedRectangle(cornerRadius: 12).fill(Palette.pan))
            .overlay(RoundedRectangle(cornerRadius: 12).stroke(Palette.line))

            Text("Дозволи macOS питає лише раз. Якщо колись натиснули «Не дозволяти» або програми немає в списку налаштувань — натисніть «Спитати дозвіл знову»: macOS забуде старе рішення й покаже запит.")
                .font(.system(size: 12)).foregroundColor(Palette.dim).fixedSize(horizontal: false, vertical: true)

            HStack(spacing: 10) {
                Button { refresh() } label: { Label("Перевірити знову", systemImage: "arrow.clockwise") }
                    .buttonStyle(PillButton(accent: false))
                Button { busy = true; meter.stop(); VoiceCheck.askAgain { done($0) } } label: { Label("Спитати дозвіл знову", systemImage: "questionmark.circle") }
                    .buttonStyle(PillButton(accent: false)).disabled(busy)
                Spacer()
                if check.ready && model.current != nil {
                    Button { meter.stop(); close(); model.startVoice() } label: { Label("Сказати команду", systemImage: "mic.fill") }
                        .buttonStyle(PillButton(accent: true))
                }
                Button("Закрити") { close() }.buttonStyle(PillButton(accent: false)).keyboardShortcut(.cancelAction)
            }
        }
        .padding(20)
        .frame(width: 600)
        .background(Palette.bg)
        .preferredColorScheme(.dark)
        .onAppear { if !preset { refresh() } }
        .onDisappear { meter.stop() }
        .onReceive(tick) { _ in guard !preset else { return }; let c = VoiceCheck.now(); if c != check { check = c; VoiceListener.shared.sendCheck(c) }; syncMeter() }
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in if !preset { refresh() } }
    }

    @ViewBuilder private func accessButton(_ a: VoiceCheck.Access, pane: VoiceCheck.Pane) -> some View {
        switch a {
        case .granted: EmptyView()
        case .ask: Button("Дозволити") { busy = true; VoiceCheck.request { done($0) } }.buttonStyle(PillButton(accent: true)).disabled(busy)
        case .denied, .restricted: Button("Відкрити налаштування") { VoiceCheck.open(pane) }.buttonStyle(PillButton(accent: true))
        }
    }

    private func refresh() { check = VoiceCheck.now(); VoiceListener.shared.sendCheck(check); syncMeter() }
    private func done(_ c: VoiceCheck) { busy = false; check = c; VoiceListener.shared.sendCheck(c); syncMeter() }
    private func syncMeter() { if check.mic == .granted && !check.device.isEmpty { meter.start() } else { meter.stop() } }
}

private struct CheckRow<Tail: View>: View {
    let icon: String
    let title: String
    let ok: Bool
    let state: String
    let hint: String?
    @ViewBuilder var tail: () -> Tail

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            Image(systemName: icon).font(.system(size: 14)).foregroundColor(Palette.txt).frame(width: 22).padding(.top, 2)
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 8) {
                    Text(title).font(.system(size: 14, weight: .semibold)).foregroundColor(Palette.txt)
                    Label(state, systemImage: ok ? "checkmark.circle.fill" : "exclamationmark.circle.fill")
                        .font(.system(size: 12, weight: .medium)).foregroundColor(ok ? Palette.ok : Palette.bad).lineLimit(1)
                }
                if let hint { Text(hint).font(.system(size: 12)).foregroundColor(Palette.dim).fixedSize(horizontal: false, vertical: true) }
            }
            Spacer(minLength: 8)
            tail()
        }
        .padding(.horizontal, 14).padding(.vertical, 12)
    }
}

// MARK: - самоперевірка без людини
//
//   open -n «ПОТУЖНЕ РАДІО.app» --args --voice-check <звіт.json> [<аудіофайл>] [--render <знімок.png>]
//        [--render-denied <знімок.png>] [--live <фраза.aiff>]
//
// Через open — щоб дозволи macOS рахувались саме цій програмі, а не терміналу.
// Пише стан дозволів, рівень мікрофона за 2 с, розпізнаний текст файлу (uk-UA),
// знімки вікна перевірки (справжній стан і «заборонено»). --live — уся дорога
// голосу: чекає сторінку радіо, тисне її кнопку з мікрофоном, грає фразу в
// динаміки й записує, що сторінка отримала (команду НЕ виконує — відповідь
// перехоплено). Потім програма виходить.
enum VoiceSelfTest {
    @MainActor static func runIfAsked(model: AppModel) -> Bool {
        let a = CommandLine.arguments
        guard let i = a.firstIndex(of: "--voice-check"), i + 1 < a.count else { return false }
        let out = URL(fileURLWithPath: a[i + 1])
        let audio = (i + 2 < a.count && !a[i + 2].hasPrefix("--")) ? a[i + 2] : nil
        func opt(_ k: String) -> String? { a.firstIndex(of: k).flatMap { $0 + 1 < a.count ? a[$0 + 1] : nil } }
        let render = opt("--render"), renderDenied = opt("--render-denied"), live = opt("--live"), buffers = opt("--buffers")
        Task { @MainActor in
            var report: [String: Any] = ["check": VoiceCheck.now().json, "bundle": Bundle.main.bundleIdentifier ?? ""]
            report["micPeak"] = await micPeak(seconds: 2)
            if let audio { report["file"] = await recognize(URL(fileURLWithPath: audio)) }
            if let render { snapshot(VoiceCheckView().environmentObject(model), render) }
            if let renderDenied {
                let c = VoiceCheck(mic: .denied, speech: .ask, device: "Вбудований мікрофон", language: true, available: true)
                snapshot(VoiceCheckView(preset: c).environmentObject(model), renderDenied)
            }
            if let buffers {
                let u = URL(fileURLWithPath: buffers)
                report["buffers4ch"] = await recognizeBuffers(u, channels: 4, mono: false)
                report["buffers4chMono"] = await recognizeBuffers(u, channels: 4, mono: true)
            }
            if let live { report["live"] = await liveRun(model: model, phrase: URL(fileURLWithPath: live)) }
            if let d = try? JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted, .sortedKeys]) { try? d.write(to: out) }
            exit(0)
        }
        return true
    }

    @MainActor private static func snapshot<V: View>(_ v: V, _ path: String) {
        let r = ImageRenderer(content: v)
        r.scale = 2
        if let img = r.nsImage, let tiff = img.tiffRepresentation, let rep = NSBitmapImageRep(data: tiff),
           let png = rep.representation(using: .png, properties: [:]) { try? png.write(to: URL(fileURLWithPath: path)) }
    }

    @MainActor private static func js(_ wv: WKWebView, _ code: String) async -> Any? {
        await withCheckedContinuation { c in wv.evaluateJavaScript(code) { r, _ in c.resume(returning: r) } }
    }

    @MainActor private static func liveRun(model: AppModel, phrase: URL) async -> [String: Any] {
        var ready = false
        var seen: Any = "вебвікна ще немає"
        for _ in 0..<180 {
            if let wv = model.web {
                seen = await js(wv, "JSON.stringify({url: location.href, state: document.readyState, start: typeof voiceStart, bridge: typeof voiceBridge === 'function' ? voiceBridge() : null})") ?? "js не відповів"
                if (await js(wv, "typeof voiceStart === 'function' && voiceBridge() === 'mac'")) as? Bool == true { ready = true; break }
            }
            try? await Task.sleep(nanoseconds: 500_000_000)
        }
        guard ready, let wv = model.web else {
            return ["error": "сторінка радіо не відкрилась або не бачить міст голосу", "radio": model.current?.ip ?? "не підключено",
                    "message": model.message ?? "", "page": seen]
        }
        _ = await js(wv, """
            window.__vt = [];
            window.potuzhneVoicePartial = t => __vt.push(['partial', t]);
            window.potuzhneVoiceError = m => { __vt.push(['error', m]); voiceDone(); };
            window.potuzhneVoiceResult = l => { __vt.push(['result', l]); voiceDone(); };
            window.potuzhneVoicePerm = p => __vt.push(['perm', p]);
            voiceStart(); true
            """)
        try? await Task.sleep(nanoseconds: 700_000_000)
        let player = NSSound(contentsOf: phrase, byReference: true)
        player?.play()
        var got: [Any] = []
        for _ in 0..<24 {
            try? await Task.sleep(nanoseconds: 500_000_000)
            got = (await js(wv, "window.__vt") as? [Any]) ?? []
            if got.contains(where: { (($0 as? [Any])?.first as? String).map { $0 == "result" || $0 == "error" } ?? false }) { break }
        }
        let busy = await js(wv, "VOICE.busy") as? Bool
        // картка «Дозволи на цьому Mac» у розділі голосових команд (є на сторінці з 1.4.17)
        _ = await js(wv, "location.hash = '#/voice'; true")
        try? await Task.sleep(nanoseconds: 3_000_000_000)
        let card = await js(wv, "(() => { const c = [...document.querySelectorAll('.card')].find(x => (x.querySelector('h2') || {}).textContent === 'Дозволи на цьому Mac'); return c ? c.innerText : 'картки немає'; })()")
        return ["events": got, "busyAfter": busy as Any, "permCard": card ?? "js не відповів"]
    }

    @MainActor private static func micPeak(seconds: Double) async -> Float {
        guard AVCaptureDevice.authorizationStatus(for: .audio) == .authorized else { return -1 }
        let m = MicMeter()
        m.start()
        var peak: Float = 0
        let end = Date().addingTimeInterval(seconds)
        while Date() < end { try? await Task.sleep(nanoseconds: 50_000_000); peak = max(peak, m.level) }
        m.stop()
        return peak
    }

    /// Фраза з файлу — тією ж дорогою, що й живий мікрофон: буферами по 1024 кадри,
    /// розмноженими на `channels` каналів (як у вбудованого мікрофона), з перетворенням
    /// на один канал чи без.
    private static func recognizeBuffers(_ url: URL, channels: AVAudioChannelCount, mono: Bool) async -> [String: Any] {
        guard let rec = SFSpeechRecognizer(locale: Locale(identifier: "uk-UA")),
              let file = try? AVAudioFile(forReading: url),
              let layout = AVAudioChannelLayout(layoutTag: kAudioChannelLayoutTag_DiscreteInOrder | UInt32(channels)),
              // понад 2 канали AVAudioFormat без розкладки каналів не створює
              case let fmt = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: file.processingFormat.sampleRate, interleaved: false, channelLayout: layout),
              let whole = AVAudioPCMBuffer(pcmFormat: file.processingFormat, frameCapacity: AVAudioFrameCount(file.length)),
              (try? file.read(into: whole)) != nil, let srcData = whole.floatChannelData else { return ["error": "файл не прочитано"] }
        let req = SFSpeechAudioBufferRecognitionRequest()
        req.shouldReportPartialResults = false
        var pos: AVAudioFrameCount = 0
        // секунда тиші попереду й позаду — як у живому слуханні
        let pad = AVAudioFrameCount(fmt.sampleRate)
        let total = whole.frameLength + 2 * pad
        while pos < total {
            let n = min(1024, total - pos)
            guard let b = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: n), let d = b.floatChannelData else { break }
            b.frameLength = n
            for c in 0..<Int(channels) {
                for i in 0..<Int(n) {
                    let at = Int(pos) + i - Int(pad)
                    d[c][i] = at >= 0 && at < Int(whole.frameLength) ? srcData[0][at] : 0
                }
            }
            if mono { if let m = MonoBuffer.from(b) { req.append(m) } } else { req.append(b) }
            pos += n
        }
        req.endAudio()
        return await withCheckedContinuation { cont in
            var done = false
            let t0 = Date()
            _ = rec.recognitionTask(with: req) { result, error in
                guard !done else { return }
                if let result, result.isFinal { done = true; cont.resume(returning: ["text": result.bestTranscription.formattedString, "sec": Date().timeIntervalSince(t0)]); return }
                if let error { done = true; let e = error as NSError; cont.resume(returning: ["error": e.localizedDescription, "domain": e.domain, "code": e.code]) }
            }
        }
    }

    private static func recognize(_ url: URL) async -> [String: Any] {
        guard let rec = SFSpeechRecognizer(locale: Locale(identifier: "uk-UA")) else { return ["error": "немає uk-UA"] }
        guard SFSpeechRecognizer.authorizationStatus() == .authorized else { return ["error": "розпізнавання не дозволено"] }
        return await withCheckedContinuation { cont in
            var done = false
            let req = SFSpeechURLRecognitionRequest(url: url)
            req.shouldReportPartialResults = false
            _ = rec.recognitionTask(with: req) { result, error in
                guard !done else { return }
                if let result, result.isFinal { done = true; cont.resume(returning: ["text": result.bestTranscription.formattedString]); return }
                if let error {
                    done = true
                    let e = error as NSError
                    cont.resume(returning: ["error": e.localizedDescription, "domain": e.domain, "code": e.code,
                                            "explain": VoiceListener.explain(error).0])
                }
            }
        }
    }
}
