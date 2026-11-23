namespace GlideMouseSwitch;

internal sealed class AutoTakePolicy
{
    private int misses;
    private DateTime nextAttempt;
    internal bool Observe(bool localActivity, bool peerUnavailable, bool alreadyAssigned, DateTime now)
    {
        if(alreadyAssigned||!localActivity||!peerUnavailable){misses=0;return false;}
        if(now<nextAttempt)return false;
        if(++misses<2)return false;
        misses=0;nextAttempt=now.AddSeconds(60);return true;
    }
    internal static Reply? ReleaseResult(Reply? peer) => peer is {Ok:false,PeerUnavailable:true}
        ? new(true,"Other computer unreachable; claiming only the configured mouse locally",PeerUnavailable:true)
        : peer;
    internal void AttemptCompleted(DateTime now) { misses=0;nextAttempt=now.AddSeconds(60); }
}

internal sealed class AutoTakeContext(DateTime started)
{
    private DateTime activityUntil=started.AddSeconds(30),peerAssignedUntil;
    private bool intendedHere;
    internal bool Update(bool recentInput,bool peerReached,bool peerAssigned,bool assignedHere,DateTime now)
    {
        if(assignedHere){intendedHere=false;peerAssignedUntil=DateTime.MinValue;return false;}
        if(recentInput)activityUntil=now.AddSeconds(20);
        if(peerReached){peerAssignedUntil=peerAssigned?now.AddSeconds(30):DateTime.MinValue;if(peerAssigned)intendedHere=false;}
        return intendedHere||now<activityUntil||now<peerAssignedUntil;
    }
    internal void ClaimStarted()=>intendedHere=true;
}
