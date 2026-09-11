// Збірка ПОТУЖНОГО РАДІО — вікно програми.

import SwiftUI

@main
struct BuilderApp: App {
    @NSApplicationDelegateAdaptor(QuitOnClose.self) var delegate
    @StateObject private var model = BuilderModel()
    var body: some Scene {
        WindowGroup("Збірка ПОТУЖНОГО РАДІО") {
            BuilderView().environmentObject(model)
                .frame(minWidth: 640, minHeight: 640)
                .preferredColorScheme(.dark)
        }
        .defaultSize(width: 780, height: 820)
        .commands { CommandGroup(replacing: .newItem) {} }
    }
}

final class QuitOnClose: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

struct Card<Content: View>: View {
    let title: String
    @ViewBuilder var content: Content
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title.uppercased()).font(.system(size: 12, weight: .semibold)).tracking(1).foregroundColor(Palette.dim)
            content
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 12).fill(Palette.pan))
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(Palette.line))
    }
}

struct BuilderView: View {
    @EnvironmentObject var m: BuilderModel
    @State private var showLog = false

    var body: some View {
        ScrollView {
            VStack(spacing: 14) {
                header
                buildCard
                if showLog || m.state == .running || isFailed { logCard }
                RadioCard(finder: m.finder)
            }
            .padding(20)
        }
        .background(Palette.bg)
    }

    private var isFailed: Bool { if case .failed = m.state { return true }; return false }

    private var header: some View {
        HStack(spacing: 14) {
            Image(nsImage: NSApp.applicationIconImage).resizable().frame(width: 56, height: 56)
            VStack(alignment: .leading, spacing: 3) {
                Text("Збірка ПОТУЖНОГО РАДІО").font(.system(size: 19, weight: .bold)).foregroundColor(Palette.acc)
                HStack(spacing: 6) {
                    Text(m.projectOK ? m.project.path : "Теку проєкту не знайдено").font(.system(size: 12))
                        .foregroundColor(m.projectOK ? Palette.dim : Palette.bad).lineLimit(1).truncationMode(.middle)
                    Button("Змінити…") { m.chooseProject() }.buttonStyle(.link).font(.system(size: 12))
                }
            }
            Spacer()
        }
    }

    private var buildCard: some View {
        Card(title: "Прошивка") {
            HStack(spacing: 10) {
                Text("Версія").foregroundColor(Palette.dim)
                TextField("1.0", text: $m.version).textFieldStyle(.roundedBorder).frame(width: 90)
                    .disabled(m.state == .running)
                Button("Наступна") { m.bumpVersion() }.buttonStyle(PillButton(accent: false)).disabled(m.state == .running)
                Spacer()
                if m.state == .running {
                    Button { m.cancel() } label: { Label("Зупинити", systemImage: "stop.fill") }.buttonStyle(PillButton(accent: false))
                } else {
                    Button { m.build() } label: { Label("Зібрати", systemImage: "hammer.fill") }.buttonStyle(PillButton(accent: true))
                        .disabled(!m.projectOK || !m.versionValid)
                }
            }
            if !m.versionValid { Text("Номер версії — цифри через крапку, наприклад 1.1").font(.system(size: 12)).foregroundColor(Palette.bad) }
            status
            Divider().background(Palette.line)
            if let fw = m.fw {
                HStack(alignment: .top) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("PotuzhneRadio-ES3C28P-update.bin").font(.system(size: 13, weight: .semibold))
                        Text("ПОТУЖНЕ РАДІО \(fw.version) · зібрано \(fw.build) · \(sizeText(fw.size))").font(.system(size: 12)).foregroundColor(Palette.dim)
                        Text("Цей файл — для оновлення через Wi-Fi: тут нижче, або на сторінці радіо в розділі «Оновлення». Поруч у firmware — повний образ для кабелю й файли сторінки (web).")
                            .font(.system(size: 12)).foregroundColor(Palette.dim).fixedSize(horizontal: false, vertical: true)
                    }
                    Spacer()
                    Button { m.revealFirmware() } label: { Label("У Finder", systemImage: "folder") }.buttonStyle(PillButton(accent: false))
                }
            } else {
                Text("Готового файлу ще немає — натисніть «Зібрати».").font(.system(size: 12)).foregroundColor(Palette.dim)
            }
            HStack {
                Spacer()
                Button(showLog ? "Сховати журнал" : "Показати журнал") { showLog.toggle() }.buttonStyle(.link).font(.system(size: 12))
            }
        }
    }

    @ViewBuilder private var status: some View {
        switch m.state {
        case .running:
            TimelineView(.periodic(from: .now, by: 1)) { _ in
                let t = Int(Date().timeIntervalSince(m.started ?? Date()))
                VStack(alignment: .leading, spacing: 6) {
                    HStack { Text(m.phase).font(.system(size: 13, weight: .semibold)); Spacer(); Text(String(format: "%d:%02d", t / 60, t % 60)).monospacedDigit().foregroundColor(Palette.dim) }
                    ProgressView().progressViewStyle(.linear).tint(Palette.acc)
                    Text("Звичайно 3–5 хвилин; якщо змінено спільні файли — до 13. Радіо тим часом грає як грало.").font(.system(size: 12)).foregroundColor(Palette.dim)
                }
            }
        case .ok:
            Label("Зібрано за \(Int(m.finishedIn) / 60):\(String(format: "%02d", Int(m.finishedIn) % 60))", systemImage: "checkmark.circle.fill")
                .foregroundColor(Palette.acc).font(.system(size: 13, weight: .semibold))
        case .failed(let e):
            Label(e, systemImage: "exclamationmark.triangle.fill").foregroundColor(Palette.bad).font(.system(size: 13))
        case .cancelled:
            Text("Збірку зупинено. Наступна піде з нуля — довше, ніж зазвичай.").font(.system(size: 12)).foregroundColor(Palette.dim)
        case .idle:
            EmptyView()
        }
    }

    private var logCard: some View {
        Card(title: "Журнал збірки") {
            ScrollViewReader { sp in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 1) {
                        ForEach(Array(m.log.enumerated()), id: \.offset) { i, l in
                            Text(l).font(.system(size: 11, design: .monospaced))
                                .foregroundColor(l.hasPrefix(">>>") ? Palette.acc : (l.contains("error") ? Palette.bad : Palette.dim))
                                .textSelection(.enabled).id(i)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                .frame(height: 220)
                .onChange(of: m.log.count) { n in if n > 0 { sp.scrollTo(n - 1, anchor: .bottom) } }
            }
        }
    }
}

