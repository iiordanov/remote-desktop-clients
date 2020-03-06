/**
* This module converts keysym values into the corresponding ISO 10646
* (UCS, Unicode) values and back.
*
* Modified from the public domain program keysym2ucs.c, and Mark Doffman's
* pyatspi2 project.
*
* The array keysymtab contains pairs of X11 keysym values for graphical
* characters and the corresponding Unicode value. The function
* keysym2ucs() maps a keysym onto a Unicode value using a binary search,
* therefore keysymtab[] must remain SORTED by keysym value.
*
* The array keysymtabbyucs, on the other hand, is sorted by unicode value.
*
* We allow to represent any UCS character in the range U-00000000 to
* U-00FFFFFF by a keysym value in the range 0x01000000 to 0x01ffffff.
* This admittedly does not cover the entire 31-bit space of UCS, but
* it does cover all of the characters up to U-10FFFF, which can be
* represented by UTF-16, and more, and it is very unlikely that higher
* UCS codes will ever be assigned by ISO. So to get Unicode character
* U+ABCD you can directly use keysym 0x0100abcd.
*
* NOTE: The comments in the table below contain the actual character
* encoded in UTF-8, so for viewing and editing best use an editor in
* UTF-8 mode.
*
* Authors: Iordan Iordanov, 2020
*
*               Markus G. Kuhn <http://www.cl.cam.ac.uk/~mgk25/>,
*               University of Cambridge, April 2001
*
* Special thanks to Richard Verhoeven <river@win.tue.nl> for preparing
* an initial draft of the mapping table.
*
* This software is in the public domain. Share and enjoy!
*/

#ifndef ucs2xkeysym_h
#define ucs2xkeysym_h

#include <stdio.h>

int ucs2keysym (const unsigned char *c);
long keysym2ucs(long keysym);

#endif /* ucs2xkeysym_h */
