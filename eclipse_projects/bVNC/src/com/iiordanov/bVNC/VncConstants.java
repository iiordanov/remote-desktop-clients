/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC;

/**
 * Keys for intent values
 */
public class VncConstants {
	public static final String CONNECTION = "com.iiordanov.bVNC.CONNECTION";

	public static final int CONN_TYPE_PLAIN        = 0;
	public static final int CONN_TYPE_SSH          = 1;
	public static final int CONN_TYPE_ULTRAVNC     = 2;
	public static final int CONN_TYPE_ANONTLS      = 3;
	public static final int CONN_TYPE_VENCRYPT     = 4;

	public static final int DIALOG_X509_CERT       = 1;
	public static final int DIALOG_SSH_CERT        = 2;

	public static final int EXTRA_KEYS_OFF         = 0;
	public static final int EXTRA_KEYS_ON          = 1;
	public static final int EXTRA_KEYS_TIMEOUT     = 2;
	
	public static final int ACTIVITY_GEN_KEY       = 1;

	public final static int AUTOX_SELECT_DISABLED  = 0;
	public final static int AUTOX_SELECT_XVFB      = 1;
	public final static int AUTOX_SELECT_XVNC      = 2;
	public final static int AUTOX_SELECT_XDUMMY    = 3;
	public final static int AUTOX_SELECT_FIND      = 4;
	public final static int AUTOX_SELECT_SUDO_FIND = 5;

	public final static int COMMAND_DISABLED      = 0;

	public final static int COMMAND_LINUX_START   = 100;
	public final static int COMMAND_LINUX_X11VNC  = 101;
	public final static int COMMAND_LINUX_STDVNC  = 102;
	public final static int COMMAND_LINUX_VINO    = 103;
	public final static int COMMAND_LINUX_END     = 199;

	public final static int COMMAND_WINDOWS_START = 200;
	public final static int COMMAND_WINDOWS_TIGHT = 201;
	public final static int COMMAND_WINDOWS_ULTRA = 202;
	public final static int COMMAND_WINDOWS_TIGER = 203;
	public final static int COMMAND_WINDOWS_REAL  = 204;
	public final static int COMMAND_WINDOWS_END   = 299;

	public final static int COMMAND_MACOSX_START  = 300;
	public final static int COMMAND_MACOSX_STDVNC = 301;
	public final static int COMMAND_MACOSX_END    = 399;

	public final static int COMMAND_CUSTOM        = 1000;

	public final static int COMMAND_AUTO_X_DISABLED      = 0;
	public final static int COMMAND_AUTO_X_CREATE_XVFB   = 1;
	public final static int COMMAND_AUTO_X_CREATE_XVNC   = 2;
	public final static int COMMAND_AUTO_X_CREATE_XDUMMY = 3;
	public final static int COMMAND_AUTO_X_FIND          = 4;
	public final static int COMMAND_AUTO_X_SUDO_FIND     = 5;
	public final static int COMMAND_AUTO_X_CUSTOM        = 6;

	public final static String COMMAND_AUTO_X_CREATE_XVFB_STRING   = "sh -c \"PORT= x11vnc -norc -nopw -wait_ui 2 -defer 15 -wait 15 -ncache 0 -timeout 10 -create -localhost ";
	public final static String COMMAND_AUTO_X_CREATE_XVNC_STRING   = "sh -c \"PORT= x11vnc -norc -nopw -wait_ui 2 -defer 15 -wait 15 -ncache 0 -timeout 10 -create -localhost -xvnc ";
	public final static String COMMAND_AUTO_X_CREATE_XDUMMY_STRING = "sh -c \"PORT= x11vnc -norc -nopw -wait_ui 2 -defer 15 -wait 15 -ncache 0 -timeout 10 -create -localhost -xdummy ";
	public final static String COMMAND_AUTO_X_FIND_STRING          = "sh -c \"PORT= x11vnc -norc -nopw -wait_ui 2 -defer 15 -wait 15 -ncache 0 -timeout 10 -find   -localhost ";
	public final static String COMMAND_AUTO_X_SUDO_FIND_STRING     = "sh -c \"PORT= sudo -S x11vnc -norc -nopw -wait_ui 2 -defer 15 -wait 15 -ncache 0 -timeout 10 -find -localhost -env FD_XDM=1 ";

	public final static String AUTO_X_USERPW             = "-unixpw $USER \"";
	public final static String AUTO_X_PASSWDFILE         = "-passwdfile rm:";
	public final static String AUTO_X_PWFILEBASENAME     = ".x11vnc_temp_pwd_";
	public final static String AUTO_X_CREATE_PASSWDFILE  = "umask 0077 && cat > ";
	public final static String AUTO_X_SYNC               = " ; sync";

