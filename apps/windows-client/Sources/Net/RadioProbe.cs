using System;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace PotuzhneRadio;

/// <summary>
/// Знайдене радіо. Address — саме та адреса, за якою ми до нього достукались
/// (IP, а якщо порт не 80 — «IP:порт»): сторінку відкриваємо за нею, а не за
/// тією, яку радіо повідомляє про себе, — ці дві можуть не збігатися за NAT.
/// </summary>
public sealed record RadioInfo
{
    public required string Address { get; init; }
    /// Ім'я в мережі (potuzhne-c447d4). Порожнє, якщо радіо на старій прошивці.
    public string Host { get; init; } = "";
    /// Адреса, яку радіо назвало саме.
    public string ReportedIp { get; init; } = "";
    public string Station { get; init; } = "";
    public bool Playing { get; init; }
    public string Board { get; init; } = "";
    public string Version { get; init; } = "";
    public string Build { get; init; } = "";
    /// Стара прошивка: /api/hello ще немає, упізнали за /api/state.
    public bool Legacy { get; init; }
    /// Яким способом знайшли — для журналу.
    public string Via { get; init; } = "";

    public string BaseUrl => $"http://{Address}/";
    public string DisplayHost => Host.Length > 0 ? Host : Address;
    public string Subtitle => Host.Length > 0 ? $"{Host} · {Address}" : Address;

    public override string ToString() =>
        $"{Subtitle} «{Station}»{(Legacy ? " (стара прошивка)" : "")} [{Via}]";
}

public enum ProbeKind
{
    /// Це наше радіо.
    Radio,
    /// За адресою хтось відповідає, але це не ПОТУЖНЕ РАДІО.
    NotRadio,
    /// Ніхто не відповів: немає пристрою, закрито або не встиг.
    NoAnswer,
}

public sealed record ProbeResult(ProbeKind Kind, RadioInfo? Radio, string Detail);

/// <summary>
/// Упізнавання радіо за адресою. Лише GET: /api/hello, а для старої прошивки —
/// /api/state. Нічого іншого радіо від програми не отримує.
/// </summary>
public static class RadioProbe
{
    /// Для перебору мережі: чекати з'єднання довше за 800 мс немає сенсу,
    /// у локальній мережі радіо відповідає за 20–50 мс.
    static readonly HttpClient Fast = MakeClient(TimeSpan.FromMilliseconds(800));

    /// Для перевірки вже відомого радіо: воно може бути зайняте звуком.
    static readonly HttpClient Patient = MakeClient(TimeSpan.FromSeconds(3));

    static HttpClient MakeClient(TimeSpan connectTimeout)
    {
        var handler = new SocketsHttpHandler
        {
            // Системний проксі не має права заважати: радіо в цій же мережі.
            UseProxy = false,
            AllowAutoRedirect = false,
            UseCookies = false,
            ConnectTimeout = connectTimeout,
            PooledConnectionIdleTimeout = TimeSpan.FromSeconds(2),
            MaxConnectionsPerServer = 2,
            AutomaticDecompression = DecompressionMethods.None,
        };
        var http = new HttpClient(handler)
        {
            Timeout = Timeout.InfiniteTimeSpan, // межа — на кожен запит окремо
            MaxResponseContentBufferSize = 256 * 1024,
        };
        http.DefaultRequestHeaders.UserAgent.ParseAdd("PotuzhneRadio-Windows/1.0");
        return http;
    }

    /// <summary>
    /// Чи наше радіо за цією адресою. address — «IP», «ім'я» або «хост:порт».
    /// helloTimeout — межа на /api/hello; запасний /api/state отримує не менше 1,5 с.
    /// </summary>
    public static async Task<ProbeResult> ProbeAsync(string address, TimeSpan helloTimeout, bool fast, CancellationToken ct)
    {
        var http = fast ? Fast : Patient;

        var (code, body) = await GetAsync(http, $"http://{address}/api/hello", helloTimeout, ct);
        if (code == 200)
        {
            var info = ParseHello(body, address);
            return info != null
                ? new ProbeResult(ProbeKind.Radio, info, "hello")
                : new ProbeResult(ProbeKind.NotRadio, null, "hello: не наш JSON");
        }
        if (code == 0)
            return new ProbeResult(ProbeKind.NoAnswer, null, body);
        if (code != 404)
            return new ProbeResult(ProbeKind.NotRadio, null, $"hello: HTTP {code}");

        // /api/hello немає — можливо, радіо ще на старій прошивці.
        var stateTimeout = helloTimeout < TimeSpan.FromMilliseconds(1500) ? TimeSpan.FromMilliseconds(1500) : helloTimeout;
        (code, body) = await GetAsync(http, $"http://{address}/api/state", stateTimeout, ct);
        if (code == 200)
        {
            var info = ParseState(body, address);
            if (info != null)
                return new ProbeResult(ProbeKind.Radio, info, "state");
        }
        return code == 0
            ? new ProbeResult(ProbeKind.NoAnswer, null, "state: " + body)
            : new ProbeResult(ProbeKind.NotRadio, null, $"state: HTTP {code}");
    }

