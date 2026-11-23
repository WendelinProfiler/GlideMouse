using System.Runtime.InteropServices;

namespace GlideMouseSwitch;

internal static class WindowsHotkey
{
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate IntPtr WindowProcedure(IntPtr window,uint message,UIntPtr wparam,IntPtr lparam);
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
    private struct WindowClass {
        public uint Size,Style;public IntPtr Procedure;public int ClassExtra,WindowExtra;
        public IntPtr Instance,Icon,Cursor,Background;
        public string? Menu;public string ClassName;public IntPtr SmallIcon;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct InputInfo { public uint Size,Time; }
    private static readonly WindowProcedure Procedure=HandleWindow;
    private static Func<Task>? shutdown;
    private static Action<string>? journal;
    private static int ending;
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]private static extern IntPtr GetModuleHandleW(string? name);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]private static extern ushort RegisterClassExW(ref WindowClass windowClass);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]private static extern IntPtr CreateWindowExW(uint extended,string className,string title,uint style,int x,int y,int width,int height,IntPtr parent,IntPtr menu,IntPtr instance,IntPtr param);
    [DllImport("user32.dll")]private static extern IntPtr DefWindowProcW(IntPtr window,uint message,UIntPtr wparam,IntPtr lparam);
    [DllImport("user32.dll")]private static extern IntPtr DispatchMessageW(ref Message message);
    [DllImport("user32.dll")][return:MarshalAs(UnmanagedType.Bool)]private static extern bool DestroyWindow(IntPtr window);
    [DllImport("user32.dll")][return:MarshalAs(UnmanagedType.Bool)]private static extern bool GetLastInputInfo(ref InputInfo info);
    internal static bool LocalActivityRecent() {
        if(!OperatingSystem.IsWindows())return false;
        var info=new InputInfo{Size=(uint)Marshal.SizeOf<InputInfo>()};
        return GetLastInputInfo(ref info)&&unchecked((uint)Environment.TickCount64-info.Time)<4000;
    }
    private static IntPtr HandleWindow(IntPtr window,uint message,UIntPtr wparam,IntPtr lparam) {
        if(message==0x0011)return new IntPtr(1); // Never veto shutdown.
        if(message==0x0016) {
            if(wparam!=UIntPtr.Zero&&Interlocked.Exchange(ref ending,1)==0&&shutdown is not null) {
                try {journal?.Invoke("Windows session ending: bounded mouse release");Task.Run(shutdown).Wait(2500);}
                catch(Exception e){journal?.Invoke("Session-end release: "+e.GetType().Name);}
            }
            return IntPtr.Zero;
        }
        return DefWindowProcW(window,message,wparam,lparam);
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct Message
    {
        public IntPtr Window;
        public uint Id;
        public UIntPtr WParam;
        public IntPtr LParam;
        public uint Time;
        public int X, Y;
        public uint Private;
    }
    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool RegisterHotKey(IntPtr window, int id, uint modifiers, uint key);
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool UnregisterHotKey(IntPtr window, int id);
    [DllImport("user32.dll")]
    private static extern int GetMessageW(out Message message, IntPtr window, uint first, uint last);

    internal static void Start(Func<Task> take, Action<string> log, Func<Task>? releaseAtShutdown=null)
    {
        if (!OperatingSystem.IsWindows()) return;
        new Thread(() =>
        {
            shutdown=releaseAtShutdown;journal=log;
            const string name="GlideMouseSwitch.SessionEnd.v1";
            var cls=new WindowClass{Size=(uint)Marshal.SizeOf<WindowClass>(),ClassName=name,Instance=GetModuleHandleW(null),Procedure=Marshal.GetFunctionPointerForDelegate(Procedure)};
            IntPtr lifecycle=RegisterClassExW(ref cls)!=0?CreateWindowExW(0,name,"GlideMouse Switch",0,0,0,0,0,IntPtr.Zero,IntPtr.Zero,cls.Instance,IntPtr.Zero):IntPtr.Zero;
            log(lifecycle!=IntPtr.Zero?"Session-end listener registered":"Session-end listener unavailable: "+Marshal.GetLastWin32Error());
            // MOD_NOREPEAT prevents a held key from requesting repeated claims.
            if (!RegisterHotKey(IntPtr.Zero, 1, 0x4003, 0x4D))
            {
                log("Shortcut Ctrl+Alt+M unavailable: " + Marshal.GetLastWin32Error());
            }
            else log("Shortcut Ctrl+Alt+M registered");
            try
            {
                while (GetMessageW(out var message, IntPtr.Zero, 0, 0) > 0) {
                    if (message.Id == 0x0312 && message.WParam.ToUInt64() == 1)
                        _ = Task.Run(take);
                    else DispatchMessageW(ref message);
                }
            }
            finally { UnregisterHotKey(IntPtr.Zero, 1);if(lifecycle!=IntPtr.Zero)DestroyWindow(lifecycle); }
        }) { IsBackground = true, Name = "GlideMouse shortcut" }.Start();
    }
}
