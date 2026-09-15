// ПОТУЖНЕ РАДІО — клієнт для Mac.
//
// Сам знаходить радіо в мережі (Bonjour і перебір підмережі, див. Discovery.swift)
// і показує його сторінку у своєму вікні. Свого інтерфейсу радіо тут немає
// навмисно: усе вміє сторінка радіо, і так воно однакове в браузері, на Mac,
// Windows і Android.

import SwiftUI
import Combine
import WebKit

@main
struct PotuzhneRadioApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup("ПОТУЖНЕ РАДІО") {
            RootView()
                .environmentObject(model)
                .frame(minWidth: 420, minHeight: 560)
                .preferredColorScheme(.dark)
        }
        .defaultSize(width: 1120, height: 800)
        .commands {
            CommandGroup(replacing: .newItem) {}
            CommandMenu("Радіо") {
                Button("Знайти інше радіо") { model.search() }.keyboardShortcut("k")
                Button("Оновити сторінку") { model.reload() }.keyboardShortcut("r")
                    .disabled(model.current == nil)
                Button("Голосові команди") { model.showVoiceHelp() }
                    .disabled(model.current == nil)
                Button("Дозволи для голосових команд…") { model.voiceCheckShown = true }
                Divider()
                Button("Відкрити в браузері") { model.openInBrowser() }
                    .disabled(model.current == nil)
            }
            CommandGroup(replacing: .help) {
                Button("Голосові команди") { model.showVoiceHelp() }
                    .disabled(model.current == nil)
                Button("Дозволи для голосових команд…") { model.voiceCheckShown = true }
            }
        }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

@MainActor
final class AppModel: ObservableObject {
    @Published var current: Radio?
    @Published var message: String?
    @Published var notice: String?
    @Published var tryingSaved = false
    @Published var voiceCheckShown = false      // вікно «Голосові команди на цьому Mac»
    let finder = RadioFinder()
    weak var web: WKWebView?

    private var health: Timer?
    private var fails = 0
    private var autoConnect = true
    private var watch: Any?
    private var active: Any?

    private let kLastIP = "lastIP", kLastHost = "lastHost"

    init() {
        // Радіо знайшлось: якщо це те саме, до якого підключались минулого разу
        // (адреса могла змінитись, ім'я — ні), або воно одне в мережі — одразу туди.
        watch = finder.objectWillChange.sink { [weak self] _ in
            DispatchQueue.main.async { self?.maybeAutoConnect() }
        }
        VoiceListener.shared.showCheck = { [weak self] in self?.voiceCheckShown = true }
        // повернулись із «Системних параметрів» — картка дозволів на сторінці бачить новий стан
        active = NotificationCenter.default.addObserver(forName: NSApplication.didBecomeActiveNotification, object: nil, queue: .main) { [weak self] _ in
            Task { @MainActor in if let wv = self?.web { VoiceListener.shared.sendCheck(to: wv) } }
        }
        _ = VoiceSelfTest.runIfAsked(model: self)          // --voice-check: самоперевірка голосу (див. VoiceCheckView.swift)
        Task { await start() }
    }

    func start() async {
        if let ip = UserDefaults.standard.string(forKey: kLastIP) {
            tryingSaved = true
            let r = await RadioProbe.hello(ip, timeout: 2.5)
            tryingSaved = false
            if let r { connect(r); return }
        }
        search()
    }

    func search(message: String? = nil) {
        health?.invalidate(); health = nil
        current = nil
        self.message = message
        autoConnect = message == nil
        finder.start()
    }

    private func maybeAutoConnect() {
        guard current == nil, autoConnect else { return }
        let lastHost = UserDefaults.standard.string(forKey: kLastHost) ?? ""
        if !lastHost.isEmpty, let r = finder.radios.first(where: { $0.host == lastHost }) { connect(r); return }
        if !finder.searching, finder.radios.count == 1 { connect(finder.radios[0]) }
    }

    func connect(_ r: Radio) {
        finder.stop()
        UserDefaults.standard.set(r.ip, forKey: kLastIP)
        if !r.host.isEmpty { UserDefaults.standard.set(r.host, forKey: kLastHost) }
        message = nil
        fails = 0
        current = r
        health?.invalidate()
        health = Timer.scheduledTimer(withTimeInterval: 10, repeats: true) { [weak self] _ in
            Task { @MainActor in await self?.checkHealth() }
        }
    }

    /// Раз на 10 с питаємо радіо; тричі поспіль мовчить — назад до пошуку.
    private func checkHealth() async {
        guard let r = current else { return }
        if await RadioProbe.hello(r.ip, timeout: 3) != nil { fails = 0; return }
        fails += 1
        if fails >= 3 { search(message: "Зв'язок з радіо втрачено. Шукаю знову…") }
    }

    func connect(address: String) async -> Bool {
        let a = address.trimmingCharacters(in: .whitespacesAndNewlines)
            .replacingOccurrences(of: "http://", with: "").replacingOccurrences(of: "/", with: "")
        guard !a.isEmpty, let r = await finder.check(a) else { return false }
        connect(r)
        return true
    }

    func reload() { web?.reload() }
    /// Інструкція з голосових команд — розділ самої сторінки радіо.
    func showVoiceHelp() { web?.evaluateJavaScript("location.hash = '#/voice'", completionHandler: nil) }
    /// Та сама кнопка з мікрофоном, що вгорі сторінки радіо.
    func startVoice() { web?.evaluateJavaScript("typeof voiceStart === 'function' && voiceStart()", completionHandler: nil) }
    func openInBrowser() { if let r = current, let u = URL(string: "http://\(r.ip)/") { NSWorkspace.shared.open(u) } }

    func show(notice text: String) {
        notice = text
        DispatchQueue.main.asyncAfter(deadline: .now() + 4) { [weak self] in if self?.notice == text { self?.notice = nil } }
    }
}

struct RootView: View {
    @EnvironmentObject var model: AppModel
    var body: some View {
        ZStack(alignment: .bottom) {
            if let r = model.current {
                RadioWebView(radio: r)
                    .navigationTitle("ПОТУЖНЕ РАДІО — \(r.host.isEmpty ? r.ip : r.host)")
                    .ignoresSafeArea()
            } else {
                SearchView()
                    .navigationTitle("ПОТУЖНЕ РАДІО")
            }
            if let n = model.notice {
                Text(n).font(.system(size: 13)).foregroundColor(Palette.txt)
                    .padding(.horizontal, 14).padding(.vertical, 9)
                    .background(RoundedRectangle(cornerRadius: 10).fill(Color(white: 0.13)))
                    .overlay(RoundedRectangle(cornerRadius: 10).stroke(Palette.line))
                    .padding(.bottom, 90)
                    .transition(.opacity)
            }
        }
        .background(Palette.bg)
        .animation(.easeInOut(duration: 0.2), value: model.notice)
        .sheet(isPresented: $model.voiceCheckShown) {
            VoiceCheckView(close: { model.voiceCheckShown = false }).environmentObject(model)
        }
    }
}
