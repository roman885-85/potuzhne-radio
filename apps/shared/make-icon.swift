// Значок ПОТУЖНОГО РАДІО: жовта антена (#e6d25a) на чорному заокругленому квадраті —
// той самий знак, що на сторінці радіо (SVG 24x24: коло r=2 у (12,11), по дві дуги
// з боків, ніжка вниз). Малюємо самі, щоб не тримати готових картинок.
//
//   swiftc make-icon.swift -o make-icon && ./make-icon <тека.iconset> [варіант]
//   варіант «build» — зі значком шестерні в куті (для програми збірки)

import AppKit

let args = CommandLine.arguments
guard args.count >= 2 else { print("вкажіть теку .iconset"); exit(1) }
let out = URL(fileURLWithPath: args[1])
let variant = args.count > 2 ? args[2] : ""
try? FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)

let yellow = NSColor(calibratedRed: 0xE6/255.0, green: 0xD2/255.0, blue: 0x5A/255.0, alpha: 1)

func render(_ px: Int) -> Data? {
    guard let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: px, pixelsHigh: px, bitsPerSample: 8,
                                     samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                                     colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0) else { return nil }
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    let s = CGFloat(px)
    // поле як у значків macOS: квадрат ~80% із заокругленням
    let inset = s * 0.1, side = s - inset * 2
    let bg = NSBezierPath(roundedRect: NSRect(x: inset, y: inset, width: side, height: side), xRadius: side * 0.225, yRadius: side * 0.225)
    NSColor.black.setFill(); bg.fill()
    NSColor(calibratedWhite: 1, alpha: 0.08).setStroke(); bg.lineWidth = max(1, s / 256); bg.stroke()

    // SVG 24x24 -> поле; вісь y у SVG униз
    let k = side * 0.7 / 24, ox = inset + (side - 24 * k) / 2, oy = inset + (side - 24 * k) / 2 - side * 0.01
    func P(_ x: CGFloat, _ y: CGFloat) -> NSPoint { NSPoint(x: ox + x * k, y: oy + (24 - y) * k) }
    let lw = 2 * k
    yellow.setStroke()
    let circle = NSBezierPath(ovalIn: NSRect(x: P(10, 13).x, y: P(10, 13).y, width: 4 * k, height: 4 * k))
    circle.lineWidth = lw; circle.stroke()
    // дуги: центр (12,11); радіуси 5 і 9; кут ±45° навколо горизонталі
    for (r, a) in [(CGFloat(4.95), CGFloat(45)), (CGFloat(9.05), CGFloat(45))] {
        for left in [true, false] {
            let p = NSBezierPath()
            let c = P(12, 11)
            if left { p.appendArc(withCenter: c, radius: r * k, startAngle: 180 - a, endAngle: 180 + a, clockwise: false) }
            else    { p.appendArc(withCenter: c, radius: r * k, startAngle: -a, endAngle: a, clockwise: false) }
            p.lineWidth = lw; p.lineCapStyle = .round; p.stroke()
        }
    }
    let stem = NSBezierPath(); stem.move(to: P(12, 13)); stem.line(to: P(12, 21))
    stem.lineWidth = lw; stem.lineCapStyle = .round; stem.stroke()

    if variant == "build" {
        // шестерня в правому нижньому куті — «збірка»
        let gc = NSPoint(x: inset + side * 0.78, y: inset + side * 0.22), R = side * 0.15
        NSColor.black.setFill()
        NSBezierPath(ovalIn: NSRect(x: gc.x - R * 1.35, y: gc.y - R * 1.35, width: R * 2.7, height: R * 2.7)).fill()
        yellow.setFill()
        let g = NSBezierPath()
        for i in 0..<16 {
            let ang = CGFloat(i) * .pi / 8, rr = i % 2 == 0 ? R : R * 0.78
            let pt = NSPoint(x: gc.x + cos(ang) * rr, y: gc.y + sin(ang) * rr)
            if i == 0 { g.move(to: pt) } else { g.line(to: pt) }
        }
        g.close(); g.fill()
        NSColor.black.setFill()
        NSBezierPath(ovalIn: NSRect(x: gc.x - R * 0.35, y: gc.y - R * 0.35, width: R * 0.7, height: R * 0.7)).fill()
    }
    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])
}

for base in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let name = scale == 1 ? "icon_\(base)x\(base).png" : "icon_\(base)x\(base)@2x.png"
        if let d = render(base * scale) { try? d.write(to: out.appendingPathComponent(name)) }
    }
}
print("готово: \(out.path)")
