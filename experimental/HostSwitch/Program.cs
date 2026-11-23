using System.Diagnostics;
using System.IO.Pipes;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace GlideMouseSwitch;

internal sealed record Reply(bool Ok, string Message, bool Connected = false, bool DriverReady = false, bool Retryable = false, string ReleaseToken = "", bool WaitingForInput = false, bool PeerUnavailable = false);
internal sealed record Settings(string Address, string Backend, string Peer = "", string PeerPin = "", int Port = 58426);

internal static class Program
{
    static readonly string Folder = OperatingSystem.IsMacOS()
        ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Library", "Application Support", "GlideMouseSwitch")
        : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "GlideMouseSwitch");
    static readonly string Pipe = "GlideMouse.Switch." + Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(Folder)))[..20];
    static readonly JsonSerializerOptions Json = new() { PropertyNameCaseInsensitive = true, WriteIndented = false };
    static string ReleaseLease => Path.Combine(Folder,"released");
    static string ReleaseTokenFile => Path.Combine(Folder,"release-token");
    static string PendingMacBond => Path.Combine(Folder,"pending-mac-bond");
    static bool HasPendingMacBond => File.Exists(PendingMacBond)&&File.ReadAllText(PendingMacBond)==Config.Address&&!File.Exists(ReleaseLease);
    static string PeerReleaseToken = "";
    static bool MacFreshBond;
    static long OwnershipGeneration;
    static volatile bool ShuttingDown;
    static CancellationTokenSource? MacObserverStop;
    static readonly AsyncLocal<CancellationToken> BackendCancellation = new();
    static void CancelObserver(CancellationTokenSource? observer) { try { observer?.Cancel(); } catch(ObjectDisposedException) { } }
    static readonly SemaphoreSlim Action = new(1, 1);
    static readonly object JournalLock = new();
    static void Log(string message) { lock(JournalLock) File.AppendAllText(Path.Combine(Folder,"handoff.log"),DateTimeOffset.Now.ToString("O")+" "+message+"\n"); }
    static Settings Config = null!;
    static X509Certificate2 Identity = null!;
    static string Pin(X509Certificate certificate) => Convert.ToHexString(SHA256.HashData(certificate.GetRawCertData()));
    static bool ValidPin(string pin) => Regex.IsMatch(pin, "\\A[0-9A-F]{64}\\z");
    static bool Trusted(X509Certificate? certificate) => certificate is not null && ValidPin(Config.PeerPin)
        && CryptographicOperations.FixedTimeEquals(Convert.FromHexString(Config.PeerPin), SHA256.HashData(certificate.GetRawCertData()));
    static void Save() => File.WriteAllText(Path.Combine(Folder, "settings.json"), JsonSerializer.Serialize(Config, Json));
    static void Protect(string path)
    {
        if (!OperatingSystem.IsWindows()) File.SetUnixFileMode(path, Directory.Exists(path)
            ? UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute
            : UnixFileMode.UserRead | UnixFileMode.UserWrite);
    }

    static async Task<int> Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && args[0] == "init")
            {
                string address = Regex.Replace(args[1], "[:-]", "").ToUpperInvariant();
                if (!Regex.IsMatch(address, "\\A[0-9A-F]{12}\\z") || address == "000000000000") throw new Exception("Invalid mouse address");
                string backend = Path.GetFullPath(args[2]);
                if (!File.Exists(backend)) throw new Exception("Bluetooth backend is missing");
                Directory.CreateDirectory(Folder); Protect(Folder);
                string key = Path.Combine(Folder, "identity.pfx");
                if (File.Exists(Path.Combine(Folder, "settings.json")) || File.Exists(key)) throw new Exception("Already configured; refusing to overwrite the existing identity");
                Config = new(address, backend); Save(); Protect(Path.Combine(Folder, "settings.json"));
                using var rsa = RSA.Create(3072);
                var request = new CertificateRequest("CN=GlideMouse Host Switch", rsa, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
                request.CertificateExtensions.Add(new X509KeyUsageExtension(X509KeyUsageFlags.DigitalSignature, true));
                using var cert = request.CreateSelfSigned(DateTimeOffset.UtcNow.AddMinutes(-5), DateTimeOffset.UtcNow.AddYears(2));
                File.WriteAllBytes(key, cert.Export(X509ContentType.Pfx)); Protect(key);
                Console.WriteLine(JsonSerializer.Serialize(new { pin = Pin(cert), folder = Folder })); return 0;
            }
            Config = JsonSerializer.Deserialize<Settings>(File.ReadAllText(Path.Combine(Folder, "settings.json")), Json) ?? throw new Exception("Missing configuration");
            if (args.Length == 3 && args[0] == "trust")
            {
                string pin = args[2].ToUpperInvariant();
                if (Uri.CheckHostName(args[1]) == UriHostNameType.Unknown || !ValidPin(pin)) throw new Exception("Invalid peer name or certificate fingerprint");
                Config = Config with { Peer = args[1], PeerPin = pin }; Save(); Console.WriteLine("Peer certificate pinned"); return 0;
            }
            if (args.Length != 1 || !new[] { "identity", "serve", "take", "release", "status", "probe", "shutdown-release" }.Contains(args[0])) throw new Exception("Unsupported local command");
            Identity = X509CertificateLoader.LoadPkcs12FromFile(Path.Combine(Folder, "identity.pfx"), null);
            if(args[0]=="identity") {
                Console.WriteLine(JsonSerializer.Serialize(new { pin=Pin(Identity), address=Config.Address },Json));return 0;
            }
            if (args[0] == "probe")
            {
                var reply = await PeerRequest("status-v1") ?? new Reply(false, "Peer is offline or unreachable");
                Console.WriteLine(JsonSerializer.Serialize(reply, Json));return reply.Ok ? 0 : 1;
            }
            if (args[0] != "serve")
            {
                using var pipe = new NamedPipeClientStream(".", Pipe, PipeDirection.InOut, PipeOptions.Asynchronous);
                await pipe.ConnectAsync(3000);
                await Write(pipe, args[0], CancellationToken.None);
                var response = await Read(pipe, CancellationToken.None); Console.WriteLine(response);
                return JsonSerializer.Deserialize<Reply>(response, Json)?.Ok == true ? 0 : 1;
            }
            using var mutex = new Mutex(true, Pipe + ".Owner", out bool owns);
            if (!owns) throw new Exception("Host Switch is already running");
            WindowsHotkey.Start(async () => { await Execute("take"); }, Log, async()=>{await PrepareShutdown();});
            await Task.WhenAll(LocalServer(), PeerServer(), AutoTakeMonitor()); return 0;
        }
        catch (Exception e) { Console.WriteLine(JsonSerializer.Serialize(new Reply(false, e.Message), Json)); return 1; }
    }

    static async Task<string> Read(Stream stream, CancellationToken cancel)
    {
        var bytes = new List<byte>(); byte[] one = new byte[1];
        while (bytes.Count < 2048)
        {
            if (await stream.ReadAsync(one, cancel) == 0) throw new IOException("Connection ended before the reply");
            if (one[0] == 10) return Encoding.UTF8.GetString(bytes.ToArray());
            bytes.Add(one[0]);
        }
        throw new IOException("Message too long");
    }
    static async Task Write(Stream stream, string message, CancellationToken cancel) =>
        await stream.WriteAsync(Encoding.UTF8.GetBytes(message + "\n"), cancel);
    static async Task LocalServer()
    {
        while (true)
        {
            using var pipe = new NamedPipeServerStream(Pipe, PipeDirection.InOut, 1, PipeTransmissionMode.Byte, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
            await pipe.WaitForConnectionAsync();
            try
            {
                using var limit = new CancellationTokenSource(TimeSpan.FromSeconds(240));
                string command = await Read(pipe, limit.Token);
                Reply reply = command == "status" ? await Backend("status") : command=="shutdown-release" ? await PrepareShutdown() : await Execute(command);
                await Write(pipe, JsonSerializer.Serialize(reply, Json), limit.Token);
            }
            catch (Exception e) { try { await Write(pipe, JsonSerializer.Serialize(new Reply(false, e.Message), Json), CancellationToken.None); } catch (IOException) { } }
        }
    }
    static async Task PeerServer()
    {
        var listener = new TcpListener(IPAddress.Any, Config.Port); listener.Start(4);
        while (true)
        {
            using var socket = await listener.AcceptTcpClientAsync();
            try
            {
                using var limit = new CancellationTokenSource(TimeSpan.FromSeconds(90));
                using var tls = new SslStream(socket.GetStream(), false, (_, cert, _, _) => Trusted(cert));
                await tls.AuthenticateAsServerAsync(new SslServerAuthenticationOptions {
                    ServerCertificate = Identity, ClientCertificateRequired = true,
                    EnabledSslProtocols = SslProtocols.Tls12 | SslProtocols.Tls13,
                    CertificateRevocationCheckMode = X509RevocationMode.NoCheck
                }, limit.Token);
                // Only fixed actions on the configured mouse are accepted.
                // A trusted shutdown hint may schedule its local claim, but can
                // never select an address, executable or driver setting.
                string command = await Read(tls, limit.Token);
                if(command=="release-v1")Log("Authenticated peer requested release");
                Reply reply;
                if(command=="source-shutdown-v1") {
                    Log("Authenticated peer announced completed shutdown release");
                    reply=new(true,"Shutdown handoff received");
                    _=Task.Run(async()=>{
                        try {
                            await Task.Delay(250);if(ShuttingDown)return;
                            var local=await Backend("status");
                            if(local.Ok&&!local.Connected&&!local.WaitingForInput)await Execute("take");
                        } catch(Exception e){Log("Shutdown takeover: "+e.GetType().Name);}
                    });
                } else reply = command == "release-v1" ? await Execute("release") : command == "status-v1" ? await Backend("status") : command.StartsWith("restore-v1 ",StringComparison.Ordinal) ? await Execute("restore:"+command[11..]) : new(false, "Unsupported peer request");
                await Write(tls, JsonSerializer.Serialize(reply, Json), limit.Token);
            }
            catch (Exception e) when (e is IOException or AuthenticationException or OperationCanceledException or SocketException) { }
        }
    }
    static async Task<Reply?> ReleasePeer() { var source=AutoTakePolicy.ReleaseResult(await PeerRequest("release-v1"));PeerReleaseToken=source is {Ok:true}?source.ReleaseToken:"";if(source is {PeerUnavailable:true})Log("Peer unavailable; standalone claim without SSH or remote release");return source; }
    static async Task<Reply?> PeerRequest(string command, int connectMilliseconds=3000, int requestMilliseconds=95000)
    {
        if (string.IsNullOrEmpty(Config.Peer) || !ValidPin(Config.PeerPin)) throw new Exception("Configure and verify the peer certificate before switching");
        using var socket = new TcpClient();
        using var connect = new CancellationTokenSource(connectMilliseconds);
        try { await socket.ConnectAsync(Config.Peer, Config.Port, connect.Token); }
        catch(SocketException e) {
            Log($"Peer TCP connection failed: {e.SocketErrorCode}, native {e.NativeErrorCode}");
            bool unavailable=e.SocketErrorCode is SocketError.ConnectionRefused or SocketError.HostUnreachable or SocketError.NetworkUnreachable or SocketError.TimedOut or SocketError.HostDown;
            return new(false,"Cannot reach the other computer's GlideMouse Switch helper",PeerUnavailable:unavailable);
        }
        catch(OperationCanceledException) { Log("Peer TCP connection timed out");return new(false,"The other computer's GlideMouse Switch helper did not respond",PeerUnavailable:true); }
        using var limit = new CancellationTokenSource(requestMilliseconds);
        using var tls = new SslStream(socket.GetStream(), false, (_, cert, _, _) => Trusted(cert));
        await tls.AuthenticateAsClientAsync(new SslClientAuthenticationOptions {
            TargetHost = "glidemouse-switch.local", ClientCertificates = new X509CertificateCollection { Identity },
            EnabledSslProtocols = SslProtocols.Tls12 | SslProtocols.Tls13,
            CertificateRevocationCheckMode = X509RevocationMode.NoCheck
        }, limit.Token);
        await Write(tls, command, limit.Token);
        return JsonSerializer.Deserialize<Reply>(await Read(tls, limit.Token), Json) ?? throw new IOException("Invalid peer response");
    }
    static bool LocalActivityRecent()
    {
        if(OperatingSystem.IsWindows())return WindowsHotkey.LocalActivityRecent();
        string guard=Path.Combine(Folder,"guard-ready.json");
        if(!File.Exists(guard)||DateTime.UtcNow-File.GetLastWriteTimeUtc(guard)>TimeSpan.FromSeconds(12))return false;
        using var state=JsonDocument.Parse(File.ReadAllText(guard));
        return state.RootElement.TryGetProperty("localInputIdleSeconds",out var age)&&age.TryGetDouble(out double seconds)&&seconds>=0&&seconds<4;
    }
    static async Task AutoTakeMonitor()
    {
        var policy=new AutoTakePolicy();
        var context=new AutoTakeContext(DateTime.UtcNow);
        while(true) {
            await Task.Delay(2000);
            try {
                if(ShuttingDown)continue;
                if(OperatingSystem.IsMacOS()) {
                    string guard=Path.Combine(Folder,"guard-ready.json");
                    if(!File.Exists(guard)||DateTime.UtcNow-File.GetLastWriteTimeUtc(guard)>TimeSpan.FromSeconds(12))continue;
                }
                var local=await Backend("status");
                if(!local.Ok)continue;
                if(local.Connected||local.WaitingForInput){context.Update(false,false,false,true,DateTime.UtcNow);policy.Observe(true,false,true,DateTime.UtcNow);continue;}
                var peer=await PeerRequest("status-v1",1000,3000);
                bool intent=context.Update(LocalActivityRecent(),peer is {Ok:true},peer is {Connected:true} or {WaitingForInput:true},false,DateTime.UtcNow);
                if(policy.Observe(intent,peer is {PeerUnavailable:true},false,DateTime.UtcNow)) {
                    context.ClaimStarted();
                    Log("Automatic takeover: assigned peer unreachable on repeated checks");
                    await Execute("take");
                    policy.AttemptCompleted(DateTime.UtcNow);
                }
            } catch(Exception e) {Log("Automatic takeover check: "+e.GetType().Name);await Task.Delay(5000);}
        }
    }
    static async Task<Reply> PrepareShutdown()
    {
        ShuttingDown=true;
        CancelObserver(Interlocked.Exchange(ref MacObserverStop,null));
        if(!await Action.WaitAsync(0))return new(false,"A mouse operation is still finishing");
        Reply released;bool owned=false;
        try {
            Interlocked.Increment(ref OwnershipGeneration);
            Log("Prepare shutdown: release configured mouse locally");
            var current=await Backend("status");owned=current.Connected||current.WaitingForInput;
            released=owned?await Backend(OperatingSystem.IsWindows()?"release-fast":"release"):new(true,"No locally assigned mouse to release");
            if(released.Ok){File.WriteAllText(ReleaseLease,"released");if(File.Exists(ReleaseTokenFile))File.Delete(ReleaseTokenFile);}
        } catch(Exception e) {released=new(false,e.Message);}
        finally {Action.Release();}
        if(released.Ok&&owned) {
            try {var notified=await PeerRequest("source-shutdown-v1",750,750);Log("Shutdown handoff notification: "+notified?.Ok);}
            catch(Exception e){Log("Shutdown handoff notification: "+e.GetType().Name);}
        }
        return released;
    }
    static async Task<Reply> Execute(string command)
    {
        bool restore=command.StartsWith("restore:",StringComparison.Ordinal);
        if (command is not ("take" or "release") && !restore) return new(false, "Unsupported local request");
        if(command=="take")ShuttingDown=false;
        var observer=Interlocked.Exchange(ref MacObserverStop,null);
        CancelObserver(observer);
        if (!await Action.WaitAsync(observer is null?0:4000)) return new(false, "A switch is already in progress");
        try
        {
            Log("Begin "+command);
            long generation=Interlocked.Increment(ref OwnershipGeneration);
            if(command=="take")MacFreshBond=false;
            if(restore) {
                if(!File.Exists(ReleaseLease)||!File.Exists(ReleaseTokenFile)
                    || !GlideMouseSwitch.ReleaseToken.CanRestore(command[8..],File.ReadAllText(ReleaseTokenFile),File.ReadAllText(ReleaseLease),File.GetLastWriteTimeUtc(ReleaseTokenFile),DateTime.UtcNow))return new(false,"The previous mouse release is no longer current");
                File.Delete(ReleaseLease);File.Delete(ReleaseTokenFile);
                var restored=await Backend("take");Log("Source restore completed: "+restored.Ok+" "+restored.Message);return restored;
            }
            PeerReleaseToken="";
            var beforeRelease=command=="release"?await Backend("status"):null;
            bool wasConnected=beforeRelease is {Connected:true} or {WaitingForInput:true};
            async Task<Reply?> PrepareTarget() {
                if(File.Exists(ReleaseLease))File.Delete(ReleaseLease);
                if(File.Exists(ReleaseTokenFile))File.Delete(ReleaseTokenFile);
                return null;
            }
            Reply result;
            try {
                result = command == "release" ? await Backend("release") : await SwitchFlow.Take(OperatingSystem.IsWindows(), Backend, ReleasePeer, () => Task.Delay(1000), Log, PrepareTarget,
                    OperatingSystem.IsMacOS()?async()=>await Backend("preflight"):null);
            } catch(Exception e) { result=new(false,e.Message); }
            if(command=="take"&&result.Ok&&(result.Connected||result.WaitingForInput))await PrepareTarget();
            if(command=="release"&&!result.Ok&&wasConnected&&!(await Backend("status")).Connected) {
                if(File.Exists(ReleaseLease))File.Delete(ReleaseLease);
                var recovery=await Backend("take");Log("Source release recovery: "+recovery.Ok+" "+recovery.Message);
                if(recovery.Connected)result=result with {Message=result.Message+"; source connection restored"};
            }
            if(command=="release"&&result.Ok&&wasConnected) {
                string token=Guid.NewGuid().ToString();File.WriteAllText(ReleaseTokenFile,token);
                File.WriteAllText(ReleaseLease,"released");result=result with {ReleaseToken=token};
            }
            if(command=="take"&&!result.Ok&&!result.Connected&&Guid.TryParseExact(PeerReleaseToken,"D",out _)) {
                var quiet=await Backend("release");
                if(quiet.Ok) {
                    var previous=await PeerRequest("restore-v1 "+PeerReleaseToken);
                    Log("Automatic source restore: "+previous?.Ok+" "+previous?.Message);
                    if(previous is {Connected:true})result=result with {Message=result.Message+"; the previous computer restored the mouse"};
                } else Log("Source restore withheld: local mouse release did not finish");
            }
            Log(command+" completed: "+result.Ok+" "+result.Message);
            if(command=="take"&&result.Ok&&(result.Connected||result.WaitingForInput)&&OperatingSystem.IsMacOS())_ = WatchMacConnection(generation);
            return result;
        }
        catch (Exception e) { return new(false, e.Message); }
        finally { Action.Release(); }
    }
    static async Task WatchMacConnection(long generation)
    {
        using var stop=new CancellationTokenSource();
        var previous=Interlocked.Exchange(ref MacObserverStop,stop);CancelObserver(previous);
        BackendCancellation.Value=stop.Token;
        try {
            for(int check=0;check<15;check++) {
                await Task.Delay(2000,stop.Token);
                if(Interlocked.Read(ref OwnershipGeneration)!=generation||File.Exists(ReleaseLease))return;
                if(!await Action.WaitAsync(0))return;
                try {
                    if(Interlocked.Read(ref OwnershipGeneration)!=generation||File.Exists(ReleaseLease))return;
                    var state=await Backend("status");
                    if(!state.Ok)return;
                    if(state.Connected||state.WaitingForInput)continue;
                    // Repair only our still-current local claim. Never contact
                    // or release the peer from this observer.
                    Log("Mac connection dropped after claim; reconnecting current bond");
                    MacFreshBond=true;
                    var repaired=await Backend("take");
                    Log("Mac current-bond repair: "+repaired.Ok+" "+repaired.Message);
                    if(!repaired.Connected)return;
                } finally { Action.Release(); }
            }
        } catch(OperationCanceledException) when(stop.IsCancellationRequested) { }
        catch(Exception e) { Log("Mac connection observer: "+e.GetType().Name); }
        finally { Interlocked.CompareExchange(ref MacObserverStop,null,stop);BackendCancellation.Value=default; }
    }
    static async Task<Reply> Backend(string action)
    {
        if (OperatingSystem.IsMacOS())
        {
            string address = string.Join("-", Enumerable.Range(0, 6).Select(i => Config.Address.Substring(i * 2, 2)));
            if(action=="take") {
                string marker=Path.Combine(Folder,"hid-ignore-active");
                File.WriteAllText(marker,"prepare native mouse input");
                for(int check=0;check<50&&File.Exists(marker);check++)await Task.Delay(100);
                if(File.Exists(marker))return new(false,"The Mac menu helper has not prepared this mouse's native input");
            }
            async Task<bool> Connected() { var p = await Run("--is-connected", address); if(p.Exit!=0)throw new IOException($"Bluetooth status unavailable (blueutil exit {p.Exit}). Check this app's Bluetooth permission in macOS settings.");return p.Text.Trim() == "1"; }
            async Task<bool> Paired() {
                var known=await Run("--format","json","--info",address);
                if(known.Exit!=0)throw new IOException("Could not verify the mouse bond");
                using var info=JsonDocument.Parse(known.Text);
                return info.RootElement.TryGetProperty("address",out var selected)
                    &&Regex.Replace(selected.GetString()??"","[:-]","").ToUpperInvariant()==Config.Address
                    &&info.RootElement.TryGetProperty("paired",out var value)&&value.ValueKind==JsonValueKind.True;
            }
            if(action=="preflight") {
                string guard=Path.Combine(Folder,"guard-ready.json");
                if(!File.Exists(guard)||DateTime.UtcNow-File.GetLastWriteTimeUtc(guard)>TimeSpan.FromSeconds(12))return new(false,"Open GlideMouse Switch and allow Bluetooth before switching; the other computer keeps the mouse");
                if(await Connected())return new(true,"Connected to this Mac",true);
                if(HasPendingMacBond&&await Paired())return new(true,"Mouse assigned to this Mac; waiting for mouse activity",WaitingForInput:true);
                if(await Paired()) {
                    var retired=await Run("--unpair",address);
                    if(retired.Exit!=0)return new(false,"Could not prepare the old Mac bond; the other computer keeps the mouse");
                    Log("Mac stale bond retired before requesting peer release");
                }
                return new(true,"Mac handoff preparation completed");
            }
            if (action == "status") {
                bool connected=await Connected();
                bool waiting=!connected&&HasPendingMacBond&&await Paired();
                return new(true,connected?"Connected to this Mac":waiting?"Mouse assigned to this Mac; waiting for mouse activity":"Not connected",connected,WaitingForInput:waiting);
            }
            if (action == "release")
            {
                // The native menu helper performs the configured device-manager
                // release and acknowledges its observed completion.
                string guard=Path.Combine(Folder,"guard-ready.json");
                if(!File.Exists(guard)||DateTime.UtcNow-File.GetLastWriteTimeUtc(guard)>TimeSpan.FromSeconds(12))return new(false,"Open the GlideMouse Switch menu app before releasing the mouse");
                if(File.Exists(PendingMacBond))File.Delete(PendingMacBond);
                MacFreshBond=false;
                if(!await Connected()&&!await Paired()) { File.WriteAllText(ReleaseLease,"released");return new(true,"Mouse is already disconnected from this Mac"); }
                using var guardState=JsonDocument.Parse(File.ReadAllText(guard));
                if(!guardState.RootElement.TryGetProperty("hidReleaseReady",out var capability)||capability.ValueKind!=JsonValueKind.True)return new(false,"The Mac menu helper cannot yet release the mouse virtual cable; pairing was kept");
                File.WriteAllText(ReleaseLease,"preparing");
                string token=Guid.NewGuid().ToString();
                string request=Path.Combine(Folder,"hid-release-request.json");
                File.WriteAllText(request+".tmp",JsonSerializer.Serialize(new {Token=token,Address=Config.Address,Expires=DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()/1000.0+10},Json));
                File.Move(request+".tmp",request,true);
                bool sent=false;
                for(int check=0;check<40;check++) {
                    await Task.Delay(250);
                    string response=Path.Combine(Folder,"hid-release-response.json");
                    if(!File.Exists(response))continue;
                    using var releaseResponse=JsonDocument.Parse(File.ReadAllText(response));
                    if(releaseResponse.RootElement.GetProperty("Token").GetString()!=token)continue;
                    sent=releaseResponse.RootElement.GetProperty("Completed").GetBoolean();
                    Log("Mouse system release: "+releaseResponse.RootElement.GetProperty("Message").GetString());break;
                }
                if(!sent) { File.Delete(ReleaseLease);return new(false,"The Mac device release could not be confirmed"); }
                File.WriteAllText(ReleaseLease,"released");
                await Run("--disconnect", address);
                await Task.Delay(500);
                bool released = !await Connected(); return new(released, released ? "Mouse released by the Mac device manager" : "Mouse is still connected");
            }
            if (await Connected()) return new(true, "Connected to this Mac", true);
            var known = await Run("--format", "json", "--info", address);
            if (known.Exit != 0) return new(false, "Could not read the mouse pairing state");
            using var deviceInfo = JsonDocument.Parse(known.Text);
            var device = deviceInfo.RootElement;
            if (!device.TryGetProperty("address", out var deviceAddress) ||
                Regex.Replace(deviceAddress.GetString() ?? "", "[:-]", "").ToUpperInvariant() != Config.Address ||
                !device.TryGetProperty("paired", out var pairedState) ||
                pairedState.ValueKind is not (JsonValueKind.True or JsonValueKind.False))
                return new(false, "The Bluetooth backend returned an invalid mouse pairing state");
            bool wasPaired = pairedState.GetBoolean();
            return await MacClaimFlow.Take(wasPaired,MacFreshBond||HasPendingMacBond,Connected,
                async()=>{var r=await Run("--connect",address);return r.Exit==0;},
                async()=>{var r=await Run("--unpair",address);if(r.Exit==0)await Task.Delay(600);return r.Exit==0;},
                async()=>{var r=await Run("--pair",address,"0000");return r.Exit==0;},
                ()=>Task.Delay(300),()=>{MacFreshBond=true;File.WriteAllText(PendingMacBond,Config.Address);},Guid.TryParseExact(PeerReleaseToken,"D",out _));
        }
        var result = await Run("--" + action);
        return JsonSerializer.Deserialize<Reply>(result.Text.Trim(), Json) ?? new(false, "No valid response from the Windows backend");
    }
    static async Task<(int Exit, string Text)> Run(params string[] arguments)
    {
        var start = new ProcessStartInfo(Config.Backend) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, CreateNoWindow = true };
        start.RedirectStandardInput=true;
        if(OperatingSystem.IsMacOS()&&arguments[0]=="--pair")start.Environment["GLIDEMOUSE_APPROVE_TARGET"]=Config.Address;
        foreach (string arg in arguments) start.ArgumentList.Add(arg);
        using var child = Process.Start(start) ?? throw new IOException("Could not start Bluetooth backend");
        child.StandardInput.Close();
        Task<string> output = child.StandardOutput.ReadToEndAsync(), error = child.StandardError.ReadToEndAsync();
        int seconds=OperatingSystem.IsMacOS()?(arguments[0] is "--is-connected" or "--format"?5:arguments[0]=="--pair"?20:25):120;
        using var limit = CancellationTokenSource.CreateLinkedTokenSource(BackendCancellation.Value);
        limit.CancelAfter(TimeSpan.FromSeconds(seconds));
        try { await child.WaitForExitAsync(limit.Token); }
        catch (OperationCanceledException) {
            try{child.Kill(true);}catch(InvalidOperationException){}
            await child.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(3));
            BackendCancellation.Value.ThrowIfCancellationRequested();
            Log("Backend "+arguments[0]+" timed out after "+seconds+" seconds");
            if(OperatingSystem.IsMacOS()&&arguments[0] is "--connect" or "--pair" or "--unpair")return (75,"");
            throw new IOException("Bluetooth status timed out; check the Mac helper's Bluetooth access");
        }
        string detail=await error;
        if(child.ExitCode!=0)Log("Backend "+arguments[0]+" exit "+child.ExitCode+": "+detail.Trim());
        return (child.ExitCode, await output);
    }
}
