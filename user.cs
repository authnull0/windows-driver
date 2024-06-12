
using System;

using System.IO;

using System.Runtime.InteropServices;

using System.Text;

using Microsoft.Win32.SafeHandles;

class Program

{

    const string DevicePath = @"\\.\ProcessMonitor";

    const int IOCTL_TRIGGER_LOGIN = unchecked ((int)0x80002000);

    const string EventName = @"Global\ProcessMonitorEvent";

    static void Main(string[] args)

    {

        SafeFileHandle handle = CreateFile(DevicePath, FileAccess.ReadWrite, FileShare.ReadWrite, IntPtr.Zero, FileMode.Open, 0, IntPtr.Zero);

        if (handle.IsInvalid)

        {

            Console.WriteLine("Failed to open device");

            return;

        }

        IntPtr eventHandle = OpenEvent(EventAccess.EVENT_ALL_ACCESS, false, EventName);

        if (eventHandle == IntPtr.Zero)

        {

            Console.WriteLine("Failed to open event");

            return;

        }

        while (true)

        {

            WaitForSingleObject(eventHandle, INFINITE);

            Console.WriteLine("Received process creation event");

            PromptForWindowsLogin();

        }

    }

    static void PromptForWindowsLogin()

    {

        IntPtr outCredBuffer;

        uint outCredSize;

        var authPackage = 0;

        var save = false;

        var user = new StringBuilder(100);

        var password = new StringBuilder(100);

        var domain = new StringBuilder(100);

        var credUiInfo = new CREDUI_INFO

        {

            cbSize = Marshal.SizeOf(typeof(CREDUI_INFO)),

            hwndParent = IntPtr.Zero,

            pszMessageText = "Please log in",

            pszCaptionText = "Authentication Required",

            hbmBanner = IntPtr.Zero

        };

        var result = CredUIPromptForWindowsCredentials(ref credUiInfo, 0, ref authPackage, IntPtr.Zero, 0, out outCredBuffer, out outCredSize, ref save, CREDUI_FLAGS.GENERIC_CREDENTIALS | CREDUI_FLAGS.DO_NOT_PERSIST);

        if (result == 0)

        {

            var username = new StringBuilder(100);

            var passwordBuffer = new StringBuilder(100);

            var domainBuffer = new StringBuilder(100);

            uint usernameSize = (uint)username.Capacity;

            uint passwordSize = (uint)passwordBuffer.Capacity;

            uint domainSize = (uint)domainBuffer.Capacity;

            CredUnPackAuthenticationBuffer(0, outCredBuffer, outCredSize, username, ref usernameSize, domainBuffer, ref domainSize, passwordBuffer, ref passwordSize);

            // Handle authentication here

            Console.WriteLine("Username: " + username);

            Console.WriteLine("Password: " + passwordBuffer);

        }

        CredFree(outCredBuffer);

    }

    [DllImport("kernel32.dll", SetLastError = true)]

    static extern SafeFileHandle CreateFile(string lpFileName, FileAccess dwDesiredAccess, FileShare dwShareMode, IntPtr lpSecurityAttributes, FileMode dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);

    [DllImport("kernel32.dll", SetLastError = true)]

    static extern bool DeviceIoControl(SafeFileHandle hDevice, int dwIoControlCode, [In] byte[] lpInBuffer, int nInBufferSize, [Out] byte[] lpOutBuffer, int nOutBufferSize, out int lpBytesReturned, IntPtr lpOverlapped);

    [DllImport("kernel32.dll", SetLastError = true)]

    static extern IntPtr OpenEvent(EventAccess dwDesiredAccess, bool bInheritHandle, string lpName);

    [DllImport("kernel32.dll", SetLastError = true)]

    static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("advapi32.dll", SetLastError = true)]

    static extern bool CredUnPackAuthenticationBuffer(uint dwFlags, IntPtr pAuthBuffer, uint cbAuthBuffer, StringBuilder pszUserName, ref uint pcchMaxUserName, StringBuilder pszDomainName, ref uint pcchMaxDomainName, StringBuilder pszPassword, ref uint pcchMaxPassword);

    [DllImport("credui.dll", CharSet = CharSet.Unicode)]

    //public static extern bool CredUnPackAuthenticationBuffer(uint dwFlags, IntPtr pAuthBuffer, uint cbAuthBuffer, StringBuilder pszUserName, ref uint pcchMaxUserName, StringBuilder pszDomainName, ref uint pcchMaxDomainName, StringBuilder pszPassword, ref uint pcchMaxPassword);

    //[DllImport("credui.dll", CharSet = CharSet.Unicode)]

    public static extern int CredUIPromptForWindowsCredentials(ref CREDUI_INFO pUiInfo, int dwAuthError, ref int pulAuthPackage, IntPtr pvInAuthBuffer, uint ulInAuthBufferSize, out IntPtr ppvOutAuthBuffer, out uint pulOutAuthBufferSize, ref bool pfSave, CREDUI_FLAGS dwFlags);

    [DllImport("credui.dll", CharSet = CharSet.Unicode)]

    public static extern void CredFree([In] IntPtr Buffer);

    [Flags]

    public enum EventAccess : uint

    {

        EVENT_ALL_ACCESS = 0x1F0003,

    }

    public const uint INFINITE = 0xFFFFFFFF;

    [Flags]

    public enum CREDUI_FLAGS

    {

        GENERIC_CREDENTIALS = 0x00000400,

        DO_NOT_PERSIST = 0x00000002,

    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]

    public struct CREDUI_INFO

    {

        public int cbSize;

        public IntPtr hwndParent;

        public string pszMessageText;

        public string pszCaptionText;

        public IntPtr hbmBanner;

    }

}
