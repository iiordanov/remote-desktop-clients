/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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


package com.undatech.opaque.input;

import android.content.Context;
import android.view.KeyEvent;

public class KeycodeMap {
	final static int ABNT_C1 = 0xC1;
	final static int ABNT_C2 = 0xC2;
	final static int ACCEPT = 0x1E;
	final static int ADD = 0x6B;
	final static int APPS = 0x5D;
	final static int ATTN = 0xF6;
	final static int BACK = 0x08;
	final static int BROWSER_BACK = 0xA6;
	final static int BROWSER_FAVORITES = 0xAB;
	final static int BROWSER_FORWARD = 0xA7;
	final static int BROWSER_HOME = 0xAC;
	final static int BROWSER_REFRESH = 0xA8;
	final static int BROWSER_SEARCH = 0xAA;
	final static int BROWSER_STOP = 0xA9;
	final static int CANCEL = 0x03;
	final static int CAPITAL = 0x14;
	final static int CLEAR = 0x0C;
	final static int CONTROL = 0x11;
	final static int CONVERT = 0x1C;
	final static int CRSEL = 0xF7;
	final static int DECIMAL = 0x6E;
	final static int DELETE = 0x2E;
	final static int DIVIDE = 0x6F;
	final static int DOWN = 0x28;
	final static int END = 0x23;
	final static int EREOF = 0xF9;
	final static int ESCAPE = 0x1B;
	final static int EXECUTE = 0x2B;
	final static int EXSEL = 0xF8;
	final static int EXT_KEY = 0x00000100;
	final static int F1 = 0x70;
	final static int F2 = 0x71;
	final static int F3 = 0x72;
	final static int F4 = 0x73;
	final static int F5 = 0x74;
	final static int F6 = 0x75;
	final static int F7 = 0x76;
	final static int F8 = 0x77;
	final static int F9 = 0x78;
	final static int F10 = 0x79;
	final static int F11 = 0x7A;
	final static int F12 = 0x7B;
	final static int F13 = 0x7C;
	final static int F14 = 0x7D;
	final static int F15 = 0x7E;
	final static int F16 = 0x7F;
	final static int F17 = 0x80;
	final static int F18 = 0x81;
	final static int F19 = 0x82;
	final static int F20 = 0x83;
	final static int F21 = 0x84;
	final static int F22 = 0x85;
	final static int F23 = 0x86;
	final static int F24 = 0x87;
	final static int FINAL = 0x18;
	final static int HANGUEL = 0x15;
	final static int HANGUL = 0x15;
	final static int HANJA = 0x19;
	final static int HELP = 0x2F;
	final static int HOME = 0x24;
	final static int INSERT = 0x2D;
	final static int JUNJA = 0x17;
	final static int KANA = 0x15;
	final static int KANJI = 0x19;
	final static int KEY_0 = 0x30;
	final static int KEY_1 = 0x31;
	final static int KEY_2 = 0x32;
	final static int KEY_3 = 0x33;
	final static int KEY_4 = 0x34;
	final static int KEY_5 = 0x35;
	final static int KEY_6 = 0x36;
	final static int KEY_7 = 0x37;
	final static int KEY_8 = 0x38;
	final static int KEY_9 = 0x39;
	final static int KEY_A = 0x41;
	final static int KEY_B = 0x42;
	final static int KEY_C = 0x43;
	final static int KEY_D = 0x44;
	final static int KEY_E = 0x45;
	final static int KEY_F = 0x46;
	final static int KEY_G = 0x47;
	final static int KEY_H = 0x48;
	final static int KEY_I = 0x49;
	final static int KEY_J = 0x4A;
	final static int KEY_K = 0x4B;
	final static int KEY_L = 0x4C;
	final static int KEY_M = 0x4D;
	final static int KEY_N = 0x4E;
	final static int KEY_O = 0x4F;
	final static int KEY_P = 0x50;
	final static int KEY_Q = 0x51;
	final static int KEY_R = 0x52;
	final static int KEY_S = 0x53;
	final static int KEY_T = 0x54;
	final static int KEY_U = 0x55;
	final static int KEY_V = 0x56;
	final static int KEY_W = 0x57;
	final static int KEY_X = 0x58;
	final static int KEY_Y = 0x59;
	final static int KEY_Z = 0x5A;
	final static int LAUNCH_APP1 = 0xB6;
	final static int LAUNCH_APP2 = 0xB7;
	final static int LAUNCH_MAIL = 0xB4;
	final static int LAUNCH_MEDIA_SELECT = 0xB5;
	final static int LBUTTON = 0x01;
	final static int LCONTROL = 0xA2;
	final static int LEFT = 0x25;
	final static int LMENU = 0xA4;
	final static int LSHIFT = 0xA0;
	final static int LWIN = 0x5B;
	final static int MBUTTON = 0x04;
	final static int MEDIA_NEXT_TRACK = 0xB0;
	final static int MEDIA_PLAY_PAUSE = 0xB3;
	final static int MEDIA_PREV_TRACK = 0xB1;
	final static int MEDIA_STOP = 0xB2;
	final static int MENU = 0x12;
	final static int MODECHANGE = 0x1F;
	final static int MULTIPLY = 0x6A;
	final static int NEXT = 0x22;
	final static int NONAME = 0xFC;
	final static int NONCONVERT = 0x1D;
	final static int NUMLOCK = 0x90;
	final static int NUMPAD0 = 0x60;
	final static int NUMPAD1 = 0x61;
	final static int NUMPAD2 = 0x62;
	final static int NUMPAD3 = 0x63;
	final static int NUMPAD4 = 0x64;
	final static int NUMPAD5 = 0x65;
	final static int NUMPAD6 = 0x66;
	final static int NUMPAD7 = 0x67;
	final static int NUMPAD8 = 0x68;
	final static int NUMPAD9 = 0x69;
	final static int OEM_102 = 0xE2;
	final static int OEM_1 = 0xBA;
	final static int OEM_2 = 0xBF;
	final static int OEM_3 = 0xC0;
	final static int OEM_4 = 0xDB;
	final static int OEM_5 = 0xDC;
	final static int OEM_6 = 0xDD;
	final static int OEM_7 = 0xDE;
	final static int OEM_8 = 0xDF;
	final static int OEM_CLEAR = 0xFE;
	final static int OEM_COMMA = 0xBC;
	final static int OEM_EQUALS = 0xBB;	
	final static int OEM_MINUS = 0xBD;
	final static int OEM_PERIOD = 0xBE;
	final static int OEM_SEMICOLON = 0xBA;
	final static int PA1 = 0xFD;
	final static int PACKET = 0xE7;
	final static int PAUSE = 0x13;
	final static int PLAY = 0xFA;
	final static int PRINT = 0x2A;
	final static int PRIOR = 0x21;
	final static int PROCESSKEY = 0xE5;
	final static int RBUTTON = 0x02;
	final static int RCONTROL = 0xA3;
	final static int RETURN = 0x0D;
	final static int RIGHT = 0x27;
	final static int RMENU = 0xA5;
	final static int RSHIFT = 0xA1;
	final static int RWIN = 0x5C;
	final static int SCROLL = 0x91;
	final static int SELECT = 0x29;
	final static int SEPARATOR = 0x6C;
	final static int SHIFT = 0x10;
	final static int SLEEP = 0x5F;
	final static int SNAPSHOT = 0x2C;
	final static int SPACE = 0x20;
	final static int SUBTRACT = 0x6D;
	final static int TAB = 0x09;
	final static int UP = 0x26;
	final static int VOLUME_DOWN = 0xAE;
	final static int VOLUME_MUTE = 0xAD;
	final static int VOLUME_UP = 0xAF;
	final static int XBUTTON1 = 0x05;
	final static int XBUTTON2 = 0x06;
	final static int ZOOM = 0xFB;

