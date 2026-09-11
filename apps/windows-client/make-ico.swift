// =============================================================================
//  make-ico.swift — малює іконку програми (антена радіо) у .ico
// =============================================================================
//  Жовта антена #e6d25a на чорному скругленому квадраті. Геометрія — дослівно
//  з SVG (поле 24×24, лінія 2, круглі кінці), розрахунок дуг той самий, що в
//  Sources/Ui/Antenna.cs, тож іконка й значок у вікні збігаються.
//
//  Усередині .ico — PNG-кадри 16…256: Windows бере потрібний розмір сама
//  (16 — заголовок вікна, 32/48 — Провідник, 256 — великі значки).
//
//  Варіант «build» — з шестернею в правому нижньому куті, для програми збірки
//  прошивки; повторює apps/shared/make-icon.swift (значок Mac-версії).
//
//  Використання:  swiftc -O make-ico.swift -o make-ico && ./make-ico AppIcon.ico [прев'ю.png|-] [build]
// =============================================================================
import Foundation
import CoreGraphics
import ImageIO

let sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
let variant = CommandLine.arguments.count > 3 ? CommandLine.arguments[3] : ""

/// Дуга SVG з точки (x, y) до (x, y + h) радіусом r (менша за півколо).
/// Повертає центр і кути в градусах, вісь Y — вниз, кут росте за годинниковою.
func svgArc(_ x: Double, _ y: Double, _ h: Double, _ r: Double, bulgeLeft: Bool)
    -> (cx: Double, cy: Double, start: Double, sweep: Double)
{
    let half = h / 2
    let d = (r * r - half * half).squareRoot()
    let a = atan2(half, d) * 180 / .pi
    return (bulgeLeft ? x + d : x - d, y + half, bulgeLeft ? 180 - a : -a, 2 * a)
}

let arcs: [(Double, Double, Double, Double, Bool)] = [
    (8.5, 7.5, 7, 5, true), (15.5, 7.5, 7, 5, false),
    (5.6, 4.6, 12.8, 9, true), (18.4, 4.6, 12.8, 9, false),
]