struct RadioCard: View {
    @EnvironmentObject var m: BuilderModel
    @ObservedObject var finder: RadioFinder
    var body: some View {
        Card(title: "Оновити радіо по Wi-Fi") {
            HStack {
                if finder.searching { ProgressView().controlSize(.small); Text("Шукаю радіо в мережі…").font(.system(size: 12)).foregroundColor(Palette.dim) }
                else if finder.radios.isEmpty { Text("Радіо в мережі не знайдено.").font(.system(size: 12)).foregroundColor(Palette.dim) }
                Spacer()
                Button { m.finder.start() } label: { Label("Шукати", systemImage: "arrow.clockwise") }.buttonStyle(PillButton(accent: false)).disabled(finder.searching)
            }
            if finder.blocked && !finder.searching {
                HStack(alignment: .top, spacing: 10) {
                    Text("macOS не дає програмі доступу до локальної мережі. Якщо «Збірка ПОТУЖНОГО РАДІО» є в налаштуваннях «Локальна мережа» — увімкніть її. Якщо немає — закрийте програму й відкрийте знову: macOS спитає дозвіл, натисніть «Дозволити».")
                        .font(.system(size: 12)).foregroundColor(Palette.txt).fixedSize(horizontal: false, vertical: true)
                    Spacer()
                    Button("Відкрити налаштування") { RadioFinder.openLocalNetworkSettings() }.buttonStyle(PillButton(accent: true))
                }
            }
            ForEach(finder.radios) { r in
                let sel = m.selected?.ip == r.ip
                HStack(spacing: 12) {
                    Image(systemName: sel ? "largecircle.fill.circle" : "circle").foregroundColor(sel ? Palette.acc : Palette.dim)
                    VStack(alignment: .leading, spacing: 2) {
                        Text("ПОТУЖНЕ РАДІО · \(r.subtitle)").font(.system(size: 13, weight: .semibold))
                        Text(r.build.isEmpty ? "версія невідома" : "на радіо: \(r.version), зібрано \(r.build)" + (m.compareText(radio: r).isEmpty ? "" : " — " + m.compareText(radio: r)))
                            .font(.system(size: 12)).foregroundColor(Palette.dim)
                    }
                    Spacer()
                }
                .padding(10)
                .background(RoundedRectangle(cornerRadius: 10).fill(sel ? Palette.pan2 : Color.clear))
                .contentShape(Rectangle())
                .onTapGesture { m.selected = r }
            }
            HStack(spacing: 10) {
                Button { if let r = m.selected { m.uploadFirmware(to: r) } } label: { Label("Залити прошивку", systemImage: "arrow.up.circle.fill") }
                    .buttonStyle(PillButton(accent: true)).disabled(m.selected == nil || m.fw == nil || m.uploading || m.state == .running)
                Button { if let r = m.selected { m.uploadWeb(to: r) } } label: { Label("Залити сторінку", systemImage: "doc.richtext") }
                    .buttonStyle(PillButton(accent: false)).disabled(m.selected == nil || m.uploading || m.state == .running)
                Spacer()
            }
            if m.uploading { ProgressView(value: m.uploadProgress).tint(Palette.acc) }
            if !m.uploadStatus.isEmpty { Text(m.uploadStatus).font(.system(size: 12)).foregroundColor(Palette.txt) }
            if !m.uploadError.isEmpty { Text(m.uploadError).font(.system(size: 12)).foregroundColor(Palette.bad) }
            Text("На час оновлення звук на радіо зупиниться. Станції, мережі, обране й налаштування лишаються.")
                .font(.system(size: 12)).foregroundColor(Palette.dim)
        }
    }
}
