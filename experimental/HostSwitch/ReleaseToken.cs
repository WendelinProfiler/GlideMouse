namespace GlideMouseSwitch;

internal static class ReleaseToken
{
    internal static bool CanRestore(string requested, string saved, string state, DateTime issued, DateTime now)
        => state == "released" && Guid.TryParseExact(requested,"D",out var token)
        && Guid.TryParseExact(saved,"D",out var current) && token == current
        && issued <= now && now-issued <= TimeSpan.FromMinutes(3);
}
