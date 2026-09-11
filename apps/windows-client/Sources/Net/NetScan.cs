using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace PotuzhneRadio;

/// <summary>Одна IPv4-адреса цього комп'ютера разом з її мережею.</summary>
public sealed record LocalNet(IPAddress Address, IPAddress Mask, string Name, string Description, bool Virtual, bool HasGateway)
{
    /// «192.168.1.x» — для рядка стану.
    public string Range24
    {
        get
        {
            var b = Address.GetAddressBytes();
            return $"{b[0]}.{b[1]}.{b[2]}.x";
        }
    }

    public override string ToString() =>
        $"{Address}/{MaskBits(Mask)} «{Description}»{(Virtual ? " віртуальний" : "")}{(HasGateway ? " зі шлюзом" : "")}";

    static int MaskBits(IPAddress m) =>
        m.GetAddressBytes().Sum(b => System.Numerics.BitOperations.PopCount(b));
}

/// <summary>
/// Перебір своєї підмережі /24: кожній адресі — GET /api/hello, до 48 запитів
/// одночасно. Це запасний шлях на випадок, коли mDNS не проходить (буває, що
/// роутер глушить multicast у Wi-Fi).
/// </summary>
public static class NetScan
{
    public const int Parallelism = 48;
    public static readonly TimeSpan ProbeTimeout = TimeSpan.FromMilliseconds(800);

    /// Назви адаптерів, за якими видно віртуальні мережі: Hyper-V, WSL,
    /// VirtualBox, VMware, VPN-тунелі. Радіо там бути не може.
    static readonly string[] VirtualMarks =
    {
        "Hyper-V", "vEthernet", "VirtualBox", "VMware", "Virtual", "Host-Only", "Parallels",
        "WSL", "TAP-", "Wintun", "WireGuard", "ZeroTier", "Tailscale", "Npcap", "Loopback",
        "Docker", "Bluetooth",
    };

    /// <summary>
    /// Усі IPv4-адреси інтерфейсів у стані Up, крім loopback, тунелів і
    /// самопризначених 169.254.x.x.
    /// </summary>
    public static List<LocalNet> Interfaces()
    {
        var list = new List<LocalNet>();
        NetworkInterface[] all;
        try { all = NetworkInterface.GetAllNetworkInterfaces(); }
        catch (Exception ex) { Log.Write($"Не вдалося прочитати мережеві інтерфейси: {ex.Message}"); return list; }

        foreach (var ni in all)
        {
            try
            {
                if (ni.OperationalStatus != OperationalStatus.Up) continue;
                if (ni.NetworkInterfaceType is NetworkInterfaceType.Loopback or NetworkInterfaceType.Tunnel) continue;
                var props = ni.GetIPProperties();
                var hasGw = props.GatewayAddresses.Any(g =>
                    g.Address.AddressFamily == AddressFamily.InterNetwork && !g.Address.Equals(IPAddress.Any));
                var text = $"{ni.Name} {ni.Description}";
                var isVirtual = VirtualMarks.Any(m => text.Contains(m, StringComparison.OrdinalIgnoreCase));
                foreach (var ua in props.UnicastAddresses)
                {
                    if (ua.Address.AddressFamily != AddressFamily.InterNetwork) continue;
                    var b = ua.Address.GetAddressBytes();
                    if (b[0] == 127 || (b[0] == 169 && b[1] == 254)) continue;
                    IPAddress mask;
                    try { mask = ua.IPv4Mask ?? IPAddress.Parse("255.255.255.0"); }
                    catch (NotImplementedException) { mask = IPAddress.Parse("255.255.255.0"); }
                    list.Add(new LocalNet(ua.Address, mask, ni.Name, ni.Description, isVirtual, hasGw));
                }
            }
            catch (Exception ex)
            {
                Log.Write($"Інтерфейс {ni.Name}: {ex.Message}");
            }
        }
        // Спершу справжні мережі зі шлюзом — там радіо найімовірніше.
        return list.OrderBy(n => n.Virtual).ThenByDescending(n => n.HasGateway).ToList();
    }

    /// <summary>
    /// Які мережі перебирати. Віртуальні адаптери відкидаємо — але лише якщо є
    /// хоч одна справжня мережа. Інакше (скажімо, Windows сама у віртуальній
    /// машині, де її єдиний адаптер зветься «Parallels…» чи «Hyper-V…») не
    /// лишилося б нічого, і пошук мовчки нічого б не знаходив.
    /// </summary>
    public static List<LocalNet> ScanTargets(List<LocalNet> all)
    {
        var real = all.Where(n => !n.Virtual).ToList();
        var chosen = real.Count > 0 ? real : all;
        // Дві адреси в одній /24 перебирати двічі немає сенсу.
        return chosen.GroupBy(n => n.Range24).Select(g => g.First()).ToList();
    }

    /// a.b.c.1 … a.b.c.254, крім власної адреси.
    public static IEnumerable<IPAddress> Hosts24(LocalNet n)
    {
        var b = n.Address.GetAddressBytes();
        for (var i = 1; i <= 254; i++)
        {
            if (i == b[3]) continue;
            yield return new IPAddress(new[] { b[0], b[1], b[2], (byte)i });
        }
    }

    /// <summary>
    /// Перебирає всі адреси мереж nets, для кожної викликає probe (якщо skip не
    /// каже її пропустити). progress отримує (перевірено, усього).
    /// </summary>
    public static async Task ScanAsync(
        IReadOnlyList<LocalNet> nets,
        Func<IPAddress, bool> skip,
        Func<IPAddress, CancellationToken, Task> probe,
        Action<int, int>? progress,
        CancellationToken ct)
    {
        var targets = nets.SelectMany(Hosts24).Distinct().ToList();
        if (targets.Count == 0) return;
        var done = 0;
        await Parallel.ForEachAsync(targets,
            new ParallelOptions { MaxDegreeOfParallelism = Parallelism, CancellationToken = ct },
            async (ip, token) =>
            {
                if (!skip(ip))
                    await probe(ip, token);
                progress?.Invoke(Interlocked.Increment(ref done), targets.Count);
            });
    }
}