    static async Task<(int code, string body)> GetAsync(HttpClient http, string url, TimeSpan timeout, CancellationToken ct)
    {
        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        cts.CancelAfter(timeout);
        try
        {
            using var req = new HttpRequestMessage(HttpMethod.Get, url);
            // Не тримати з'єднання відкритим: у радіо небагато сокетів, а
            // сторінка й звук потребують їх більше, ніж ця перевірка.
            req.Headers.ConnectionClose = true;
            req.Headers.Accept.ParseAdd("application/json");
            using var resp = await http.SendAsync(req, HttpCompletionOption.ResponseContentRead, cts.Token);
            var bytes = await resp.Content.ReadAsByteArrayAsync(cts.Token);
            return ((int)resp.StatusCode, Encoding.UTF8.GetString(bytes));
        }
        catch (OperationCanceledException) when (!ct.IsCancellationRequested)
        {
            return (0, "немає відповіді");
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception ex)
        {
            return (0, Short(ex));
        }
    }

    static string Short(Exception ex)
    {
        while (ex.InnerException != null) ex = ex.InnerException;
        return ex is SocketException se ? $"{se.SocketErrorCode}" : ex.Message;
    }

    /// {"potuzhne":1,"board":"ES3C28P","v":"1.0","build":"…","host":"potuzhne-c447d4","ip":"…","station":"…","play":0}
    public static RadioInfo? ParseHello(string body, string address)
    {
        try
        {
            using var doc = JsonDocument.Parse(body);
            var r = doc.RootElement;
            if (r.ValueKind != JsonValueKind.Object || !r.TryGetProperty("potuzhne", out var p))
                return null;
            var ok = p.ValueKind switch
            {
                JsonValueKind.Number => p.TryGetInt32(out var n) && n == 1,
                JsonValueKind.True => true,
                JsonValueKind.String => p.GetString() == "1",
                _ => false,
            };
            if (!ok) return null;
            return new RadioInfo
            {
                Address = address,
                Host = Str(r, "host"),
                ReportedIp = Str(r, "ip"),
                Station = Str(r, "station"),
                Playing = Int(r, "play") == 1,
                Board = Str(r, "board"),
                Version = Str(r, "v"),
                Build = Str(r, "build"),
            };
        }
        catch (JsonException)
        {
            return null;
        }
    }

    /// Стара прошивка: у /api/state поле fw починається з «POTUZHNE-RADIO-FW|».
    /// Приклад: "POTUZHNE-RADIO-FW|ES3C28P|1.0|11.09.2026 17:33|"
    public static RadioInfo? ParseState(string body, string address)
    {
        try
        {
            using var doc = JsonDocument.Parse(body);
            var r = doc.RootElement;
            if (r.ValueKind != JsonValueKind.Object) return null;
            var fw = Str(r, "fw");
            if (!fw.StartsWith("POTUZHNE-RADIO-FW|", StringComparison.Ordinal)) return null;
            var parts = fw.Split('|');
            string Part(int i) => i < parts.Length ? parts[i] : "";
            return new RadioInfo
            {
                Address = address,
                ReportedIp = Str(r, "ip"),
                Station = Str(r, "name"),
                Playing = Int(r, "play") == 1,
                Board = Part(1),
                Version = Part(2).Length > 0 ? Part(2) : Str(r, "v"),
                Build = Part(3).Length > 0 ? Part(3) : Str(r, "build"),
                Legacy = true,
            };
        }
        catch (JsonException)
        {
            return null;
        }
    }

