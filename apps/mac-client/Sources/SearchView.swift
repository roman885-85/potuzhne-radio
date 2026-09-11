// Екран пошуку: що знайшлось у мережі, підключення, ручна адреса.

import SwiftUI

struct SearchView: View {
    @EnvironmentObject var model: AppModel
    var body: some View { SearchContent(finder: model.finder) }
}

struct SearchContent: View {
    @EnvironmentObject var model: AppModel
    @ObservedObject var finder: RadioFinder
    @State private var manual = false

    var body: some View {
        ScrollView {
            VStack(spacing: 18) {
                Image(nsImage: NSApp.applicationIconImage).resizable().frame(width: 96, height: 96)
                    .padding(.top, 36)
                Text("ПОТУЖНЕ РАДІО").font(.system(size: 22, weight: .bold)).tracking(1.5).foregroundColor(Palette.acc)
                status
                if let m = model.message {
                    Text(m).font(.system(size: 13)).foregroundColor(Palette.bad).multilineTextAlignment(.center)
                }
                VStack(spacing: 10) {
                    ForEach(finder.radios) { r in RadioRow(radio: r) { model.connect(r) } }
                }
                .frame(maxWidth: 520)
                if finder.blocked && !finder.searching {
                    VStack(spacing: 10) {
                        Text("macOS не дає програмі доступу до локальної мережі, тому радіо не видно. Якщо «ПОТУЖНЕ РАДІО» є в налаштуваннях «Локальна мережа» — увімкніть його. Якщо його там немає — закрийте програму й відкрийте знову: macOS спитає дозвіл, натисніть «Дозволити».")
                            .font(.system(size: 13)).foregroundColor(Palette.txt).multilineTextAlignment(.center).frame(maxWidth: 460)
                        Button { RadioFinder.openLocalNetworkSettings() } label: { Label("Відкрити налаштування", systemImage: "gearshape") }.buttonStyle(PillButton(accent: true))
                    }
                } else if !finder.searching && finder.radios.isEmpty && !model.tryingSaved {
                    Text("Радіо не знайдено. Перевірте, що комп'ютер і радіо в одній мережі Wi-Fi, а в «Системних параметрах → Конфіденційність і безпека → Локальна мережа» програмі дозволено доступ.")
                        .font(.system(size: 13)).foregroundColor(Palette.dim).multilineTextAlignment(.center)
                        .frame(maxWidth: 460)
                }
                HStack(spacing: 10) {
                    Button { model.search() } label: { Label("Шукати ще раз", systemImage: "arrow.clockwise") }
                        .buttonStyle(PillButton(accent: false)).disabled(finder.searching)
                    Button { manual = true } label: { Label("Ввести адресу…", systemImage: "keyboard") }
                        .buttonStyle(PillButton(accent: false))
                }
                .padding(.top, 6)
                Spacer(minLength: 30)
            }
            .frame(maxWidth: .infinity)
            .padding(.horizontal, 24)
        }
        .background(Palette.bg)
        .sheet(isPresented: $manual) { ManualSheet(shown: $manual) }
    }

    @ViewBuilder private var status: some View {
        if model.tryingSaved {
            HStack(spacing: 8) { ProgressView().controlSize(.small); Text("Підключаюся до радіо, з яким працювали минулого разу…") }
                .font(.system(size: 13)).foregroundColor(Palette.dim)
        } else if finder.searching {
            HStack(spacing: 8) { ProgressView().controlSize(.small); Text("Шукаю радіо в мережі…") }
                .font(.system(size: 13)).foregroundColor(Palette.dim)
        } else if !finder.radios.isEmpty {
            Text(finder.radios.count == 1 ? "Знайдено одне радіо" : "Знайдено радіо: \(finder.radios.count)")
                .font(.system(size: 13)).foregroundColor(Palette.dim)
        }
    }
}

struct RadioRow: View {
    let radio: Radio
    let action: () -> Void
    @State private var hover = false
    var body: some View {
        Button(action: action) {
            HStack(spacing: 14) {
                Image(systemName: "dot.radiowaves.left.and.right").font(.system(size: 22)).foregroundColor(Palette.acc)
                    .frame(width: 44, height: 44).background(RoundedRectangle(cornerRadius: 10).fill(Palette.pan2))
                VStack(alignment: .leading, spacing: 3) {
                    Text(radio.title).font(.system(size: 15, weight: .semibold)).foregroundColor(Palette.txt)
                    Text(radio.subtitle).font(.system(size: 12)).foregroundColor(Palette.dim)
                    if !radio.station.isEmpty {
                        Text((radio.playing ? "грає: " : "") + radio.station).font(.system(size: 12)).foregroundColor(radio.playing ? Palette.acc : Palette.dim).lineLimit(1)
                    }
                }
                Spacer()
                Text("Підключитися").font(.system(size: 13, weight: .semibold)).foregroundColor(.black)
                    .padding(.horizontal, 14).padding(.vertical, 8).background(RoundedRectangle(cornerRadius: 8).fill(Palette.acc))
            }
            .padding(12)
            .background(RoundedRectangle(cornerRadius: 12).fill(hover ? Palette.pan2 : Palette.pan))
            .overlay(RoundedRectangle(cornerRadius: 12).stroke(Palette.line))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .onHover { hover = $0 }
    }
}

struct ManualSheet: View {
    @EnvironmentObject var model: AppModel
    @Binding var shown: Bool
    @State private var address = ""
    @State private var busy = false
    @State private var error = ""
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Адреса радіо").font(.system(size: 16, weight: .semibold))
            Text("IP-адреса з екрана радіо (наприклад 192.168.1.32) або ім'я в мережі (potuzhne-xxxx.local).")
                .font(.system(size: 12)).foregroundColor(Palette.dim).fixedSize(horizontal: false, vertical: true)
            TextField("192.168.1.32", text: $address).textFieldStyle(.roundedBorder).onSubmit(go)
            if !error.isEmpty { Text(error).font(.system(size: 12)).foregroundColor(Palette.bad) }
            HStack {
                Spacer()
                Button("Скасувати") { shown = false }.keyboardShortcut(.cancelAction)
                Button(busy ? "Перевіряю…" : "Підключитися", action: go).keyboardShortcut(.defaultAction).disabled(busy || address.isEmpty)
            }
        }
        .padding(20).frame(width: 400)
    }
    private func go() {
        busy = true; error = ""
        Task {
            let ok = await model.connect(address: address)
            busy = false
            if ok { shown = false } else { error = "За цією адресою ПОТУЖНЕ РАДІО не відповідає." }
        }
    }
}
