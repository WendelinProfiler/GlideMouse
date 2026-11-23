namespace GlideMouseSwitch;

internal static class MacClaimFlow
{
    // Pairing may already open the HID connection. Never open it again or
    // destroy the new bond merely because a redundant open returned an error.
    internal static async Task<Reply> Take(bool paired, bool freshBond,
        Func<Task<bool>> connected, Func<Task<bool>> connect,
        Func<Task<bool>> remove, Func<Task<bool>> pair,
        Func<Task> wait, Action rememberFreshBond, bool staleAfterPeerRelease = false)
    {
        if(await connected())return new(true,"Connected to this Mac",true);
        if(paired) {
            if(freshBond || !staleAfterPeerRelease) {
                await connect();
                for(int check=0;check<4;check++) {
                    if(await connected())return new(true,"Connected to this Mac",true);
                    await wait();
                }
            }
            if(freshBond)return new(true,"Mouse assigned to this Mac; waiting for mouse activity",WaitingForInput:true);
            if(!await remove())return new(false,"Could not retire the stale Mac pairing",Retryable:!staleAfterPeerRelease);
        }
        if(!await pair())return new(false,"The Mac could not complete mouse authentication",Retryable:!staleAfterPeerRelease);
        rememberFreshBond();
        for(int check=0;check<4;check++) {
            if(await connected())return new(true,"Connected to this Mac",true);
            await wait();
        }
        return new(true,"Mouse assigned to this Mac; waiting for mouse activity",WaitingForInput:true);
    }
}