    static string Str(JsonElement o, string name) =>
        o.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() ?? "" : "";

    static int Int(JsonElement o, string name) =>
        o.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.Number && v.TryGetInt32(out var n) ? n : -1;

    // ---------------------------------------------------------------------
    //  Адреса, введена вручну
    // ---------------------------------------------------------------------

    /// <summary>
    /// Приводить введене людиною до «хост» або «хост:порт». Приймає також
    /// «http://192.168.1.32/», «potuzhne-c447d4.local» тощо. null — не адреса.
    /// </summary>
    public static (string host, int port)? ParseAddress(string input)
    {
        var s = (input ?? "").Trim();
        if (s.Length == 0) return null;
        if (!s.Contains("://", StringComparison.Ordinal)) s = "http://" + s;
        if (!Uri.TryCreate(s, UriKind.Absolute, out var uri)) return null;
        if (uri.Scheme != Uri.UriSchemeHttp && uri.Scheme != Uri.UriSchemeHttps) return null;
        var host = uri.IdnHost;
        if (string.IsNullOrWhiteSpace(host)) return null;
        // Радіо відповідає лише по http; https у введеному просто ігноруємо.
        var port = uri.IsDefaultPort ? 80 : uri.Port;
        return (host, port);
    }

    public static string Join(string host, int port)
    {
        if (IPAddress.TryParse(host, out var ip) && ip.AddressFamily == AddressFamily.InterNetworkV6 && !host.StartsWith('['))
            host = $"[{host}]";
        return port == 80 ? host : $"{host}:{port}";
    }

    /// <summary>
    /// Перевіряє адресу, введену вручну. Ім'я без крапки («potuzhne-c447d4»)
    /// пробуємо ще й як «….local» — і власним mDNS-запитом, і через систему.
    /// </summary>
    public static async Task<ProbeResult> ProbeManualAsync(string input, CancellationToken ct)
    {
        var parsed = ParseAddress(input);
        if (parsed == null)
            return new ProbeResult(ProbeKind.NoAnswer, null, "це не схоже на адресу");
        var (host, port) = parsed.Value;
        var timeout = TimeSpan.FromSeconds(3);
        var isIp = IPAddress.TryParse(host.Trim('[', ']'), out _);

        ProbeResult best = new(ProbeKind.NoAnswer, null, "немає відповіді");
        void Keep(ProbeResult r) { if (r.Kind == ProbeKind.NotRadio && best.Kind == ProbeKind.NoAnswer) best = r; }

        // Ім'я в локальній мережі («potuzhne-c447d4» чи «….local»): спершу власний
        // запит mDNS — він відповідає за частки секунди, тоді як системний
        // розпізнавач на голому імені може думати кілька секунд.
        var local = isIp ? null
                  : host.EndsWith(".local", StringComparison.OrdinalIgnoreCase) ? host
                  : host.Contains('.') ? null : host + ".local";
        if (local != null)
        {
            var ip = await Mdns.ResolveAsync(local, TimeSpan.FromSeconds(2), ct);
            if (ip != null)
            {
                var viaMdns = await ProbeAsync(Join(ip.ToString(), port), timeout, false, ct);
                if (viaMdns.Kind == ProbeKind.Radio)
                    return viaMdns with { Radio = WithHost(viaMdns.Radio!, local) };
                Keep(viaMdns);
            }
        }

        // Далі — рівно те, що ввели, через систему.
        var direct = await ProbeAsync(Join(host, port), timeout, false, ct);
        if (direct.Kind == ProbeKind.Radio) return direct;
        Keep(direct);

        // І голе ім'я як «….local» через системний розпізнавач (Windows 10+ знає .local).
        if (local != null && !host.Equals(local, StringComparison.OrdinalIgnoreCase))
        {
            var viaSystem = await ProbeAsync(Join(local, port), timeout, false, ct);
            if (viaSystem.Kind == ProbeKind.Radio) return viaSystem;
            Keep(viaSystem);
        }
        return best;
    }

    static RadioInfo WithHost(RadioInfo r, string localName) =>
        r.Host.Length > 0 ? r : r with { Host = localName.EndsWith(".local", StringComparison.OrdinalIgnoreCase) ? localName[..^6] : localName };
}
