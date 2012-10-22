package com.iiordanov.bVNC;

/**
 * Keys for intent values
 */
public class VncConstants {
	public static final String CONNECTION = "com.iiordanov.bVNC.CONNECTION";

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

	public final static int COMMAND_AUTO_X_START         = 400;
	public final static int COMMAND_AUTO_X_CREATE_XVFB   = 401;
	public final static int COMMAND_AUTO_X_CREATE_XVNC   = 402;
	public final static int COMMAND_AUTO_X_CREATE_XDUMMY = 403;
	public final static int COMMAND_AUTO_X_FIND          = 404;
	public final static int COMMAND_AUTO_X_CUSTOM        = 406;
	public final static int COMMAND_AUTO_X_END           = 499;

	public final static int COMMAND_AUTO_X_START_MV_DMRC         = 500;
	public final static int COMMAND_AUTO_X_CREATE_XVFB_MV_DMRC   = 501;
	public final static int COMMAND_AUTO_X_CREATE_XVNC_MV_DMRC   = 502;
	public final static int COMMAND_AUTO_X_CREATE_XDUMMY_MV_DMRC = 503;
	public final static int COMMAND_AUTO_X_END_MV_DMRC           = 599;

	public final static int COMMAND_AUTO_X_SUDO_START            = 600;
	public final static int COMMAND_AUTO_X_SUDO_FIND             = 605;
	public final static int COMMAND_AUTO_X_SUDO_END              = 699;
	
	public final static int COMMAND_CUSTOM        = 1000;

	public final static String COMMAND_AUTO_X_CREATE_XVFB_STRING   = "PORT= x11vnc -timeout 60 -create -localhost -nopw";
	public final static String COMMAND_AUTO_X_CREATE_XVNC_STRING   = "PORT= x11vnc -timeout 60 -create -localhost -nopw -xvnc";
	public final static String COMMAND_AUTO_X_CREATE_XDUMMY_STRING = "PORT= x11vnc -timeout 60 -create -localhost -nopw -xdummy";
	public final static String COMMAND_AUTO_X_FIND_STRING          = "PORT= x11vnc -timeout 60 -find   -localhost -nopw";
	public final static String COMMAND_AUTO_X_SUDO_FIND_STRING     = "PORT= sudo x11vnc -timeout 60 -find -localhost -env FD_XDM=1 -nopw";

	public final static String MV_DMRC_AWAY = "[ -O ${HOME}/.dmrc ] && mv ${HOME}/.dmrc ${HOME}/.dmrc.$$ ; ";
	public final static String MV_DMRC_BACK = " ; [ -O ${HOME}/.dmrc.$$ ] && mv ${HOME}/.dmrc.$$ ${HOME}/.dmrc";

	public static boolean isCommandAutoX (int command) {
		return (command > COMMAND_AUTO_X_START && command < COMMAND_AUTO_X_END);
	}
	
	public static boolean isCommandAutoXMvDmrc (int command) {
		return (command > COMMAND_AUTO_X_START_MV_DMRC && command < COMMAND_AUTO_X_END_MV_DMRC);
	}

	public static boolean isCommandAutoXSudo (int command) {
		return (command > COMMAND_AUTO_X_SUDO_START && command < COMMAND_AUTO_X_SUDO_END);
	}

	public static boolean isCommandAnyAutoX (int command) {
		return isCommandAutoX (command) ||
				isCommandAutoXMvDmrc (command) ||
				 isCommandAutoXSudo (command);
	}
	
	/**
	 * Converts an auto_x command index to the equivalent auto_x_..._mv_dmrc index
	 * if it isn't already such an index. Otherwise returns it unaltered.
	 */
	public static int convertAutoXToMvDmrc (int command) {
		if (!isCommandAutoXMvDmrc (command) &&
			 isCommandAutoX (command))
			command += COMMAND_AUTO_X_START_MV_DMRC - COMMAND_AUTO_X_START;
			
		return command;
	}

	/**
	 * Converts an auto_x_..._mv_dmrc command index to the equivalent auto_x index
	 * if it isn't already such an index. Otherwise returns it unaltered.
	 */
	public static int convertAutoXToNotMvDmrc (int command) {
		if (!isCommandAutoX (command) &&
			 isCommandAutoXMvDmrc (command))
			command -= COMMAND_AUTO_X_START_MV_DMRC - COMMAND_AUTO_X_START;
			
		return command;
	}

	/**
	 * Returns a string matching a command index.
	 * @param command - command to convert
	 * @return string matching command.
	 */
	public static String getCommandString (int command, String vars) {
		switch (command) {
		case COMMAND_AUTO_X_CREATE_XVFB:
			return vars + COMMAND_AUTO_X_CREATE_XVFB_STRING;

		case COMMAND_AUTO_X_CREATE_XVNC:
			return vars + COMMAND_AUTO_X_CREATE_XVNC_STRING;
			
		case COMMAND_AUTO_X_CREATE_XDUMMY:
			return vars + COMMAND_AUTO_X_CREATE_XDUMMY_STRING;
			
		case COMMAND_AUTO_X_CREATE_XVFB_MV_DMRC:
			return MV_DMRC_AWAY + vars + COMMAND_AUTO_X_CREATE_XVFB_STRING + MV_DMRC_BACK;

		case COMMAND_AUTO_X_CREATE_XVNC_MV_DMRC:
			return MV_DMRC_AWAY + vars + COMMAND_AUTO_X_CREATE_XVNC_STRING + MV_DMRC_BACK;

		case COMMAND_AUTO_X_CREATE_XDUMMY_MV_DMRC:
			return MV_DMRC_AWAY + vars + COMMAND_AUTO_X_CREATE_XDUMMY_STRING + MV_DMRC_BACK;

		case COMMAND_AUTO_X_FIND:
			return vars + COMMAND_AUTO_X_FIND_STRING;

		case COMMAND_AUTO_X_SUDO_FIND:
			return vars + COMMAND_AUTO_X_SUDO_FIND_STRING;
		}
		return "";
	}
}
