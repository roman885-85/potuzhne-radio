// Кольори й кнопки — ті самі, що на екрані радіо та на його сторінці.

import SwiftUI

enum Palette {
    static let bg = Color.black
    static let pan = Color(red: 0x11/255, green: 0x11/255, blue: 0x11/255)
    static let pan2 = Color(red: 0x1b/255, green: 0x1b/255, blue: 0x1b/255)
    static let line = Color(red: 0x26/255, green: 0x26/255, blue: 0x26/255)
    static let txt = Color(red: 0xf2/255, green: 0xf2/255, blue: 0xf2/255)
    static let dim = Color(red: 0x8c/255, green: 0x8c/255, blue: 0x8c/255)
    static let acc = Color(red: 0xe6/255, green: 0xd2/255, blue: 0x5a/255)
    static let bad = Color(red: 0xef/255, green: 0x5b/255, blue: 0x5b/255)
}

struct PillButton: ButtonStyle {
    var accent: Bool
    func makeBody(configuration: Configuration) -> some View { Pill(configuration: configuration, accent: accent) }
}

private struct Pill: View {
    let configuration: ButtonStyle.Configuration
    let accent: Bool
    @Environment(\.isEnabled) private var enabled
    var body: some View {
        configuration.label
            .font(.system(size: 13, weight: .semibold))
            .foregroundColor(accent ? .black : Palette.txt)
            .padding(.horizontal, 14).padding(.vertical, 8)
            .background(RoundedRectangle(cornerRadius: 8).fill(accent ? Palette.acc : Palette.pan2))
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(accent ? Palette.acc : Color(white: 0.2)))
            .opacity(!enabled ? 0.4 : configuration.isPressed ? 0.8 : 1)
    }
}

