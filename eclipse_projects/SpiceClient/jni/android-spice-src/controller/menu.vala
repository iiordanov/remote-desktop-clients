// Copyright (C) 2011 Red Hat, Inc.

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, see <http://www.gnu.org/licenses/>.

using GLib;
using Custom;
using SpiceProtocol.Controller;

namespace SpiceCtrl {

public class MenuItem: Object {

	public Menu submenu;
	public int parent_id;
	public int id;
	public string text;
	public string accel;
	public SpiceProtocol.Controller.MenuFlags flags;

	public MenuItem (int id, string text, SpiceProtocol.Controller.MenuFlags flags) {
		this.id = id;
		this.text = text;
		this.flags = flags;
	}

	public MenuItem.from_string (string str) throws SpiceCtrl.Error {
		var params = str.split (SpiceProtocol.Controller.MENU_PARAM_DELIMITER);
		if (warn_if (params.length != 5))
			throw new SpiceCtrl.Error.VALUE(""); /* Vala: why is it mandatory to give a string? */
		parent_id = int.parse (params[0]);
		id = int.parse (params[1]);
		var textaccel = params[2].split ("\t");
		text = textaccel[0];
		if (textaccel.length > 1)
			accel = textaccel[1];
		flags = (SpiceProtocol.Controller.MenuFlags)int.parse (params[3]);

		submenu = new Menu ();
	}

	public string to_string () {
		var sub = submenu.to_string ();
		var str = @"pid: $parent_id, id: $id, text: \"$text\", flags: $flags";
		foreach (var l in sub.to_string ().split ("\n")) {
			if (l == "")
				continue;
			str += @"\n    $l";
		}
		return str;
	}
}

public class Menu: Object {

	public List<MenuItem> items;

	public Menu? find_id (int id) {
		if (id == 0)
			return this;

		foreach (var item in items) {
			if (item.id == id)
				return item.submenu;

			var menu = item.submenu.find_id (id);
			if (menu != null)
				return menu;
		}

		return null;
	}

	public Menu.from_string (string str) {
		foreach (var itemstr  in str.split (SpiceProtocol.Controller.MENU_ITEM_DELIMITER)) {
			try {
				if (itemstr.length == 0)
					continue;
				var item = new MenuItem.from_string (itemstr);
				var parent = find_id (item.parent_id);
				if (parent == null)
					throw new SpiceCtrl.Error.VALUE("Invalid parent menu id");
				parent.items.append (item);
			} catch (SpiceCtrl.Error e) {
				warning (e.message);
			}
		}
	}

	public string to_string () {
		var str = "";
		foreach (var i in items)
			str += @"\n$i";
		return str;
	}
}

} // SpiceCtrl
