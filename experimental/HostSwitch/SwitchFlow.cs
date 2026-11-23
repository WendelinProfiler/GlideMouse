namespace GlideMouseSwitch;

internal static class SwitchFlow
{
    internal static async Task<Reply> Take(bool windows, Func<string, Task<Reply>> backend,
        Func<Task<Reply?>> releasePeer, Func<Task> wait, Action<string>? trace = null,
        Func<Task<Reply?>>? prepareTarget = null, Func<Task<Reply?>>? preflight = null)
    {
        var current = await backend("status");
        if (current.Ok && ((current.Connected && (!windows || current.DriverReady))||(!windows&&current.WaitingForInput))) return current;
        if(preflight is not null) {
            var prepared=await preflight();
            if(prepared is {Ok:false})return prepared;
            if(!windows&&prepared is {Ok:true}&&(prepared.Connected||prepared.WaitingForInput))return prepared;
        }
        var source = await releasePeer();
        if (source is { Ok: false }) return new(false, "The other computer could not release the mouse: " + source.Message);
        if(prepareTarget is not null) { var prepared=await prepareTarget();if(prepared is {Ok:false})return prepared; }
        int attempts = windows ? 1 : 3;
        Reply last = new(false,"The mouse did not become ready");
        for(int attempt=0;attempt<attempts;attempt++)
        {
            trace?.Invoke($"Claim attempt {attempt+1} of {attempts}");
            var taken = await backend("take");
            if(!windows&&taken.Ok&&taken.WaitingForInput)return taken;
            if (!taken.Ok) {
                last=taken;
                if(windows || !taken.Retryable)return taken;
            } else {
                bool stable=true;
                for (int check = 0; check < 4; check++) {
                    await wait(); var state = await backend("status");
                    if(!windows&&state.Ok&&state.WaitingForInput)return state;
                    if (!state.Ok || !state.Connected || (windows && !state.DriverReady)) {
                        last=new(false,"The mouse connection did not remain ready",state.Connected,state.DriverReady);
                        if(!state.Ok)return state;
                        stable=false;break;
                    }
                }
                if(stable)return taken;
            }
            if(attempt+1<attempts)await wait();
        }
        return last;
    }
}