func render(_ n: Int) -> CGImage {
    let cs = CGColorSpace(name: CGColorSpace.sRGB)!
    let ctx = CGContext(data: nil, width: n, height: n, bitsPerComponent: 8, bytesPerRow: 0,
                        space: cs, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    // Далі все у координатах із віссю Y донизу — як у SVG і в GDI+.
    ctx.translateBy(x: 0, y: CGFloat(n))
    ctx.scaleBy(x: 1, y: -1)
    ctx.setShouldAntialias(true)
    ctx.interpolationQuality = .high

    let size = Double(n)
    // Маленькі розміри (16–24) — без полів і з тоншою лінією: інакше кружечок
    // зливається з внутрішніми дугами в одну пляму.
    let small = n <= 24
    let margin = small ? 0.0 : size * 0.04
    let square = CGRect(x: margin, y: margin, width: size - 2 * margin, height: size - 2 * margin)
    let radius = square.width * 0.22
    ctx.addPath(CGPath(roundedRect: square, cornerWidth: radius, cornerHeight: radius, transform: nil))
    ctx.setFillColor(CGColor(srgbRed: 0, green: 0, blue: 0, alpha: 1))
    ctx.fillPath()

    // Малюнок — у полі 24×24, вписаному в квадрат із відступом.
    let inset = square.width * (small ? 0.0 : 0.15)
    let box = square.insetBy(dx: inset, dy: inset)
    let s = box.width / 24
    let ox = box.minX, oy = box.minY - 0.8 * s   // межі малюнка y 3.6…22 — центруємо
    func P(_ x: Double, _ y: Double) -> CGPoint { CGPoint(x: ox + x * s, y: oy + y * s) }

    ctx.setStrokeColor(CGColor(srgbRed: 0xe6 / 255.0, green: 0xd2 / 255.0, blue: 0x5a / 255.0, alpha: 1))
    ctx.setLineWidth((small ? 1.7 : 2.0) * s)
    ctx.setLineCap(.round)
    ctx.setLineJoin(.round)

    // Кружечок у центрі.
    ctx.addEllipse(in: CGRect(origin: P(10, 9), size: CGSize(width: 4 * s, height: 4 * s)))
    ctx.strokePath()

    // Дуги — ламаними по 64 відрізки: так не залежимо від того, куди
    // CoreGraphics повертає «за годинниковою» у перевернутих координатах.
    for (x, y, h, r, left) in arcs {
        let a = svgArc(x, y, h, r, bulgeLeft: left)
        let steps = 64
        for i in 0...steps {
            let t = (a.start + a.sweep * Double(i) / Double(steps)) * .pi / 180
            let p = P(a.cx + r * cos(t), a.cy + r * sin(t))
            if i == 0 { ctx.move(to: p) } else { ctx.addLine(to: p) }
        }
        ctx.strokePath()
    }

    // Ніжка.
    ctx.move(to: P(12, 13))
    ctx.addLine(to: P(12, 21))
    ctx.strokePath()

    if variant == "build" {
        // Шестерня в правому нижньому куті — «збірка»: чорний диск, 16-кутна
        // зірка через зубець і чорна дірка посередині, як у Mac-версії.
        let gx = square.minX + square.width * 0.78, gy = square.minY + square.height * 0.78
        let R = square.width * 0.15
        ctx.setFillColor(CGColor(srgbRed: 0, green: 0, blue: 0, alpha: 1))
        ctx.fillEllipse(in: CGRect(x: gx - R * 1.35, y: gy - R * 1.35, width: R * 2.7, height: R * 2.7))
        ctx.setFillColor(CGColor(srgbRed: 0xe6 / 255.0, green: 0xd2 / 255.0, blue: 0x5a / 255.0, alpha: 1))
        for i in 0..<16 {
            let ang = Double(i) * .pi / 8, rr = i % 2 == 0 ? R : R * 0.78
            let p = CGPoint(x: gx + cos(ang) * rr, y: gy + sin(ang) * rr)
            if i == 0 { ctx.move(to: p) } else { ctx.addLine(to: p) }
        }
        ctx.closePath()
        ctx.fillPath()
        ctx.setFillColor(CGColor(srgbRed: 0, green: 0, blue: 0, alpha: 1))
        ctx.fillEllipse(in: CGRect(x: gx - R * 0.35, y: gy - R * 0.35, width: R * 0.7, height: R * 0.7))
    }

    return ctx.makeImage()!
}

func png(_ img: CGImage) -> Data {
    let data = NSMutableData()
    let dest = CGImageDestinationCreateWithData(data as CFMutableData, "public.png" as CFString, 1, nil)!
    CGImageDestinationAddImage(dest, img, nil)
    CGImageDestinationFinalize(dest)
    return data as Data
}

let args = CommandLine.arguments
guard args.count >= 2 else {
    FileHandle.standardError.write("Використання: make-ico <вихід.ico> [прев'ю-256.png]\n".data(using: .utf8)!)
    exit(2)
}

let frames = sizes.map { png(render($0)) }

// Формат .ico: ICONDIR (6 байт), по ICONDIRENTRY (16 байт) на кадр, далі самі PNG.
var ico = Data()
func u16(_ v: Int) { var x = UInt16(v).littleEndian; ico.append(Data(bytes: &x, count: 2)) }
func u32(_ v: Int) { var x = UInt32(v).littleEndian; ico.append(Data(bytes: &x, count: 4)) }
u16(0); u16(1); u16(sizes.count)
var offset = 6 + 16 * sizes.count
for (i, n) in sizes.enumerated() {
    ico.append(UInt8(n >= 256 ? 0 : n))   // ширина (0 = 256)
    ico.append(UInt8(n >= 256 ? 0 : n))   // висота
    ico.append(0)                          // кольорів у палітрі
    ico.append(0)                          // зарезервовано
    u16(1)                                 // площин
    u16(32)                                // біт на піксель
    u32(frames[i].count)
    u32(offset)
    offset += frames[i].count
}
for f in frames { ico.append(f) }

try! ico.write(to: URL(fileURLWithPath: args[1]))
if args.count >= 3 && args[2] != "-" {
    try! png(render(256)).write(to: URL(fileURLWithPath: args[2]))
}
print("\(args[1]): \(sizes.count) кадрів (\(sizes.map(String.init).joined(separator: ", "))), \(ico.count) байт")
