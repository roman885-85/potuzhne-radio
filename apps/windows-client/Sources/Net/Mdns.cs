using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace PotuzhneRadio;

/// <summary>Відповідь mDNS про один екземпляр сервісу _potuzhne._tcp.</summary>
public sealed record MdnsHit(IPAddress Ip, int Port, string Instance, string TxtHost, string Board, string Version)
{
    public string Address => RadioProbe.Join(Ip.ToString(), Port);
}

/// <summary>
/// Мінімальний клієнт mDNS/DNS-SD без сторонніх бібліотек: запит PTR на
/// _potuzhne._tcp.local. і розбір відповіді (PTR, SRV, TXT, A).
///
/// Запит іде з довільного порту, а не з 5353, і з бітом QU. За RFC 6762 §6.7
/// відповідач на такий запит мусить відповісти одноадресно — просто на наш
/// порт. Так не треба займати 5353, який у Windows тримає її власна служба
/// mDNS, і не треба просити дозволу брандмауера на вхідний multicast.
/// </summary>
public static class Mdns
{
    public const string ServiceName = "_potuzhne._tcp.local";

    static readonly IPEndPoint Group = new(IPAddress.Parse("224.0.0.251"), 5353);

    const ushort TypeA = 1, TypePtr = 12, TypeTxt = 16, TypeSrv = 33;

    /// <summary>
    /// Шукає радіо протягом duration: три запити (0 с, 1 с, 2,5 с) з кожного
    /// інтерфейсу, onHit — на кожен новий екземпляр (одна адреса — один раз).
    /// </summary>
    public static async Task BrowseAsync(IReadOnlyList<LocalNet> nets, Action<MdnsHit> onHit, TimeSpan duration, CancellationToken ct)
    {
        var query = BuildQuery(ServiceName, TypePtr);
        var seen = new ConcurrentDictionary<string, byte>();
        await RunAsync(nets, query, duration, ct, (msg, from) =>
        {
            foreach (var hit in ExtractHits(msg, from))
            {
                if (!seen.TryAdd(hit.Address, 0)) continue;
                Log.Write($"mDNS: «{hit.Instance}» → {hit.Address} (host={hit.TxtHost}, board={hit.Board}, ver={hit.Version})");
                onHit(hit);
            }
            return false;
        });
    }

