// Сторінка радіо у вікні. WKWebView сам не зберігає файли й не відкриває
// вибір файлу — усе це треба дати йому явно: записи з картки, експорт
// списку станцій (blob-посилання), імпорт, свій логотип, файл прошивки.

import SwiftUI
import WebKit

struct RadioWebView: NSViewRepresentable {
    let radio: Radio
    @EnvironmentObject var model: AppModel

    func makeCoordinator() -> Coordinator { Coordinator(model: model) }

    func makeNSView(context: Context) -> WKWebView {
        let cfg = WKWebViewConfiguration()
        cfg.websiteDataStore = .default()                 // localStorage сторінки — між запусками
        cfg.mediaTypesRequiringUserActionForPlayback = []
        cfg.preferences.isElementFullscreenEnabled = true
        let wv = WKWebView(frame: .zero, configuration: cfg)
        wv.navigationDelegate = context.coordinator
        wv.uiDelegate = context.coordinator
        wv.allowsBackForwardNavigationGestures = false
        wv.setValue(false, forKey: "drawsBackground")       // без білого спалаху до першого кадру
        wv.customUserAgent = nil
        context.coordinator.load(wv, radio)
        model.web = wv
        return wv
    }

    func updateNSView(_ wv: WKWebView, context: Context) {
        if context.coordinator.ip != radio.ip { context.coordinator.load(wv, radio) }
        model.web = wv
    }

    @MainActor
    final class Coordinator: NSObject, WKNavigationDelegate, WKUIDelegate, WKDownloadDelegate {
        let model: AppModel
        var ip = ""
        private var targets: [ObjectIdentifier: URL] = [:]

        init(model: AppModel) { self.model = model }

        func load(_ wv: WKWebView, _ r: Radio) {
            ip = r.ip
            if let u = URL(string: "http://\(r.ip)/") { wv.load(URLRequest(url: u, cachePolicy: .reloadIgnoringLocalCacheData)) }
        }

        // посилання назовні (не на радіо) — у звичайний браузер
        func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction, preferences: WKWebpagePreferences,
                     decisionHandler: @escaping @MainActor (WKNavigationActionPolicy, WKWebpagePreferences) -> Void) {
            if action.shouldPerformDownload { decisionHandler(.download, preferences); return }
            if let u = action.request.url, let host = u.host, host != ip, ["http", "https"].contains(u.scheme ?? ""),
               action.navigationType == .linkActivated {
                NSWorkspace.shared.open(u); decisionHandler(.cancel, preferences); return
            }
            decisionHandler(.allow, preferences)
        }

        // «attachment» або тип, який сторінкою не показати, — зберегти
        func webView(_ webView: WKWebView, decidePolicyFor response: WKNavigationResponse,
                     decisionHandler: @escaping @MainActor (WKNavigationResponsePolicy) -> Void) {
            let disp = (response.response as? HTTPURLResponse)?.value(forHTTPHeaderField: "Content-Disposition") ?? ""
            if disp.lowercased().contains("attachment") || !response.canShowMIMEType { decisionHandler(.download) } else { decisionHandler(.allow) }
        }

        func webView(_ webView: WKWebView, navigationAction: WKNavigationAction, didBecome download: WKDownload) { download.delegate = self }
        func webView(_ webView: WKWebView, navigationResponse: WKNavigationResponse, didBecome download: WKDownload) { download.delegate = self }

        func download(_ download: WKDownload, decideDestinationUsing response: URLResponse, suggestedFilename: String,
                      completionHandler: @escaping @MainActor (URL?) -> Void) {
            let dir = FileManager.default.urls(for: .downloadsDirectory, in: .userDomainMask)[0]
            var dest = dir.appendingPathComponent(suggestedFilename)
            let base = dest.deletingPathExtension().lastPathComponent, ext = dest.pathExtension
            var n = 2
            while FileManager.default.fileExists(atPath: dest.path) {
                dest = dir.appendingPathComponent(ext.isEmpty ? "\(base) \(n)" : "\(base) \(n).\(ext)"); n += 1
            }
            targets[ObjectIdentifier(download)] = dest
            completionHandler(dest)
        }

        func downloadDidFinish(_ download: WKDownload) {
            let name = targets.removeValue(forKey: ObjectIdentifier(download))?.lastPathComponent ?? "файл"
            model.show(notice: "Збережено в «Завантаження»: \(name)")
        }

        func download(_ download: WKDownload, didFailWithError error: Error, resumeData: Data?) {
            targets.removeValue(forKey: ObjectIdentifier(download))
            model.show(notice: "Файл не збережено: \(error.localizedDescription)")
        }

        // <input type=file>
        func webView(_ webView: WKWebView, runOpenPanelWith parameters: WKOpenPanelParameters, initiatedByFrame frame: WKFrameInfo,
                     completionHandler: @escaping @MainActor ([URL]?) -> Void) {
            let p = NSOpenPanel()
            p.allowsMultipleSelection = parameters.allowsMultipleSelection
            p.canChooseDirectories = false
            p.prompt = "Вибрати"
            p.begin { completionHandler($0 == .OK ? p.urls : nil) }
        }

        // window.open / target=_blank — у браузер
        func webView(_ webView: WKWebView, createWebViewWith configuration: WKWebViewConfiguration, for action: WKNavigationAction,
                     windowFeatures: WKWindowFeatures) -> WKWebView? {
            if let u = action.request.url { NSWorkspace.shared.open(u) }
            return nil
        }

        func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo,
                     completionHandler: @escaping @MainActor () -> Void) {
            let a = NSAlert(); a.messageText = message; a.addButton(withTitle: "Гаразд"); a.runModal(); completionHandler()
        }

        func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo,
                     completionHandler: @escaping @MainActor (Bool) -> Void) {
            let a = NSAlert(); a.messageText = message; a.addButton(withTitle: "Так"); a.addButton(withTitle: "Скасувати")
            completionHandler(a.runModal() == .alertFirstButtonReturn)
        }

        // сторінка не відкрилась — радіо, мабуть, зникло
        func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
            let e = error as NSError
            if e.code == NSURLErrorCancelled || e.code == 102 { return }       // 102 — перехід став завантаженням
            model.search(message: "Сторінка радіо не відкрилась. Шукаю знову…")
        }
    }
}