	private static int[] keyCodeToVirtualKey;

	public KeycodeMap(Context context) {
		keyCodeToVirtualKey = new int[256];
		keyCodeToVirtualKey[92] = PRIOR | EXT_KEY;
		keyCodeToVirtualKey[93] = NEXT | EXT_KEY;
		keyCodeToVirtualKey[111] = ESCAPE;
		keyCodeToVirtualKey[112] = DELETE | EXT_KEY;
		keyCodeToVirtualKey[113] = LCONTROL;
		keyCodeToVirtualKey[114] = RCONTROL;
		keyCodeToVirtualKey[115] = CAPITAL;
		keyCodeToVirtualKey[116] = SCROLL;
		keyCodeToVirtualKey[120] = PRINT;
		keyCodeToVirtualKey[121] = PAUSE;
		keyCodeToVirtualKey[122] = HOME | EXT_KEY;
		keyCodeToVirtualKey[123] = END | EXT_KEY;
		keyCodeToVirtualKey[124] = INSERT | EXT_KEY;
		keyCodeToVirtualKey[131] = F1;
		keyCodeToVirtualKey[132] = F2;
		keyCodeToVirtualKey[133] = F3;
		keyCodeToVirtualKey[134] = F4;
		keyCodeToVirtualKey[135] = F5;
		keyCodeToVirtualKey[136] = F6;
		keyCodeToVirtualKey[137] = F7;
		keyCodeToVirtualKey[138] = F8;
		keyCodeToVirtualKey[139] = F9;
		keyCodeToVirtualKey[140] = F10;
		keyCodeToVirtualKey[141] = F11;
		keyCodeToVirtualKey[142] = F12;
		keyCodeToVirtualKey[143] = NUMLOCK;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_APOSTROPHE] = OEM_7;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_BACK] = ESCAPE;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_BACKSLASH] = OEM_5;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_COMMA] = OEM_COMMA;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_DEL] = BACK;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_DPAD_DOWN] = DOWN | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_DPAD_LEFT] = LEFT | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_DPAD_RIGHT] = RIGHT | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_DPAD_UP] = UP | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_ENTER] = RETURN;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_EQUALS] = OEM_EQUALS;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F1] = F1;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F2] = F2;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F3] = F3;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F4] = F4;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F5] = F5;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F6] = F6;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F7] = F7;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F8] = F8;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F9] = F9;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F10] = F10;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F11] = F11;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F12] = F12;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_FORWARD_DEL] = DELETE | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_GRAVE] = OEM_3;	
		keyCodeToVirtualKey[KeyEvent.KEYCODE_INSERT] = INSERT | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_LEFT_BRACKET] = OEM_4;		
		keyCodeToVirtualKey[KeyEvent.KEYCODE_MINUS] = OEM_MINUS;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_MOVE_END] = END | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_MOVE_HOME] = HOME | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUM_LOCK] = NUMLOCK;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_0] = NUMPAD0;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_1] = NUMPAD1;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_2] = NUMPAD2;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_3] = NUMPAD3;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_4] = NUMPAD4;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_5] = NUMPAD5;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_6] = NUMPAD6;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_7] = NUMPAD7;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_8] = NUMPAD8;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_9] = NUMPAD9;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_ADD] = ADD;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_DIVIDE] = OEM_2;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_DOT] = DECIMAL;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_ENTER] = RETURN;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_EQUALS] = OEM_EQUALS;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_MULTIPLY] = MULTIPLY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_NUMPAD_SUBTRACT] = SUBTRACT;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_PAGE_DOWN] = NEXT | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_PAGE_UP] = PRIOR | EXT_KEY;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_PERIOD] = OEM_PERIOD;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_PLUS] = ADD;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_RIGHT_BRACKET] = OEM_6;		
		keyCodeToVirtualKey[KeyEvent.KEYCODE_SEMICOLON] = OEM_SEMICOLON;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_SHIFT_LEFT] = LSHIFT;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_SHIFT_RIGHT] = RSHIFT;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_SLASH] = OEM_2;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_SPACE] = SPACE;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_SYSRQ] = PRINT;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_TAB] = TAB;

		keyCodeToVirtualKey[KeyEvent.KEYCODE_0] = KEY_0;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_1] = KEY_1;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_2] = KEY_2;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_3] = KEY_3;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_4] = KEY_4;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_5] = KEY_5;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_6] = KEY_6;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_7] = KEY_7;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_8] = KEY_8;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_9] = KEY_9;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_A] = KEY_A;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_B] = KEY_B;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_C] = KEY_C;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_D] = KEY_D;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_E] = KEY_E;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_F] = KEY_F;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_G] = KEY_G;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_H] = KEY_H;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_I] = KEY_I;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_J] = KEY_J;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_K] = KEY_K;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_L] = KEY_L;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_M] = KEY_M;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_N] = KEY_N;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_O] = KEY_O;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_P] = KEY_P;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_Q] = KEY_Q;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_R] = KEY_R;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_S] = KEY_S;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_T] = KEY_T;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_U] = KEY_U;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_V] = KEY_V;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_W] = KEY_W;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_X] = KEY_X;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_Y] = KEY_Y;
		keyCodeToVirtualKey[KeyEvent.KEYCODE_Z] = KEY_Z;
//		keymapAndroid[KeyEvent.KEYCODE_ALT_LEFT] = LMENU;
//		keymapAndroid[KeyEvent.KEYCODE_ALT_RIGHT] = RMENU;
	}
	
	public int getVirtKey(int androidKeyCode) {
		if(androidKeyCode >= 0 && androidKeyCode <= 0xFF) {
			return keyCodeToVirtualKey[androidKeyCode];
		}
		return 0;
	}
}