    /// <summary>Адреса імені «щось.local» — власним запитом A, без системного розпізнавача.</summary>
    public static async Task<IPAddress?> ResolveAsync(string name, TimeSpan timeout, CancellationToken ct)
    {
        name = name.TrimEnd('.');
        var query = BuildQuery(name, TypeA);
        IPAddress? result = null;
        await RunAsync(NetScan.Interfaces(), query, timeout, ct, (msg, from) =>
        {
            foreach (var r in Parse(msg))
                if (r.Type == TypeA && r.Address != null && r.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
                {
                    result = r.Address;
                    return true;
                }
            return false;
        });
        Log.Write(result != null ? $"mDNS: {name} → {result}" : $"mDNS: {name} не відповів");
        return result;
    }

    /// <summary>
    /// Спільна основа: по сокету на кожен інтерфейс, запит у групу 224.0.0.251:5353,
    /// приймання відповідей до кінця часу. handle повертає true — «досить».
    /// </summary>
    static async Task RunAsync(IReadOnlyList<LocalNet> nets, byte[] query, TimeSpan duration, CancellationToken ct,
        Func<byte[], IPAddress, bool> handle)
    {
        using var stop = CancellationTokenSource.CreateLinkedTokenSource(ct);
        stop.CancelAfter(duration);
        var sockets = new List<Socket>();
        try
        {
            foreach (var n in nets.GroupBy(n => n.Address.ToString()).Select(g => g.First()))
            {
                try
                {
                    var s = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
                    s.Bind(new IPEndPoint(n.Address, 0));
                    s.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastInterface, n.Address.GetAddressBytes());
                    s.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastTimeToLive, 255);
                    if (OperatingSystem.IsWindows())
                    {
                        // SIO_UDP_CONNRESET = off: інакше чужий ICMP «порт недосяжний»
                        // обриває приймання на цьому сокеті помилкою.
                        s.IOControl(unchecked((int)0x9800000C), new byte[] { 0, 0, 0, 0 }, null);
                    }
                    sockets.Add(s);
                }
                catch (Exception ex)
                {
                    Log.Write($"mDNS: інтерфейс {n.Address} пропущено — {ex.Message}");
                }
            }
            if (sockets.Count == 0) return;

            var receivers = sockets.Select(s => ReceiveLoop(s, handle, stop)).ToList();

            // Три запити: перший міг загубитися в Wi-Fi, а радіо могло бути зайняте.
            foreach (var delay in new[] { 0, 1000, 1500 })
            {
                if (delay > 0)
                {
                    try { await Task.Delay(delay, stop.Token); }
                    catch (OperationCanceledException) { break; }
                }
                foreach (var s in sockets)
                {
                    try { await s.SendToAsync(query, SocketFlags.None, Group, stop.Token); }
                    catch (OperationCanceledException) { break; }
                    catch (Exception ex) { Log.Write($"mDNS: запит з {s.LocalEndPoint} не пішов — {ex.Message}"); }
                }
            }
            await Task.WhenAll(receivers);
        }
        finally
        {
            foreach (var s in sockets) s.Dispose();
        }
        ct.ThrowIfCancellationRequested();
    }

    static async Task ReceiveLoop(Socket s, Func<byte[], IPAddress, bool> handle, CancellationTokenSource stop)
    {
        var buf = new byte[9000];
        while (!stop.IsCancellationRequested)
        {
            SocketReceiveFromResult res;
            try
            {
                res = await s.ReceiveFromAsync(buf, SocketFlags.None, new IPEndPoint(IPAddress.Any, 0), stop.Token);
            }
            catch (OperationCanceledException) { return; }
            catch (ObjectDisposedException) { return; }
            catch (SocketException) { continue; }
            if (res.ReceivedBytes <= 0) continue;
            var msg = buf.AsSpan(0, res.ReceivedBytes).ToArray();
            var from = ((IPEndPoint)res.RemoteEndPoint).Address;
            try
            {
                if (handle(msg, from)) { stop.Cancel(); return; }
            }
            catch (Exception ex)
            {
                Log.Write($"mDNS: не розібрав відповідь від {from} — {ex.Message}");
            }
        }
    }

    // ---------------------------------------------------------------------
    //  Повідомлення DNS
    // ---------------------------------------------------------------------

    /// Запит з одним питанням; клас 0x8001 = IN з бітом QU («відповідай мені напряму»).
    public static byte[] BuildQuery(string name, ushort qtype)
    {
        var b = new List<byte>(64) { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 };
        foreach (var label in name.TrimEnd('.').Split('.'))
        {
            var bytes = Encoding.UTF8.GetBytes(label);
            b.Add((byte)bytes.Length);
            b.AddRange(bytes);
        }
        b.Add(0);
        b.Add((byte)(qtype >> 8)); b.Add((byte)qtype);
        b.Add(0x80); b.Add(0x01);
        return b.ToArray();
    }

    public sealed class Record
    {
        public string Name = "";
        public ushort Type;
        public string? Target;                       // PTR, SRV
        public int Port;                             // SRV
        public IPAddress? Address;                   // A
        public Dictionary<string, string>? Txt;      // TXT
    }

    /// Усі записи відповіді (answers + authority + additional). Не відповідь — порожньо.
    public static List<Record> Parse(byte[] m)
    {
        var list = new List<Record>();
        if (m.Length < 12) return list;
        if ((m[2] & 0x80) == 0) return list; // це запит, а не відповідь
        int qd = U16(m, 4), an = U16(m, 6), ns = U16(m, 8), ar = U16(m, 10);
        var pos = 12;
        for (var i = 0; i < qd; i++)
        {
            ReadName(m, ref pos);
            pos += 4;
        }
        var total = an + ns + ar;
        for (var i = 0; i < total; i++)
        {
            if (pos >= m.Length) break;
            var name = ReadName(m, ref pos);
            if (pos + 10 > m.Length) break;
            var type = (ushort)U16(m, pos);
            var len = U16(m, pos + 8);
            pos += 10;
            var start = pos;
            if (start + len > m.Length) break;
            var r = new Record { Name = name, Type = type };
            switch (type)
            {
                case TypePtr:
                {
                    var p = start;
                    r.Target = ReadName(m, ref p);
                    break;
                }
                case TypeSrv when len >= 7:
                {
                    r.Port = U16(m, start + 4);
                    var p = start + 6;
                    r.Target = ReadName(m, ref p);
                    break;
                }
                case TypeA when len == 4:
                    r.Address = new IPAddress(m.AsSpan(start, 4));
                    break;
                case TypeTxt:
                {
                    r.Txt = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                    var p = start;
                    while (p < start + len)
                    {
                        int l = m[p++];
                        if (p + l > start + len) break;
                        var kv = Encoding.UTF8.GetString(m, p, l);
                        p += l;
                        var eq = kv.IndexOf('=');
                        if (eq > 0) r.Txt[kv[..eq]] = kv[(eq + 1)..];
                        else if (kv.Length > 0) r.Txt[kv] = "";
                    }
                    break;
                }
            }
            list.Add(r);
            pos = start + len;
        }
        return list;
    }

    /// <summary>
    /// Екземпляри _potuzhne._tcp у відповіді. Адреса — із запису A для цілі SRV,
    /// а якщо його в пакеті немає — адреса відправника: це саме радіо.
    /// </summary>
    public static IEnumerable<MdnsHit> ExtractHits(byte[] msg, IPAddress from)
    {
        var recs = Parse(msg);
        if (recs.Count == 0) yield break;

        var suffix = "." + ServiceName;
        var instances = new List<string>();
        foreach (var r in recs)
        {
            if (r.Type == TypePtr && r.Target != null && Same(r.Name, ServiceName)) instances.Add(r.Target);
            else if ((r.Type is TypeSrv or TypeTxt) && r.Name.EndsWith(suffix, StringComparison.OrdinalIgnoreCase)) instances.Add(r.Name);
        }

        foreach (var inst in instances.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            var srv = recs.FirstOrDefault(r => r.Type == TypeSrv && Same(r.Name, inst));
            var txt = recs.FirstOrDefault(r => r.Type == TypeTxt && Same(r.Name, inst))?.Txt;
            IPAddress? ip = null;
            if (srv?.Target != null)
            {
                var addrs = recs.Where(r => r.Type == TypeA && r.Address != null && Same(r.Name, srv.Target))
                                .Select(r => r.Address!).ToList();
                ip = addrs.FirstOrDefault(a => a.Equals(from)) ?? addrs.FirstOrDefault();
            }
            ip ??= from;
            var port = srv?.Port is > 0 ? srv.Port : 80;
            var label = inst.EndsWith(suffix, StringComparison.OrdinalIgnoreCase) ? inst[..^suffix.Length] : inst;
            string T(string k) => txt != null && txt.TryGetValue(k, out var v) ? v : "";
            yield return new MdnsHit(ip, port, label, T("host"), T("board"), T("ver"));
        }
    }

    static bool Same(string a, string b) =>
        string.Equals(a.TrimEnd('.'), b.TrimEnd('.'), StringComparison.OrdinalIgnoreCase);

    static int U16(byte[] m, int p) => (m[p] << 8) | m[p + 1];

    /// Ім'я DNS зі стисненням (RFC 1035 §4.1.4); захист від петель посилань.
    static string ReadName(byte[] m, ref int pos)
    {
        var labels = new List<string>();
        var p = pos;
        var jumped = false;
        var hops = 0;
        while (true)
        {
            if (p >= m.Length) throw new FormatException("ім'я виходить за кінець пакета");
            int len = m[p];
            if (len == 0) { p++; break; }
            if ((len & 0xC0) == 0xC0)
            {
                if (p + 1 >= m.Length) throw new FormatException("обрізане посилання");
                var target = ((len & 0x3F) << 8) | m[p + 1];
                if (!jumped) pos = p + 2;
                jumped = true;
                if (++hops > 32) throw new FormatException("петля посилань");
                p = target;
                continue;
            }
            if ((len & 0xC0) != 0) throw new FormatException("невідомий тип мітки");
            p++;
            if (p + len > m.Length) throw new FormatException("мітка за кінцем пакета");
            labels.Add(Encoding.UTF8.GetString(m, p, len));
            p += len;
        }
        if (!jumped) pos = p;
        return string.Join('.', labels);
    }
}
