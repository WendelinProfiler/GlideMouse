using GlideMouseSwitch;
int cases = 0;
async Task Check(bool windows, Reply initial, Reply? source, Reply taken, Reply after, bool expected, string[] expectedCalls)
{
    var calls = new List<string>(); bool first = true;
    Task<Reply> Backend(string action) { calls.Add(action); if(action == "take") return Task.FromResult(taken); var result=first?initial:after; first=false; return Task.FromResult(result); }
    Task<Reply?> Peer() { calls.Add("peer");return Task.FromResult(source); }
    var result = await SwitchFlow.Take(windows, Backend, Peer, () => Task.CompletedTask);
    if (result.Ok != expected || !calls.SequenceEqual(expectedCalls)) throw new Exception("Wrong switch flow: " + string.Join(",", calls));
    cases++;
}
var idle = new Reply(true,"idle");var ready=new Reply(true,"ready",true,true);var native=new Reply(true,"native",true,false);var failed=new Reply(false,"failed");
await Check(true,ready,null,ready,ready,true,["status"]);
await Check(true,idle,failed,ready,ready,false,["status","peer"]);
await Check(true,idle,null,ready,ready,true,["status","peer","take","status","status","status","status"]);
await Check(true,idle,ready,failed,ready,false,["status","peer","take"]);
await Check(true,idle,ready,ready,native,false,["status","peer","take","status"]);
await Check(true,native,ready,ready,ready,true,["status","peer","take","status","status","status","status"]);
await Check(false,idle,ready,native,native,true,["status","peer","take","status","status","status","status"]);
await Check(false,idle,ready,native,idle,false,["status","peer","take","status","take","status","take","status"]);
await Check(false,idle,ready,new(false,"Bluetooth denied"),idle,false,["status","peer","take"]);
await Check(false,idle,ready,new(false,"temporary link failure",Retryable:true),idle,false,["status","peer","take","take","take"]);
{
    int peerCalls=0,takeCalls=0,statusCalls=0;
    Task<Reply> Backend(string action) {
        if(action=="take") { takeCalls++;return Task.FromResult(takeCalls==1?new Reply(false,"temporary authentication failure",Retryable:true):native); }
        statusCalls++;return Task.FromResult(statusCalls==1?idle:native);
    }
    var result=await SwitchFlow.Take(false,Backend,()=>{peerCalls++;return Task.FromResult<Reply?>(ready);},()=>Task.CompletedTask);
    if(!result.Ok||takeCalls!=2||peerCalls!=1)throw new Exception("A transient Mac failure must recover without releasing the source twice");
    cases++;
}
{
    int peerCalls=0,takeCalls=0,statusCalls=0;
    Task<Reply> Backend(string action) {
        if(action=="take") { takeCalls++;return Task.FromResult(native); }
        statusCalls++;return Task.FromResult(statusCalls<=2?idle:native);
    }
    var result=await SwitchFlow.Take(false,Backend,()=>{peerCalls++;return Task.FromResult<Reply?>(ready);},()=>Task.CompletedTask);
    if(!result.Ok||takeCalls!=2||peerCalls!=1)throw new Exception("An unstable initial Mac connection must be re-established and verified");
    cases++;
}
{
    var now=DateTime.UtcNow;var token=Guid.NewGuid().ToString();
    void Lease(bool expected,string requested,string saved,string state,DateTime issued,DateTime current) {
        if(ReleaseToken.CanRestore(requested,saved,state,issued,current)!=expected)throw new Exception("Unsafe source restoration lease");cases++;
    }
    Lease(true,token,token,"released",now,now.AddSeconds(90));
    Lease(false,Guid.NewGuid().ToString(),token,"released",now,now);
    Lease(false,"not-a-token",token,"released",now,now);
    Lease(false,token,token,"preparing",now,now);
    Lease(false,token,token,"",now,now);
    Lease(false,token,token,"released",now,now.AddMinutes(4));
    Lease(false,token,token,"released",now.AddSeconds(1),now);
}
{
    var calls=new List<string>();
    Task<Reply> Backend(string action){calls.Add(action);return Task.FromResult(action=="status"?idle:ready);}
    var result=await SwitchFlow.Take(true,Backend,()=>Task.FromResult<Reply?>(failed),()=>Task.CompletedTask,prepareTarget:()=>{calls.Add("prepare");return Task.FromResult<Reply?>(null);});
    if(result.Ok||calls.Contains("prepare")||calls.Contains("take"))throw new Exception("An unreachable peer must not release local ownership or start Bluetooth pairing");cases++;
    calls.Clear();
    result=await SwitchFlow.Take(true,Backend,()=>{calls.Add("peer");return Task.FromResult<Reply?>(ready);},()=>Task.CompletedTask,prepareTarget:()=>{calls.Add("prepare");return Task.FromResult<Reply?>(failed);});
    if(result.Ok||!calls.SequenceEqual(new[]{"status","peer","prepare"}))throw new Exception("Target preparation must follow source release and prevent pairing on failure");cases++;
}
{
    int connects=0,removes=0,pairs=0,reads=0;bool remembered=false;
    Task<bool> Connected()=>Task.FromResult(++reads>1);
    var result=await MacClaimFlow.Take(false,false,Connected,
        ()=>{connects++;return Task.FromResult(false);},
        ()=>{removes++;return Task.FromResult(true);},
        ()=>{pairs++;return Task.FromResult(true);},()=>Task.CompletedTask,()=>remembered=true);
    if(!result.Ok||connects!=0||removes!=0||pairs!=1||!remembered)throw new Exception("An automatically connected fresh pairing must never be reopened");cases++;
    connects=removes=pairs=0;
    result=await MacClaimFlow.Take(true,true,()=>Task.FromResult(false),
        ()=>{connects++;return Task.FromResult(false);},
        ()=>{removes++;return Task.FromResult(true);},
        ()=>{pairs++;return Task.FromResult(true);},()=>Task.CompletedTask,()=>{});
    if(!result.Ok||!result.WaitingForInput||result.Connected||connects!=1||removes!=0||pairs!=0)throw new Exception("A fresh idle bond must survive a transient connect failure without claiming a live connection");cases++;
    connects=removes=pairs=0;reads=0;
    result=await MacClaimFlow.Take(true,false,()=>Task.FromResult(++reads>5),
        ()=>{connects++;return Task.FromResult(false);},
        ()=>{removes++;return Task.FromResult(true);},
        ()=>{pairs++;return Task.FromResult(true);},()=>Task.CompletedTask,()=>{});
    if(!result.Ok||connects!=1||removes!=1||pairs!=1)throw new Exception("Only a proven disconnected old bond may be retired before pairing");cases++;
    result=await MacClaimFlow.Take(false,false,()=>Task.FromResult(false),
        ()=>throw new Exception("Connection must not precede successful authentication"),
        ()=>throw new Exception("There is no old bond to remove"),()=>Task.FromResult(false),()=>Task.CompletedTask,()=>throw new Exception("Failed pairing must not be marked fresh"));
    if(result.Ok||!result.Retryable)throw new Exception("Authentication failure must remain a failed claim");cases++;
}
{
    int pairs=0,removes=0,connects=0;
    var result=await MacClaimFlow.Take(true,false,()=>Task.FromResult(pairs>0),
        ()=>{connects++;return Task.FromResult(false);},
        ()=>{removes++;return Task.FromResult(true);},
        ()=>{pairs++;return Task.FromResult(true);},()=>Task.CompletedTask,()=>{},staleAfterPeerRelease:true);
    if(!result.Ok||connects!=0||removes!=1||pairs!=1)throw new Exception("A confirmed peer handoff must not waste a connection attempt on its old Mac bond");cases++;
    pairs=removes=connects=0;
    result=await MacClaimFlow.Take(true,true,()=>Task.FromResult(false),
        ()=>{connects++;return Task.FromResult(false);},
        ()=>{removes++;return Task.FromResult(true);},
        ()=>{pairs++;return Task.FromResult(true);},()=>Task.CompletedTask,()=>{},staleAfterPeerRelease:true);
    if(!result.Ok||!result.WaitingForInput||connects!=1||removes!=0||pairs!=0)throw new Exception("The source handoff token must never invalidate a fresh bond from this same claim");cases++;
}
{
    var waiting=new Reply(true,"waiting",WaitingForInput:true);
    await Check(false,waiting,failed,failed,failed,true,["status"]);
    await Check(false,idle,ready,waiting,idle,true,["status","peer","take"]);
    await Check(false,idle,ready,native,waiting,true,["status","peer","take","status"]);
    await Check(true,idle,ready,waiting,idle,false,["status","peer","take","status"]);
    int connects=0,removes=0;bool remembered=false;
    var result=await MacClaimFlow.Take(false,false,()=>Task.FromResult(false),
        ()=>{connects++;return Task.FromResult(false);},
        ()=>{removes++;return Task.FromResult(true);},
        ()=>Task.FromResult(true),()=>Task.CompletedTask,()=>remembered=true);
    if(!result.Ok||!result.WaitingForInput||result.Connected||!remembered||connects!=0||removes!=0)throw new Exception("Successful authentication without activity must retain the new owner and await real input");cases++;
}
{
    var calls=new List<string>();
    Task<Reply> Backend(string action){calls.Add(action);return Task.FromResult(action=="take"?native:idle);}
    var result=await SwitchFlow.Take(false,Backend,()=>{calls.Add("peer");return Task.FromResult<Reply?>(ready);},()=>Task.CompletedTask,
        preflight:()=>{calls.Add("preflight");return Task.FromResult<Reply?>(failed);});
    if(result.Ok||!calls.SequenceEqual(new[]{"status","preflight"}))throw new Exception("Failed target preparation must preserve the source connection");cases++;
    calls.Clear();
    result=await SwitchFlow.Take(false,Backend,()=>{calls.Add("peer");return Task.FromResult<Reply?>(failed);},()=>Task.CompletedTask,
        preflight:()=>{calls.Add("preflight");return Task.FromResult<Reply?>(null);});
    if(result.Ok||!calls.SequenceEqual(new[]{"status","preflight","peer"}))throw new Exception("Peer release must follow completed preparation");cases++;
    calls.Clear();
    result=await SwitchFlow.Take(false,Backend,()=>throw new Exception("Already-local connection must not release peer"),()=>Task.CompletedTask,
        preflight:()=>Task.FromResult<Reply?>(native));
    if(!result.Ok||!result.Connected||calls.Count!=1)throw new Exception("A connection established during preflight must be retained");cases++;
    result=await MacClaimFlow.Take(false,false,()=>Task.FromResult(false),()=>throw new Exception(),()=>throw new Exception(),
        ()=>Task.FromResult(false),()=>Task.CompletedTask,()=>throw new Exception(),staleAfterPeerRelease:true);
    if(result.Ok||result.Retryable)throw new Exception("Failed authentication after source release must return promptly for source restoration");cases++;
}
{
    var offline=AutoTakePolicy.ReleaseResult(new(false,"offline",PeerUnavailable:true));
    if(offline is not {Ok:true,PeerUnavailable:true})throw new Exception("An offline source must not prevent a standalone target claim");cases++;
    var denied=AutoTakePolicy.ReleaseResult(new(false,"certificate or release failure"));
    if(denied is not {Ok:false,PeerUnavailable:false})throw new Exception("Authentication and explicit release failures must not become offline claims");cases++;
    var policy=new AutoTakePolicy();var now=DateTime.UnixEpoch;
    void Observe(bool expected,bool active,bool unavailable,bool assigned,int seconds){if(policy.Observe(active,unavailable,assigned,now.AddSeconds(seconds))!=expected)throw new Exception("Unsafe automatic takeover policy");cases++;}
    Observe(false,false,true,false,0);
    Observe(false,true,true,false,1);
    Observe(false,true,false,false,2);
    Observe(false,true,true,false,3);
    Observe(false,true,true,true,4);
    Observe(false,true,true,false,5);
    Observe(true,true,true,false,6);
    Observe(false,true,true,false,7);
    Observe(false,true,true,false,65);
    Observe(false,true,true,false,66);
    Observe(true,true,true,false,68);
}
{
    var now=DateTime.UnixEpoch;var context=new AutoTakeContext(now);
    void Intent(bool expected,bool input,bool reached,bool peerAssigned,bool here,int seconds){if(context.Update(input,reached,peerAssigned,here,now.AddSeconds(seconds))!=expected)throw new Exception("Automatic intent lost or incorrectly inferred");cases++;}
    Intent(true,false,false,false,false,0);
    Intent(false,false,false,false,false,31);
    Intent(true,false,true,true,false,40);
    Intent(true,false,false,false,false,45);
    Intent(false,false,false,false,false,71);
    Intent(true,true,false,false,false,80);
    Intent(true,false,false,false,false,90);
    Intent(false,false,false,false,false,101);
    context.ClaimStarted();Intent(true,false,false,false,false,200);
    Intent(false,false,false,false,true,201);
    Intent(false,false,false,false,false,202);
    var policy=new AutoTakePolicy();
    policy.Observe(true,true,false,now);policy.Observe(true,true,false,now.AddSeconds(1));
    policy.AttemptCompleted(now.AddSeconds(120));
    if(policy.Observe(true,true,false,now.AddSeconds(121))||policy.Observe(true,true,false,now.AddSeconds(179)))throw new Exception("Long failed claims must not cause immediate retry storms");cases++;
    if(policy.Observe(true,true,false,now.AddSeconds(180))||!policy.Observe(true,true,false,now.AddSeconds(182)))throw new Exception("Completed-attempt cooldown must allow a later bounded retry");cases++;
}
Console.WriteLine($"PASS {cases} host-switch cases including idle peer loss, native-shutdown coordination and retry limits");
namespace GlideMouseSwitch { internal sealed record Reply(bool Ok,string Message,bool Connected=false,bool DriverReady=false,bool Retryable=false,bool WaitingForInput=false,bool PeerUnavailable=false); }