	public final static String MV_DMRC_AWAY = "[ -O ${HOME}/.dmrc ] && mv ${HOME}/.dmrc ${HOME}/.dmrc.$$ ; ";
	public final static String MV_DMRC_BACK = " ; [ -O ${HOME}/.dmrc.$$ ] && mv ${HOME}/.dmrc.$$ ${HOME}/.dmrc";

	public final static int AUTOX_GEOM_SELECT_NATIVE = 0;
	public final static int AUTOX_GEOM_SELECT_CUSTOM = 1;

	public final static int AUTOX_SESS_PROG_SELECT_AUTO    = 0;
	public final static int AUTOX_SESS_PROG_SELECT_CUSTOM  = 1;
	public final static int AUTOX_SESS_PROG_SELECT_KDE     = 2;
	public final static int AUTOX_SESS_PROG_SELECT_UNITY   = 3;
	public final static int AUTOX_SESS_PROG_SELECT_UNITY2D = 4;
	public final static int AUTOX_SESS_PROG_SELECT_XFCE    = 5;
	public final static int AUTOX_SESS_PROG_SELECT_GNOME   = 6;
	public final static int AUTOX_SESS_PROG_SELECT_GNOMECL = 7;
	public final static int AUTOX_SESS_PROG_SELECT_TRINITY = 8;
	public final static int AUTOX_SESS_PROG_SELECT_MATE    = 9;

	public final static String AUTOX_SESS_PROG_AUTO    = "/etc/X11/Xsession";
	public final static String AUTOX_SESS_PROG_KDE     = "/usr/bin/startkde";
	public final static String AUTOX_SESS_PROG_UNITY   = "/usr/bin/gnome-session --session=ubuntu";
	public final static String AUTOX_SESS_PROG_UNITY2D = "/usr/bin/gnome-session --session=ubuntu-2d";
	public final static String AUTOX_SESS_PROG_XFCE    = "/usr/bin/xfce4-session";
	public final static String AUTOX_SESS_PROG_GNOME   = "/usr/bin/gnome-session --session=gnome";
	public final static String AUTOX_SESS_PROG_GNOMECL = "/usr/bin/gnome-session --session=gnome-classic";
	public final static String AUTOX_SESS_PROG_TRINITY = "/usr/bin/starttde";
	public final static String AUTOX_SESS_PROG_MATE    = "/usr/bin/mate-session";

	/**
	 * Returns a string matching a session selection index
	 * @param index - index to convert
	 * @return string matching prog.
	 */
	public static String getSessionProgString (int index) {
		switch (index) {
		case AUTOX_SESS_PROG_SELECT_AUTO:
			return AUTOX_SESS_PROG_AUTO;
		case AUTOX_SESS_PROG_SELECT_KDE:
			return AUTOX_SESS_PROG_KDE;
		case AUTOX_SESS_PROG_SELECT_UNITY:
			return AUTOX_SESS_PROG_UNITY;
		case AUTOX_SESS_PROG_SELECT_UNITY2D:
			return AUTOX_SESS_PROG_UNITY2D;
		case AUTOX_SESS_PROG_SELECT_XFCE:
			return AUTOX_SESS_PROG_XFCE;
		case AUTOX_SESS_PROG_SELECT_GNOME:
			return AUTOX_SESS_PROG_GNOME;
		case AUTOX_SESS_PROG_SELECT_GNOMECL:
			return AUTOX_SESS_PROG_GNOMECL;
		case AUTOX_SESS_PROG_SELECT_TRINITY:
			return AUTOX_SESS_PROG_TRINITY;
		case AUTOX_SESS_PROG_SELECT_MATE:
			return AUTOX_SESS_PROG_MATE;
		}
		return "";
	}

	/**
	 * Returns a string matching a command index.
	 * @param command - command to convert
	 * @param opts - options to add to command
	 * @return string matching command.
	 */
	public static String getCommandString (int command, String opts) {
		switch (command) {
		case COMMAND_AUTO_X_CREATE_XVFB:
			return COMMAND_AUTO_X_CREATE_XVFB_STRING + opts;

		case COMMAND_AUTO_X_CREATE_XVNC:
			return COMMAND_AUTO_X_CREATE_XVNC_STRING + opts;

		case COMMAND_AUTO_X_CREATE_XDUMMY:
			return COMMAND_AUTO_X_CREATE_XDUMMY_STRING + opts;

		case COMMAND_AUTO_X_FIND:
			return COMMAND_AUTO_X_FIND_STRING + opts;

		case COMMAND_AUTO_X_SUDO_FIND:
			return COMMAND_AUTO_X_SUDO_FIND_STRING + opts;
		}
		return "";
	}
}
