using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;

namespace PotuzhneRadio;

/// <summary>Що повідомляє пошук, поки йде.</summary>
public abstract record DiscoveryEvent;
public sealed record RadioFoundEvent(RadioInfo Radio) : DiscoveryEvent;
public sealed record ScanProgressEvent(int Done, int Total, string Ranges) : DiscoveryEvent;

public sealed class DiscoveryOptions
{
    public bool UseMdns = true;
    public bool UseScan = true;
    public TimeSpan MdnsDuration = TimeSpan.FromSeconds(3.5);
}

/// <summary>
/// Пошук радіо двома способами одночасно: mDNS (_potuzhne._tcp) і перебір
/// своєї /24. Кожну знайдену адресу підтверджує /api/hello (або /api/state
/// для старої прошивки) — радіо вважається знайденим лише після цього.
/// </summary>
public static class Discovery
{
    public static async Task<List<RadioInfo>> RunAsync(DiscoveryOptions opt, Action<DiscoveryEvent> report, CancellationToken ct)
    {
        var started = DateTime.UtcNow;
        var all = NetScan.Interfaces();
        var scanNets = NetScan.ScanTargets(all);
        Log.Write($"Пошук: інтерфейсів {all.Count}; " +
                  (all.Count == 0 ? "мережі немає" : string.Join("; ", all.Select(n => n.ToString()))));
        Log.Write($"Пошук: перебираю {string.Join(", ", scanNets.Select(n => n.Range24))}" +
                  $"{(opt.UseScan ? "" : " — вимкнено")}; mDNS {(opt.UseMdns ? "увімкнено" : "вимкнено")}");

        var found = new ConcurrentDictionary<string, RadioInfo>();
        // Що вже перевіряє mDNS: перебір ці адреси пропускає. Навпаки — ні:
        // перебір чекає лише 800 мс, і відповідь mDNS заслуговує на свою спробу.
        var mdnsPending = new ConcurrentDictionary<string, Lazy<Task>>();

        async Task Confirm(string address, TimeSpan timeout, bool fast, string via, MdnsHit? hit, CancellationToken token)
        {
            ProbeResult r;
            try { r = await RadioProbe.ProbeAsync(address, timeout, fast, token); }
            catch (OperationCanceledException) { return; }
            if (r.Kind != ProbeKind.Radio || r.Radio == null)
            {
                if (hit != null) Log.Write($"mDNS назвав {address}, але перевірка не пройшла: {r.Detail}");
                return;
            }
            var radio = r.Radio with
            {
                Via = via,
                Host = r.Radio.Host.Length > 0 ? r.Radio.Host : hit?.TxtHost ?? "",
            };
            if (found.TryAdd(address, radio))
            {
                Log.Write($"Знайдено ({via}, {r.Detail}): {radio}");
                report(new RadioFoundEvent(radio));
            }
        }

        // Відповідь mDNS — майже напевно наше радіо, тож чекаємо довше, ніж при
        // переборі, і пробуємо двічі: перший запит процесу на повільному
        // комп'ютері буває довгим, а зайняте звуком радіо — неквапним.
        async Task ConfirmMdns(string address, MdnsHit hit)
        {
            await Confirm(address, TimeSpan.FromSeconds(4), false, "mDNS", null, ct);
            if (!found.ContainsKey(address) && !ct.IsCancellationRequested)
                await Confirm(address, TimeSpan.FromSeconds(4), false, "mDNS", hit, ct);
        }

        var jobs = new List<Task>();

        if (opt.UseMdns && all.Count > 0)
        {
            jobs.Add(Task.Run(async () =>
            {
                var confirms = new ConcurrentBag<Task>();
                try
                {
                    await Mdns.BrowseAsync(all, hit =>
                    {
                        var lazy = mdnsPending.GetOrAdd(hit.Address,
                            a => new Lazy<Task>(() => ConfirmMdns(a, hit)));
                        confirms.Add(lazy.Value);
                    }, opt.MdnsDuration, ct);
                }
                catch (OperationCanceledException) { }
                catch (Exception ex) { Log.Write($"mDNS: збій — {ex.Message}"); }
                await Task.WhenAll(confirms);
            }, ct));
        }

        if (opt.UseScan && scanNets.Count > 0)
        {
            var ranges = string.Join(", ", scanNets.Select(n => n.Range24));
            var lastReport = 0;
            jobs.Add(Task.Run(async () =>
            {
                try
                {
                    await NetScan.ScanAsync(scanNets,
                        ip => found.ContainsKey(ip.ToString()) || mdnsPending.ContainsKey(ip.ToString()),
                        (ip, token) => Confirm(ip.ToString(), NetScan.ProbeTimeout, true, "перебір мережі", null, token),
                        (done, total) =>
                        {
                            // Не засипати вікно подіями: раз на 8 адрес і наприкінці.
                            var prev = Volatile.Read(ref lastReport);
                            if (done == total || done - prev >= 8)
                            {
                                Volatile.Write(ref lastReport, done);
                                report(new ScanProgressEvent(done, total, ranges));
                            }
                        }, ct);
                }
                catch (OperationCanceledException) { }
            }, ct));
        }

        try { await Task.WhenAll(jobs); }
        catch (OperationCanceledException) { }
        ct.ThrowIfCancellationRequested();

        var list = found.Values.OrderBy(r => SortKey(r.Address)).ToList();
        Log.Write($"Пошук завершено за {(DateTime.UtcNow - started).TotalSeconds:0.0} с: знайдено {list.Count}");
        return list;
    }

    static string SortKey(string address)
    {
        var host = address.Split(':')[0];
        return IPAddress.TryParse(host, out var ip)
            ? string.Concat(ip.GetAddressBytes().Select(b => b.ToString("D3")))
            : address;
    }
}
