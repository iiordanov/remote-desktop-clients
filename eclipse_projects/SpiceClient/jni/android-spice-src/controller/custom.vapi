using GLib;

namespace Custom {

	[CCode (cname = "g_warn_if", cheader_filename = "custom.h")]
	public bool warn_if(bool condition);
}

namespace Spice {
	[CCode (cheader_filename = "namedpipe.h")]
	public class NamedPipe: Object {
		public NamedPipe (string name) throws GLib.Error;
	}

	[CCode (cheader_filename = "namedpipeconnection.h")]
	public class NamedPipeConnection: GLib.IOStream {
	}

	[CCode (cheader_filename = "namedpipelistener.h")]
	public class NamedPipeListener: Object {
		[CCode (has_construct_function = false)]
		public NamedPipeListener ();
		public async unowned Spice.NamedPipeConnection accept_async (GLib.Cancellable? cancellable = null, out GLib.Object? source_object = null) throws GLib.Error;
		public void add_named_pipe (NamedPipe namedpipe);
	}
}

namespace Win32 {
	[CCode (cheader_filename = "windows.h", cname = "GetCurrentProcessId")]
	public uint32 GetCurrentProcessId ();
}
