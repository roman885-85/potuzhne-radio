using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace PotuzhneRadio;

/// <summary>
/// Оновлення радіо по Wi-Fi — ті самі запити, що робить його сторінка
/// «Оновлення» і Mac-версія програми:
///   прошивка: POST /update, multipart: updatetarget=fw (обов'язково ПЕРЕД
///             файлом — радіо читає його на першому шматку), файл у полі update;
///   сторінка: POST /webboard, обидва .gz у полі www.
/// </summary>
public static class Uploader
{
    static readonly HttpClient Http = new(new SocketsHttpHandler
    {
        UseProxy = false,
        AllowAutoRedirect = false,
        ConnectTimeout = TimeSpan.FromSeconds(8),
    })
    {
        Timeout = TimeSpan.FromSeconds(180),
    };

    public static MultipartFormDataContent FirmwareForm(byte[] bin)
    {
        var form = new MultipartFormDataContent(NewBoundary());
        Part(form, new ByteArrayContent(Encoding.ASCII.GetBytes("fw")), "updatetarget", null);
        Part(form, new ByteArrayContent(bin), "update", "PotuzhneRadio-ES3C28P-update.bin");
        return form;
    }

    public static MultipartFormDataContent WebForm(IEnumerable<string> files)
    {
        var form = new MultipartFormDataContent(NewBoundary());
        foreach (var path in files)
            Part(form, new ByteArrayContent(File.ReadAllBytes(path)), "www", Path.GetFileName(path));
        return form;
    }

    static string NewBoundary() => "potuzhne-" + Guid.NewGuid().ToString("N");

    /// <summary>
    /// Заголовок частини пишемо самі, рівно як браузер: name="…"; filename="…".
    /// Розбір форми у вбудованому сервері радіо вважає лапки обов'язковими
    /// (зрізає по одному знаку з кожного боку), а .NET сам лапок не ставить і
    /// додає ще filename*=… — радіо прочитало б ім'я поля як «pdat».
    /// </summary>
    static void Part(MultipartFormDataContent form, HttpContent c, string name, string? file)
    {
        c.Headers.ContentDisposition = ContentDispositionHeaderValue.Parse(
            file == null ? $"form-data; name=\"{name}\"" : $"form-data; name=\"{name}\"; filename=\"{file}\"");
        if (file != null) c.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
        form.Add(c);
    }

    /// <summary>Надсилає форму; progress — (надіслано, усього). Повертає (код, текст відповіді).</summary>
    public static async Task<(int code, string body)> PostAsync(string url, MultipartFormDataContent form,
        Action<long, long>? progress, CancellationToken ct)
    {
        using var content = new ProgressContent(form, progress);
        using var req = new HttpRequestMessage(HttpMethod.Post, url) { Content = content };
        req.Headers.ConnectionClose = true;
        using var resp = await Http.SendAsync(req, ct);
        var body = await resp.Content.ReadAsStringAsync(ct);
        return ((int)resp.StatusCode, body);
    }

    /// <summary>
    /// Обгортка, що рахує надіслане. Форму спершу складаємо в пам'ять (прошивка —
    /// близько 2 МБ): так відома довжина і запит іде з Content-Length, а не
    /// шматками — вбудований сервер радіо шматкового передавання не приймає.
    /// </summary>
    sealed class ProgressContent : HttpContent
    {
        readonly byte[] _body;
        readonly Action<long, long>? _progress;

        public ProgressContent(HttpContent inner, Action<long, long>? progress)
        {
            _body = inner.ReadAsByteArrayAsync().GetAwaiter().GetResult();
            _progress = progress;
            foreach (var h in inner.Headers)
                Headers.TryAddWithoutValidation(h.Key, h.Value);
            Headers.ContentLength = _body.LongLength;
        }

        protected override async Task SerializeToStreamAsync(Stream stream, TransportContext? context)
        {
            const int chunk = 16 * 1024;
            for (var off = 0; off < _body.Length; off += chunk)
            {
                var n = Math.Min(chunk, _body.Length - off);
                await stream.WriteAsync(_body.AsMemory(off, n));
                _progress?.Invoke(off + n, _body.LongLength);
            }
        }

        protected override bool TryComputeLength(out long length)
        {
            length = _body.LongLength;
            return true;
        }
    }

    /// <summary>
    /// Після прошивки радіо перезавантажується: чекаємо до 90 с, поки знову
    /// відповість на /api/hello. status — рядок для вікна.
    /// </summary>
    public static async Task<RadioInfo?> WaitForRebootAsync(string address, Action<string> status, CancellationToken ct)
    {
        await Task.Delay(4000, ct);
        for (var i = 0; i < 43; i++)
        {
            var r = await RadioProbe.ProbeAsync(address, TimeSpan.FromSeconds(2), false, ct);
            if (r.Kind == ProbeKind.Radio && r.Radio != null) return r.Radio;
            status($"Радіо перезавантажується… {i * 2 + 6} с");
            await Task.Delay(2000, ct);
        }
        return null;
    }
}
