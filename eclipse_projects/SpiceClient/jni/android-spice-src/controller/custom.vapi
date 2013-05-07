using GLib;

namespace Custom {

	[CCode (cname = "g_warn_if", cheader_filename = "custom.h")]
	public bool warn_if(bool condition);
}

namespace Spice {

	[CCode (cname = "GObject", ref_function = "g_object_ref", unref_function = "g_object_unref", free_function = "")]
	class ControllerListener {
		[CCode (cname = "spice_controller_listener_new", cheader_filename = "spice-controller-listener.h")]
		public static ControllerListener new_listener (string addr) throws GLib.Error;

		[CCode (cname = "spice_controller_listener_accept_async", cheader_filename = "spice-controller-listener.h")]
		public async unowned GLib.IOStream accept_async (GLib.Cancellable? cancellable = null, out GLib.Object? source_object = null) throws GLib.Error;
	}

	[CCode (cname = "GObject", ref_function = "g_object_ref", unref_function = "g_object_unref", free_function = "")]
	class ForeignMenuListener {
		[CCode (cname = "spice_foreign_menu_listener_new", cheader_filename = "spice-foreign-menu-listener.h")]
		public static ForeignMenuListener new_listener (string addr) throws GLib.Error;

		[CCode (cname = "spice_foreign_menu_listener_accept_async", cheader_filename = "spice-foreign-menu-listener.h")]
		public async unowned GLib.IOStream accept_async (GLib.Cancellable? cancellable = null, out GLib.Object? source_object = null) throws GLib.Error;
	}
}
