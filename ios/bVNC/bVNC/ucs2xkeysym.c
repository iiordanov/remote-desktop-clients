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
* Authors: Iordan Iordanov, 2012
*
*               Markus G. Kuhn <http://www.cl.cam.ac.uk/~mgk25/>,
*               University of Cambridge, April 2001
*
* Special thanks to Richard Verhoeven <river@win.tue.nl> for preparing
* an initial draft of the mapping table.
*
* This software is in the public domain. Share and enjoy!
*/

#include "ucs2xkeysym.h"

struct codepair {
    unsigned short keysym;
    unsigned short ucs;
} keysymtab[] = {
    { 0x01a1, 0x0104 }, /*                     Aogonek Ą LATIN CAPITAL LETTER A WITH OGONEK */
    { 0x01a2, 0x02d8 }, /*                       breve ˘ BREVE */
    { 0x01a3, 0x0141 }, /*                     Lstroke Ł LATIN CAPITAL LETTER L WITH STROKE */
    { 0x01a5, 0x013d }, /*                      Lcaron Ľ LATIN CAPITAL LETTER L WITH CARON */
    { 0x01a6, 0x015a }, /*                      Sacute Ś LATIN CAPITAL LETTER S WITH ACUTE */
    { 0x01a9, 0x0160 }, /*                      Scaron Š LATIN CAPITAL LETTER S WITH CARON */
    { 0x01aa, 0x015e }, /*                    Scedilla Ş LATIN CAPITAL LETTER S WITH CEDILLA */
    { 0x01ab, 0x0164 }, /*                      Tcaron Ť LATIN CAPITAL LETTER T WITH CARON */
    { 0x01ac, 0x0179 }, /*                      Zacute Ź LATIN CAPITAL LETTER Z WITH ACUTE */
    { 0x01ae, 0x017d }, /*                      Zcaron Ž LATIN CAPITAL LETTER Z WITH CARON */
    { 0x01af, 0x017b }, /*                   Zabovedot Ż LATIN CAPITAL LETTER Z WITH DOT ABOVE */
    { 0x01b1, 0x0105 }, /*                     aogonek ą LATIN SMALL LETTER A WITH OGONEK */
    { 0x01b2, 0x02db }, /*                      ogonek ˛ OGONEK */
    { 0x01b3, 0x0142 }, /*                     lstroke ł LATIN SMALL LETTER L WITH STROKE */
    { 0x01b5, 0x013e }, /*                      lcaron ľ LATIN SMALL LETTER L WITH CARON */
    { 0x01b6, 0x015b }, /*                      sacute ś LATIN SMALL LETTER S WITH ACUTE */
    { 0x01b7, 0x02c7 }, /*                       caron ˇ CARON */
    { 0x01b9, 0x0161 }, /*                      scaron š LATIN SMALL LETTER S WITH CARON */
    { 0x01ba, 0x015f }, /*                    scedilla ş LATIN SMALL LETTER S WITH CEDILLA */
    { 0x01bb, 0x0165 }, /*                      tcaron ť LATIN SMALL LETTER T WITH CARON */
    { 0x01bc, 0x017a }, /*                      zacute ź LATIN SMALL LETTER Z WITH ACUTE */
    { 0x01bd, 0x02dd }, /*                 doubleacute ˝ DOUBLE ACUTE ACCENT */
    { 0x01be, 0x017e }, /*                      zcaron ž LATIN SMALL LETTER Z WITH CARON */
    { 0x01bf, 0x017c }, /*                   zabovedot ż LATIN SMALL LETTER Z WITH DOT ABOVE */
    { 0x01c0, 0x0154 }, /*                      Racute Ŕ LATIN CAPITAL LETTER R WITH ACUTE */
    { 0x01c3, 0x0102 }, /*                      Abreve Ă LATIN CAPITAL LETTER A WITH BREVE */
    { 0x01c5, 0x0139 }, /*                      Lacute Ĺ LATIN CAPITAL LETTER L WITH ACUTE */
    { 0x01c6, 0x0106 }, /*                      Cacute Ć LATIN CAPITAL LETTER C WITH ACUTE */
    { 0x01c8, 0x010c }, /*                      Ccaron Č LATIN CAPITAL LETTER C WITH CARON */
    { 0x01ca, 0x0118 }, /*                     Eogonek Ę LATIN CAPITAL LETTER E WITH OGONEK */
    { 0x01cc, 0x011a }, /*                      Ecaron Ě LATIN CAPITAL LETTER E WITH CARON */
    { 0x01cf, 0x010e }, /*                      Dcaron Ď LATIN CAPITAL LETTER D WITH CARON */
    { 0x01d0, 0x0110 }, /*                     Dstroke Đ LATIN CAPITAL LETTER D WITH STROKE */
    { 0x01d1, 0x0143 }, /*                      Nacute Ń LATIN CAPITAL LETTER N WITH ACUTE */
    { 0x01d2, 0x0147 }, /*                      Ncaron Ň LATIN CAPITAL LETTER N WITH CARON */
    { 0x01d5, 0x0150 }, /*                Odoubleacute Ő LATIN CAPITAL LETTER O WITH DOUBLE ACUTE */
    { 0x01d8, 0x0158 }, /*                      Rcaron Ř LATIN CAPITAL LETTER R WITH CARON */
    { 0x01d9, 0x016e }, /*                       Uring Ů LATIN CAPITAL LETTER U WITH RING ABOVE */
    { 0x01db, 0x0170 }, /*                Udoubleacute Ű LATIN CAPITAL LETTER U WITH DOUBLE ACUTE */
    { 0x01de, 0x0162 }, /*                    Tcedilla Ţ LATIN CAPITAL LETTER T WITH CEDILLA */
    { 0x01e0, 0x0155 }, /*                      racute ŕ LATIN SMALL LETTER R WITH ACUTE */
    { 0x01e3, 0x0103 }, /*                      abreve ă LATIN SMALL LETTER A WITH BREVE */
    { 0x01e5, 0x013a }, /*                      lacute ĺ LATIN SMALL LETTER L WITH ACUTE */
    { 0x01e6, 0x0107 }, /*                      cacute ć LATIN SMALL LETTER C WITH ACUTE */
    { 0x01e8, 0x010d }, /*                      ccaron č LATIN SMALL LETTER C WITH CARON */
    { 0x01ea, 0x0119 }, /*                     eogonek ę LATIN SMALL LETTER E WITH OGONEK */
    { 0x01ec, 0x011b }, /*                      ecaron ě LATIN SMALL LETTER E WITH CARON */
    { 0x01ef, 0x010f }, /*                      dcaron ď LATIN SMALL LETTER D WITH CARON */
    { 0x01f0, 0x0111 }, /*                     dstroke đ LATIN SMALL LETTER D WITH STROKE */
    { 0x01f1, 0x0144 }, /*                      nacute ń LATIN SMALL LETTER N WITH ACUTE */
    { 0x01f2, 0x0148 }, /*                      ncaron ň LATIN SMALL LETTER N WITH CARON */
    { 0x01f5, 0x0151 }, /*                odoubleacute ő LATIN SMALL LETTER O WITH DOUBLE ACUTE */
    { 0x01f8, 0x0159 }, /*                      rcaron ř LATIN SMALL LETTER R WITH CARON */
    { 0x01f9, 0x016f }, /*                       uring ů LATIN SMALL LETTER U WITH RING ABOVE */
    { 0x01fb, 0x0171 }, /*                udoubleacute ű LATIN SMALL LETTER U WITH DOUBLE ACUTE */
    { 0x01fe, 0x0163 }, /*                    tcedilla ţ LATIN SMALL LETTER T WITH CEDILLA */
    { 0x01ff, 0x02d9 }, /*                    abovedot ˙ DOT ABOVE */
    { 0x02a1, 0x0126 }, /*                     Hstroke Ħ LATIN CAPITAL LETTER H WITH STROKE */
    { 0x02a6, 0x0124 }, /*                 Hcircumflex Ĥ LATIN CAPITAL LETTER H WITH CIRCUMFLEX */
    { 0x02a9, 0x0130 }, /*                   Iabovedot İ LATIN CAPITAL LETTER I WITH DOT ABOVE */
    { 0x02ab, 0x011e }, /*                      Gbreve Ğ LATIN CAPITAL LETTER G WITH BREVE */
    { 0x02ac, 0x0134 }, /*                 Jcircumflex Ĵ LATIN CAPITAL LETTER J WITH CIRCUMFLEX */
    { 0x02b1, 0x0127 }, /*                     hstroke ħ LATIN SMALL LETTER H WITH STROKE */
    { 0x02b6, 0x0125 }, /*                 hcircumflex ĥ LATIN SMALL LETTER H WITH CIRCUMFLEX */
    { 0x02b9, 0x0131 }, /*                    idotless ı LATIN SMALL LETTER DOTLESS I */
    { 0x02bb, 0x011f }, /*                      gbreve ğ LATIN SMALL LETTER G WITH BREVE */
    { 0x02bc, 0x0135 }, /*                 jcircumflex ĵ LATIN SMALL LETTER J WITH CIRCUMFLEX */
    { 0x02c5, 0x010a }, /*                   Cabovedot Ċ LATIN CAPITAL LETTER C WITH DOT ABOVE */
    { 0x02c6, 0x0108 }, /*                 Ccircumflex Ĉ LATIN CAPITAL LETTER C WITH CIRCUMFLEX */
    { 0x02d5, 0x0120 }, /*                   Gabovedot Ġ LATIN CAPITAL LETTER G WITH DOT ABOVE */
    { 0x02d8, 0x011c }, /*                 Gcircumflex Ĝ LATIN CAPITAL LETTER G WITH CIRCUMFLEX */
    { 0x02dd, 0x016c }, /*                      Ubreve Ŭ LATIN CAPITAL LETTER U WITH BREVE */
    { 0x02de, 0x015c }, /*                 Scircumflex Ŝ LATIN CAPITAL LETTER S WITH CIRCUMFLEX */
    { 0x02e5, 0x010b }, /*                   cabovedot ċ LATIN SMALL LETTER C WITH DOT ABOVE */
    { 0x02e6, 0x0109 }, /*                 ccircumflex ĉ LATIN SMALL LETTER C WITH CIRCUMFLEX */
    { 0x02f5, 0x0121 }, /*                   gabovedot ġ LATIN SMALL LETTER G WITH DOT ABOVE */
    { 0x02f8, 0x011d }, /*                 gcircumflex ĝ LATIN SMALL LETTER G WITH CIRCUMFLEX */
    { 0x02fd, 0x016d }, /*                      ubreve ŭ LATIN SMALL LETTER U WITH BREVE */
    { 0x02fe, 0x015d }, /*                 scircumflex ŝ LATIN SMALL LETTER S WITH CIRCUMFLEX */
    { 0x03a2, 0x0138 }, /*                         kra ĸ LATIN SMALL LETTER KRA */
    { 0x03a3, 0x0156 }, /*                    Rcedilla Ŗ LATIN CAPITAL LETTER R WITH CEDILLA */
    { 0x03a5, 0x0128 }, /*                      Itilde Ĩ LATIN CAPITAL LETTER I WITH TILDE */
    { 0x03a6, 0x013b }, /*                    Lcedilla Ļ LATIN CAPITAL LETTER L WITH CEDILLA */
    { 0x03aa, 0x0112 }, /*                     Emacron Ē LATIN CAPITAL LETTER E WITH MACRON */
    { 0x03ab, 0x0122 }, /*                    Gcedilla Ģ LATIN CAPITAL LETTER G WITH CEDILLA */
    { 0x03ac, 0x0166 }, /*                      Tslash Ŧ LATIN CAPITAL LETTER T WITH STROKE */
    { 0x03b3, 0x0157 }, /*                    rcedilla ŗ LATIN SMALL LETTER R WITH CEDILLA */
    { 0x03b5, 0x0129 }, /*                      itilde ĩ LATIN SMALL LETTER I WITH TILDE */
    { 0x03b6, 0x013c }, /*                    lcedilla ļ LATIN SMALL LETTER L WITH CEDILLA */
    { 0x03ba, 0x0113 }, /*                     emacron ē LATIN SMALL LETTER E WITH MACRON */
    { 0x03bb, 0x0123 }, /*                    gcedilla ģ LATIN SMALL LETTER G WITH CEDILLA */
    { 0x03bc, 0x0167 }, /*                      tslash ŧ LATIN SMALL LETTER T WITH STROKE */
    { 0x03bd, 0x014a }, /*                         ENG Ŋ LATIN CAPITAL LETTER ENG */
    { 0x03bf, 0x014b }, /*                         eng ŋ LATIN SMALL LETTER ENG */
    { 0x03c0, 0x0100 }, /*                     Amacron Ā LATIN CAPITAL LETTER A WITH MACRON */
    { 0x03c7, 0x012e }, /*                     Iogonek Į LATIN CAPITAL LETTER I WITH OGONEK */
    { 0x03cc, 0x0116 }, /*                   Eabovedot Ė LATIN CAPITAL LETTER E WITH DOT ABOVE */
    { 0x03cf, 0x012a }, /*                     Imacron Ī LATIN CAPITAL LETTER I WITH MACRON */
    { 0x03d1, 0x0145 }, /*                    Ncedilla Ņ LATIN CAPITAL LETTER N WITH CEDILLA */
    { 0x03d2, 0x014c }, /*                     Omacron Ō LATIN CAPITAL LETTER O WITH MACRON */
    { 0x03d3, 0x0136 }, /*                    Kcedilla Ķ LATIN CAPITAL LETTER K WITH CEDILLA */
    { 0x03d9, 0x0172 }, /*                     Uogonek Ų LATIN CAPITAL LETTER U WITH OGONEK */
    { 0x03dd, 0x0168 }, /*                      Utilde Ũ LATIN CAPITAL LETTER U WITH TILDE */
    { 0x03de, 0x016a }, /*                     Umacron Ū LATIN CAPITAL LETTER U WITH MACRON */
    { 0x03e0, 0x0101 }, /*                     amacron ā LATIN SMALL LETTER A WITH MACRON */
    { 0x03e7, 0x012f }, /*                     iogonek į LATIN SMALL LETTER I WITH OGONEK */
    { 0x03ec, 0x0117 }, /*                   eabovedot ė LATIN SMALL LETTER E WITH DOT ABOVE */
    { 0x03ef, 0x012b }, /*                     imacron ī LATIN SMALL LETTER I WITH MACRON */
    { 0x03f1, 0x0146 }, /*                    ncedilla ņ LATIN SMALL LETTER N WITH CEDILLA */
    { 0x03f2, 0x014d }, /*                     omacron ō LATIN SMALL LETTER O WITH MACRON */
    { 0x03f3, 0x0137 }, /*                    kcedilla ķ LATIN SMALL LETTER K WITH CEDILLA */
    { 0x03f9, 0x0173 }, /*                     uogonek ų LATIN SMALL LETTER U WITH OGONEK */
    { 0x03fd, 0x0169 }, /*                      utilde ũ LATIN SMALL LETTER U WITH TILDE */
    { 0x03fe, 0x016b }, /*                     umacron ū LATIN SMALL LETTER U WITH MACRON */
    { 0x047e, 0x203e }, /*                    overline ‾ OVERLINE */
    { 0x04a1, 0x3002 }, /*               kana_fullstop 。 IDEOGRAPHIC FULL STOP */
    { 0x04a2, 0x300c }, /*         kana_openingbracket 「 LEFT CORNER BRACKET */
    { 0x04a3, 0x300d }, /*         kana_closingbracket 」 RIGHT CORNER BRACKET */
    { 0x04a4, 0x3001 }, /*                  kana_comma 、 IDEOGRAPHIC COMMA */
    { 0x04a5, 0x30fb }, /*            kana_conjunctive ・ KATAKANA MIDDLE DOT */
    { 0x04a6, 0x30f2 }, /*                     kana_WO ヲ KATAKANA LETTER WO */
    { 0x04a7, 0x30a1 }, /*                      kana_a ァ KATAKANA LETTER SMALL A */
    { 0x04a8, 0x30a3 }, /*                      kana_i ィ KATAKANA LETTER SMALL I */
    { 0x04a9, 0x30a5 }, /*                      kana_u ゥ KATAKANA LETTER SMALL U */
    { 0x04aa, 0x30a7 }, /*                      kana_e ェ KATAKANA LETTER SMALL E */
    { 0x04ab, 0x30a9 }, /*                      kana_o ォ KATAKANA LETTER SMALL O */
    { 0x04ac, 0x30e3 }, /*                     kana_ya ャ KATAKANA LETTER SMALL YA */
    { 0x04ad, 0x30e5 }, /*                     kana_yu ュ KATAKANA LETTER SMALL YU */
    { 0x04ae, 0x30e7 }, /*                     kana_yo ョ KATAKANA LETTER SMALL YO */
    { 0x04af, 0x30c3 }, /*                    kana_tsu ッ KATAKANA LETTER SMALL TU */
    { 0x04b0, 0x30fc }, /*              prolongedsound ー KATAKANA-HIRAGANA PROLONGED SOUND MARK */
    { 0x04b1, 0x30a2 }, /*                      kana_A ア KATAKANA LETTER A */
    { 0x04b2, 0x30a4 }, /*                      kana_I イ KATAKANA LETTER I */
    { 0x04b3, 0x30a6 }, /*                      kana_U ウ KATAKANA LETTER U */
    { 0x04b4, 0x30a8 }, /*                      kana_E エ KATAKANA LETTER E */
    { 0x04b5, 0x30aa }, /*                      kana_O オ KATAKANA LETTER O */
    { 0x04b6, 0x30ab }, /*                     kana_KA カ KATAKANA LETTER KA */
    { 0x04b7, 0x30ad }, /*                     kana_KI キ KATAKANA LETTER KI */
    { 0x04b8, 0x30af }, /*                     kana_KU ク KATAKANA LETTER KU */
    { 0x04b9, 0x30b1 }, /*                     kana_KE ケ KATAKANA LETTER KE */
    { 0x04ba, 0x30b3 }, /*                     kana_KO コ KATAKANA LETTER KO */
    { 0x04bb, 0x30b5 }, /*                     kana_SA サ KATAKANA LETTER SA */
    { 0x04bc, 0x30b7 }, /*                    kana_SHI シ KATAKANA LETTER SI */
    { 0x04bd, 0x30b9 }, /*                     kana_SU ス KATAKANA LETTER SU */
    { 0x04be, 0x30bb }, /*                     kana_SE セ KATAKANA LETTER SE */
    { 0x04bf, 0x30bd }, /*                     kana_SO ソ KATAKANA LETTER SO */
    { 0x04c0, 0x30bf }, /*                     kana_TA タ KATAKANA LETTER TA */
    { 0x04c1, 0x30c1 }, /*                    kana_CHI チ KATAKANA LETTER TI */
    { 0x04c2, 0x30c4 }, /*                    kana_TSU ツ KATAKANA LETTER TU */
    { 0x04c3, 0x30c6 }, /*                     kana_TE テ KATAKANA LETTER TE */
    { 0x04c4, 0x30c8 }, /*                     kana_TO ト KATAKANA LETTER TO */
    { 0x04c5, 0x30ca }, /*                     kana_NA ナ KATAKANA LETTER NA */
    { 0x04c6, 0x30cb }, /*                     kana_NI ニ KATAKANA LETTER NI */
    { 0x04c7, 0x30cc }, /*                     kana_NU ヌ KATAKANA LETTER NU */
    { 0x04c8, 0x30cd }, /*                     kana_NE ネ KATAKANA LETTER NE */
    { 0x04c9, 0x30ce }, /*                     kana_NO ノ KATAKANA LETTER NO */
    { 0x04ca, 0x30cf }, /*                     kana_HA ハ KATAKANA LETTER HA */
    { 0x04cb, 0x30d2 }, /*                     kana_HI ヒ KATAKANA LETTER HI */
    { 0x04cc, 0x30d5 }, /*                     kana_FU フ KATAKANA LETTER HU */
    { 0x04cd, 0x30d8 }, /*                     kana_HE ヘ KATAKANA LETTER HE */
    { 0x04ce, 0x30db }, /*                     kana_HO ホ KATAKANA LETTER HO */
    { 0x04cf, 0x30de }, /*                     kana_MA マ KATAKANA LETTER MA */
    { 0x04d0, 0x30df }, /*                     kana_MI ミ KATAKANA LETTER MI */
    { 0x04d1, 0x30e0 }, /*                     kana_MU ム KATAKANA LETTER MU */
    { 0x04d2, 0x30e1 }, /*                     kana_ME メ KATAKANA LETTER ME */
    { 0x04d3, 0x30e2 }, /*                     kana_MO モ KATAKANA LETTER MO */
    { 0x04d4, 0x30e4 }, /*                     kana_YA ヤ KATAKANA LETTER YA */
    { 0x04d5, 0x30e6 }, /*                     kana_YU ユ KATAKANA LETTER YU */
    { 0x04d6, 0x30e8 }, /*                     kana_YO ヨ KATAKANA LETTER YO */
    { 0x04d7, 0x30e9 }, /*                     kana_RA ラ KATAKANA LETTER RA */
    { 0x04d8, 0x30ea }, /*                     kana_RI リ KATAKANA LETTER RI */
    { 0x04d9, 0x30eb }, /*                     kana_RU ル KATAKANA LETTER RU */
    { 0x04da, 0x30ec }, /*                     kana_RE レ KATAKANA LETTER RE */
    { 0x04db, 0x30ed }, /*                     kana_RO ロ KATAKANA LETTER RO */
    { 0x04dc, 0x30ef }, /*                     kana_WA ワ KATAKANA LETTER WA */
    { 0x04dd, 0x30f3 }, /*                      kana_N ン KATAKANA LETTER N */
    { 0x04de, 0x309b }, /*                 voicedsound ゛ KATAKANA-HIRAGANA VOICED SOUND MARK */
    { 0x04df, 0x309c }, /*             semivoicedsound ゜ KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK */
    { 0x05ac, 0x060c }, /*                Arabic_comma ، ARABIC COMMA */
    { 0x05bb, 0x061b }, /*            Arabic_semicolon ؛ ARABIC SEMICOLON */
    { 0x05bf, 0x061f }, /*        Arabic_question_mark ؟ ARABIC QUESTION MARK */
    { 0x05c1, 0x0621 }, /*                Arabic_hamza ء ARABIC LETTER HAMZA */
    { 0x05c2, 0x0622 }, /*          Arabic_maddaonalef آ ARABIC LETTER ALEF WITH MADDA ABOVE */
    { 0x05c3, 0x0623 }, /*          Arabic_hamzaonalef أ ARABIC LETTER ALEF WITH HAMZA ABOVE */
    { 0x05c4, 0x0624 }, /*           Arabic_hamzaonwaw ؤ ARABIC LETTER WAW WITH HAMZA ABOVE */
    { 0x05c5, 0x0625 }, /*       Arabic_hamzaunderalef إ ARABIC LETTER ALEF WITH HAMZA BELOW */
    { 0x05c6, 0x0626 }, /*           Arabic_hamzaonyeh ئ ARABIC LETTER YEH WITH HAMZA ABOVE */
    { 0x05c7, 0x0627 }, /*                 Arabic_alef ا ARABIC LETTER ALEF */
    { 0x05c8, 0x0628 }, /*                  Arabic_beh ب ARABIC LETTER BEH */
    { 0x05c9, 0x0629 }, /*           Arabic_tehmarbuta ة ARABIC LETTER TEH MARBUTA */
    { 0x05ca, 0x062a }, /*                  Arabic_teh ت ARABIC LETTER TEH */
    { 0x05cb, 0x062b }, /*                 Arabic_theh ث ARABIC LETTER THEH */
    { 0x05cc, 0x062c }, /*                 Arabic_jeem ج ARABIC LETTER JEEM */
    { 0x05cd, 0x062d }, /*                  Arabic_hah ح ARABIC LETTER HAH */
    { 0x05ce, 0x062e }, /*                 Arabic_khah خ ARABIC LETTER KHAH */
    { 0x05cf, 0x062f }, /*                  Arabic_dal د ARABIC LETTER DAL */
    { 0x05d0, 0x0630 }, /*                 Arabic_thal ذ ARABIC LETTER THAL */
    { 0x05d1, 0x0631 }, /*                   Arabic_ra ر ARABIC LETTER REH */
    { 0x05d2, 0x0632 }, /*                 Arabic_zain ز ARABIC LETTER ZAIN */
    { 0x05d3, 0x0633 }, /*                 Arabic_seen س ARABIC LETTER SEEN */
    { 0x05d4, 0x0634 }, /*                Arabic_sheen ش ARABIC LETTER SHEEN */
    { 0x05d5, 0x0635 }, /*                  Arabic_sad ص ARABIC LETTER SAD */
    { 0x05d6, 0x0636 }, /*                  Arabic_dad ض ARABIC LETTER DAD */
    { 0x05d7, 0x0637 }, /*                  Arabic_tah ط ARABIC LETTER TAH */
    { 0x05d8, 0x0638 }, /*                  Arabic_zah ظ ARABIC LETTER ZAH */
    { 0x05d9, 0x0639 }, /*                  Arabic_ain ع ARABIC LETTER AIN */
    { 0x05da, 0x063a }, /*                Arabic_ghain غ ARABIC LETTER GHAIN */
    { 0x05e0, 0x0640 }, /*              Arabic_tatweel ـ ARABIC TATWEEL */
    { 0x05e1, 0x0641 }, /*                  Arabic_feh ف ARABIC LETTER FEH */
    { 0x05e2, 0x0642 }, /*                  Arabic_qaf ق ARABIC LETTER QAF */
    { 0x05e3, 0x0643 }, /*                  Arabic_kaf ك ARABIC LETTER KAF */
    { 0x05e4, 0x0644 }, /*                  Arabic_lam ل ARABIC LETTER LAM */
    { 0x05e5, 0x0645 }, /*                 Arabic_meem م ARABIC LETTER MEEM */
    { 0x05e6, 0x0646 }, /*                 Arabic_noon ن ARABIC LETTER NOON */
    { 0x05e7, 0x0647 }, /*                   Arabic_ha ه ARABIC LETTER HEH */
    { 0x05e8, 0x0648 }, /*                  Arabic_waw و ARABIC LETTER WAW */
    { 0x05e9, 0x0649 }, /*          Arabic_alefmaksura ى ARABIC LETTER ALEF MAKSURA */
    { 0x05ea, 0x064a }, /*                  Arabic_yeh ي ARABIC LETTER YEH */
    { 0x05eb, 0x064b }, /*             Arabic_fathatan ً ARABIC FATHATAN */
    { 0x05ec, 0x064c }, /*             Arabic_dammatan ٌ ARABIC DAMMATAN */
    { 0x05ed, 0x064d }, /*             Arabic_kasratan ٍ ARABIC KASRATAN */
    { 0x05ee, 0x064e }, /*                Arabic_fatha َ ARABIC FATHA */
    { 0x05ef, 0x064f }, /*                Arabic_damma ُ ARABIC DAMMA */
    { 0x05f0, 0x0650 }, /*                Arabic_kasra ِ ARABIC KASRA */
    { 0x05f1, 0x0651 }, /*               Arabic_shadda ّ ARABIC SHADDA */
    { 0x05f2, 0x0652 }, /*                Arabic_sukun ْ ARABIC SUKUN */
    { 0x06a1, 0x0452 }, /*                 Serbian_dje ђ CYRILLIC SMALL LETTER DJE */
    { 0x06a2, 0x0453 }, /*               Macedonia_gje ѓ CYRILLIC SMALL LETTER GJE */
    { 0x06a3, 0x0451 }, /*                 Cyrillic_io ё CYRILLIC SMALL LETTER IO */
    { 0x06a4, 0x0454 }, /*                Ukrainian_ie є CYRILLIC SMALL LETTER UKRAINIAN IE */
    { 0x06a5, 0x0455 }, /*               Macedonia_dse ѕ CYRILLIC SMALL LETTER DZE */
    { 0x06a6, 0x0456 }, /*                 Ukrainian_i і CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I */
    { 0x06a7, 0x0457 }, /*                Ukrainian_yi ї CYRILLIC SMALL LETTER YI */
    { 0x06a8, 0x0458 }, /*                 Cyrillic_je ј CYRILLIC SMALL LETTER JE */
    { 0x06a9, 0x0459 }, /*                Cyrillic_lje љ CYRILLIC SMALL LETTER LJE */
    { 0x06aa, 0x045a }, /*                Cyrillic_nje њ CYRILLIC SMALL LETTER NJE */
    { 0x06ab, 0x045b }, /*                Serbian_tshe ћ CYRILLIC SMALL LETTER TSHE */
    { 0x06ac, 0x045c }, /*               Macedonia_kje ќ CYRILLIC SMALL LETTER KJE */
    { 0x06ae, 0x045e }, /*         Byelorussian_shortu ў CYRILLIC SMALL LETTER SHORT U */
    { 0x06af, 0x045f }, /*               Cyrillic_dzhe џ CYRILLIC SMALL LETTER DZHE */
    { 0x06b0, 0x2116 }, /*                  numerosign № NUMERO SIGN */
    { 0x06b1, 0x0402 }, /*                 Serbian_DJE Ђ CYRILLIC CAPITAL LETTER DJE */
    { 0x06b2, 0x0403 }, /*               Macedonia_GJE Ѓ CYRILLIC CAPITAL LETTER GJE */
    { 0x06b3, 0x0401 }, /*                 Cyrillic_IO Ё CYRILLIC CAPITAL LETTER IO */
    { 0x06b4, 0x0404 }, /*                Ukrainian_IE Є CYRILLIC CAPITAL LETTER UKRAINIAN IE */
    { 0x06b5, 0x0405 }, /*               Macedonia_DSE Ѕ CYRILLIC CAPITAL LETTER DZE */
    { 0x06b6, 0x0406 }, /*                 Ukrainian_I І CYRILLIC CAPITAL LETTER BYELORUSSIAN-UKRAINIAN I */
    { 0x06b7, 0x0407 }, /*                Ukrainian_YI Ї CYRILLIC CAPITAL LETTER YI */
    { 0x06b8, 0x0408 }, /*                 Cyrillic_JE Ј CYRILLIC CAPITAL LETTER JE */
    { 0x06b9, 0x0409 }, /*                Cyrillic_LJE Љ CYRILLIC CAPITAL LETTER LJE */
    { 0x06ba, 0x040a }, /*                Cyrillic_NJE Њ CYRILLIC CAPITAL LETTER NJE */
    { 0x06bb, 0x040b }, /*                Serbian_TSHE Ћ CYRILLIC CAPITAL LETTER TSHE */
    { 0x06bc, 0x040c }, /*               Macedonia_KJE Ќ CYRILLIC CAPITAL LETTER KJE */
    { 0x06be, 0x040e }, /*         Byelorussian_SHORTU Ў CYRILLIC CAPITAL LETTER SHORT U */
    { 0x06bf, 0x040f }, /*               Cyrillic_DZHE Џ CYRILLIC CAPITAL LETTER DZHE */
    { 0x06c0, 0x044e }, /*                 Cyrillic_yu ю CYRILLIC SMALL LETTER YU */
    { 0x06c1, 0x0430 }, /*                  Cyrillic_a а CYRILLIC SMALL LETTER A */
    { 0x06c2, 0x0431 }, /*                 Cyrillic_be б CYRILLIC SMALL LETTER BE */
    { 0x06c3, 0x0446 }, /*                Cyrillic_tse ц CYRILLIC SMALL LETTER TSE */
    { 0x06c4, 0x0434 }, /*                 Cyrillic_de д CYRILLIC SMALL LETTER DE */
    { 0x06c5, 0x0435 }, /*                 Cyrillic_ie е CYRILLIC SMALL LETTER IE */
    { 0x06c6, 0x0444 }, /*                 Cyrillic_ef ф CYRILLIC SMALL LETTER EF */
    { 0x06c7, 0x0433 }, /*                Cyrillic_ghe г CYRILLIC SMALL LETTER GHE */
    { 0x06c8, 0x0445 }, /*                 Cyrillic_ha х CYRILLIC SMALL LETTER HA */
    { 0x06c9, 0x0438 }, /*                  Cyrillic_i и CYRILLIC SMALL LETTER I */
    { 0x06ca, 0x0439 }, /*             Cyrillic_shorti й CYRILLIC SMALL LETTER SHORT I */
    { 0x06cb, 0x043a }, /*                 Cyrillic_ka к CYRILLIC SMALL LETTER KA */
    { 0x06cc, 0x043b }, /*                 Cyrillic_el л CYRILLIC SMALL LETTER EL */
    { 0x06cd, 0x043c }, /*                 Cyrillic_em м CYRILLIC SMALL LETTER EM */
    { 0x06ce, 0x043d }, /*                 Cyrillic_en н CYRILLIC SMALL LETTER EN */
    { 0x06cf, 0x043e }, /*                  Cyrillic_o о CYRILLIC SMALL LETTER O */
    { 0x06d0, 0x043f }, /*                 Cyrillic_pe п CYRILLIC SMALL LETTER PE */
    { 0x06d1, 0x044f }, /*                 Cyrillic_ya я CYRILLIC SMALL LETTER YA */
    { 0x06d2, 0x0440 }, /*                 Cyrillic_er р CYRILLIC SMALL LETTER ER */
    { 0x06d3, 0x0441 }, /*                 Cyrillic_es с CYRILLIC SMALL LETTER ES */
    { 0x06d4, 0x0442 }, /*                 Cyrillic_te т CYRILLIC SMALL LETTER TE */
    { 0x06d5, 0x0443 }, /*                  Cyrillic_u у CYRILLIC SMALL LETTER U */
    { 0x06d6, 0x0436 }, /*                Cyrillic_zhe ж CYRILLIC SMALL LETTER ZHE */
    { 0x06d7, 0x0432 }, /*                 Cyrillic_ve в CYRILLIC SMALL LETTER VE */
    { 0x06d8, 0x044c }, /*           Cyrillic_softsign ь CYRILLIC SMALL LETTER SOFT SIGN */
    { 0x06d9, 0x044b }, /*               Cyrillic_yeru ы CYRILLIC SMALL LETTER YERU */
    { 0x06da, 0x0437 }, /*                 Cyrillic_ze з CYRILLIC SMALL LETTER ZE */
    { 0x06db, 0x0448 }, /*                Cyrillic_sha ш CYRILLIC SMALL LETTER SHA */
    { 0x06dc, 0x044d }, /*                  Cyrillic_e э CYRILLIC SMALL LETTER E */
    { 0x06dd, 0x0449 }, /*              Cyrillic_shcha щ CYRILLIC SMALL LETTER SHCHA */
    { 0x06de, 0x0447 }, /*                Cyrillic_che ч CYRILLIC SMALL LETTER CHE */
    { 0x06df, 0x044a }, /*           Cyrillic_hardsign ъ CYRILLIC SMALL LETTER HARD SIGN */
    { 0x06e0, 0x042e }, /*                 Cyrillic_YU Ю CYRILLIC CAPITAL LETTER YU */
    { 0x06e1, 0x0410 }, /*                  Cyrillic_A А CYRILLIC CAPITAL LETTER A */
    { 0x06e2, 0x0411 }, /*                 Cyrillic_BE Б CYRILLIC CAPITAL LETTER BE */
    { 0x06e3, 0x0426 }, /*                Cyrillic_TSE Ц CYRILLIC CAPITAL LETTER TSE */
    { 0x06e4, 0x0414 }, /*                 Cyrillic_DE Д CYRILLIC CAPITAL LETTER DE */
    { 0x06e5, 0x0415 }, /*                 Cyrillic_IE Е CYRILLIC CAPITAL LETTER IE */
    { 0x06e6, 0x0424 }, /*                 Cyrillic_EF Ф CYRILLIC CAPITAL LETTER EF */
    { 0x06e7, 0x0413 }, /*                Cyrillic_GHE Г CYRILLIC CAPITAL LETTER GHE */
    { 0x06e8, 0x0425 }, /*                 Cyrillic_HA Х CYRILLIC CAPITAL LETTER HA */
    { 0x06e9, 0x0418 }, /*                  Cyrillic_I И CYRILLIC CAPITAL LETTER I */
    { 0x06ea, 0x0419 }, /*             Cyrillic_SHORTI Й CYRILLIC CAPITAL LETTER SHORT I */
    { 0x06eb, 0x041a }, /*                 Cyrillic_KA К CYRILLIC CAPITAL LETTER KA */
    { 0x06ec, 0x041b }, /*                 Cyrillic_EL Л CYRILLIC CAPITAL LETTER EL */
    { 0x06ed, 0x041c }, /*                 Cyrillic_EM М CYRILLIC CAPITAL LETTER EM */
    { 0x06ee, 0x041d }, /*                 Cyrillic_EN Н CYRILLIC CAPITAL LETTER EN */
    { 0x06ef, 0x041e }, /*                  Cyrillic_O О CYRILLIC CAPITAL LETTER O */
    { 0x06f0, 0x041f }, /*                 Cyrillic_PE П CYRILLIC CAPITAL LETTER PE */
    { 0x06f1, 0x042f }, /*                 Cyrillic_YA Я CYRILLIC CAPITAL LETTER YA */
    { 0x06f2, 0x0420 }, /*                 Cyrillic_ER Р CYRILLIC CAPITAL LETTER ER */
    { 0x06f3, 0x0421 }, /*                 Cyrillic_ES С CYRILLIC CAPITAL LETTER ES */
    { 0x06f4, 0x0422 }, /*                 Cyrillic_TE Т CYRILLIC CAPITAL LETTER TE */
    { 0x06f5, 0x0423 }, /*                  Cyrillic_U У CYRILLIC CAPITAL LETTER U */
    { 0x06f6, 0x0416 }, /*                Cyrillic_ZHE Ж CYRILLIC CAPITAL LETTER ZHE */
    { 0x06f7, 0x0412 }, /*                 Cyrillic_VE В CYRILLIC CAPITAL LETTER VE */
    { 0x06f8, 0x042c }, /*           Cyrillic_SOFTSIGN Ь CYRILLIC CAPITAL LETTER SOFT SIGN */
    { 0x06f9, 0x042b }, /*               Cyrillic_YERU Ы CYRILLIC CAPITAL LETTER YERU */
    { 0x06fa, 0x0417 }, /*                 Cyrillic_ZE З CYRILLIC CAPITAL LETTER ZE */
    { 0x06fb, 0x0428 }, /*                Cyrillic_SHA Ш CYRILLIC CAPITAL LETTER SHA */
    { 0x06fc, 0x042d }, /*                  Cyrillic_E Э CYRILLIC CAPITAL LETTER E */
    { 0x06fd, 0x0429 }, /*              Cyrillic_SHCHA Щ CYRILLIC CAPITAL LETTER SHCHA */
    { 0x06fe, 0x0427 }, /*                Cyrillic_CHE Ч CYRILLIC CAPITAL LETTER CHE */
    { 0x06ff, 0x042a }, /*           Cyrillic_HARDSIGN Ъ CYRILLIC CAPITAL LETTER HARD SIGN */
    { 0x07a1, 0x0386 }, /*           Greek_ALPHAaccent Ά GREEK CAPITAL LETTER ALPHA WITH TONOS */
    { 0x07a2, 0x0388 }, /*         Greek_EPSILONaccent Έ GREEK CAPITAL LETTER EPSILON WITH TONOS */
    { 0x07a3, 0x0389 }, /*             Greek_ETAaccent Ή GREEK CAPITAL LETTER ETA WITH TONOS */
    { 0x07a4, 0x038a }, /*            Greek_IOTAaccent Ί GREEK CAPITAL LETTER IOTA WITH TONOS */
    { 0x07a5, 0x03aa }, /*         Greek_IOTAdiaeresis Ϊ GREEK CAPITAL LETTER IOTA WITH DIALYTIKA */
    { 0x07a7, 0x038c }, /*         Greek_OMICRONaccent Ό GREEK CAPITAL LETTER OMICRON WITH TONOS */
    { 0x07a8, 0x038e }, /*         Greek_UPSILONaccent Ύ GREEK CAPITAL LETTER UPSILON WITH TONOS */
    { 0x07a9, 0x03ab }, /*       Greek_UPSILONdieresis Ϋ GREEK CAPITAL LETTER UPSILON WITH DIALYTIKA */
    { 0x07ab, 0x038f }, /*           Greek_OMEGAaccent Ώ GREEK CAPITAL LETTER OMEGA WITH TONOS */
    { 0x07ae, 0x0385 }, /*        Greek_accentdieresis ΅ GREEK DIALYTIKA TONOS */
    { 0x07af, 0x2015 }, /*              Greek_horizbar ― HORIZONTAL BAR */
    { 0x07b1, 0x03ac }, /*           Greek_alphaaccent ά GREEK SMALL LETTER ALPHA WITH TONOS */
    { 0x07b2, 0x03ad }, /*         Greek_epsilonaccent έ GREEK SMALL LETTER EPSILON WITH TONOS */
    { 0x07b3, 0x03ae }, /*             Greek_etaaccent ή GREEK SMALL LETTER ETA WITH TONOS */
    { 0x07b4, 0x03af }, /*            Greek_iotaaccent ί GREEK SMALL LETTER IOTA WITH TONOS */
    { 0x07b5, 0x03ca }, /*          Greek_iotadieresis ϊ GREEK SMALL LETTER IOTA WITH DIALYTIKA */
    { 0x07b6, 0x0390 }, /*    Greek_iotaaccentdieresis ΐ GREEK SMALL LETTER IOTA WITH DIALYTIKA AND TONOS */
    { 0x07b7, 0x03cc }, /*         Greek_omicronaccent ό GREEK SMALL LETTER OMICRON WITH TONOS */
    { 0x07b8, 0x03cd }, /*         Greek_upsilonaccent ύ GREEK SMALL LETTER UPSILON WITH TONOS */
    { 0x07b9, 0x03cb }, /*       Greek_upsilondieresis ϋ GREEK SMALL LETTER UPSILON WITH DIALYTIKA */
    { 0x07ba, 0x03b0 }, /* Greek_upsilonaccentdieresis ΰ GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS */
    { 0x07bb, 0x03ce }, /*           Greek_omegaaccent ώ GREEK SMALL LETTER OMEGA WITH TONOS */
    { 0x07c1, 0x0391 }, /*                 Greek_ALPHA Α GREEK CAPITAL LETTER ALPHA */
    { 0x07c2, 0x0392 }, /*                  Greek_BETA Β GREEK CAPITAL LETTER BETA */
    { 0x07c3, 0x0393 }, /*                 Greek_GAMMA Γ GREEK CAPITAL LETTER GAMMA */
    { 0x07c4, 0x0394 }, /*                 Greek_DELTA Δ GREEK CAPITAL LETTER DELTA */
    { 0x07c5, 0x0395 }, /*               Greek_EPSILON Ε GREEK CAPITAL LETTER EPSILON */
    { 0x07c6, 0x0396 }, /*                  Greek_ZETA Ζ GREEK CAPITAL LETTER ZETA */
    { 0x07c7, 0x0397 }, /*                   Greek_ETA Η GREEK CAPITAL LETTER ETA */
    { 0x07c8, 0x0398 }, /*                 Greek_THETA Θ GREEK CAPITAL LETTER THETA */
    { 0x07c9, 0x0399 }, /*                  Greek_IOTA Ι GREEK CAPITAL LETTER IOTA */
    { 0x07ca, 0x039a }, /*                 Greek_KAPPA Κ GREEK CAPITAL LETTER KAPPA */
    { 0x07cb, 0x039b }, /*                Greek_LAMBDA Λ GREEK CAPITAL LETTER LAMDA */
    { 0x07cc, 0x039c }, /*                    Greek_MU Μ GREEK CAPITAL LETTER MU */
    { 0x07cd, 0x039d }, /*                    Greek_NU Ν GREEK CAPITAL LETTER NU */
    { 0x07ce, 0x039e }, /*                    Greek_XI Ξ GREEK CAPITAL LETTER XI */
    { 0x07cf, 0x039f }, /*               Greek_OMICRON Ο GREEK CAPITAL LETTER OMICRON */
    { 0x07d0, 0x03a0 }, /*                    Greek_PI Π GREEK CAPITAL LETTER PI */
    { 0x07d1, 0x03a1 }, /*                   Greek_RHO Ρ GREEK CAPITAL LETTER RHO */
    { 0x07d2, 0x03a3 }, /*                 Greek_SIGMA Σ GREEK CAPITAL LETTER SIGMA */
    { 0x07d4, 0x03a4 }, /*                   Greek_TAU Τ GREEK CAPITAL LETTER TAU */
    { 0x07d5, 0x03a5 }, /*               Greek_UPSILON Υ GREEK CAPITAL LETTER UPSILON */
    { 0x07d6, 0x03a6 }, /*                   Greek_PHI Φ GREEK CAPITAL LETTER PHI */
    { 0x07d7, 0x03a7 }, /*                   Greek_CHI Χ GREEK CAPITAL LETTER CHI */
    { 0x07d8, 0x03a8 }, /*                   Greek_PSI Ψ GREEK CAPITAL LETTER PSI */
    { 0x07d9, 0x03a9 }, /*                 Greek_OMEGA Ω GREEK CAPITAL LETTER OMEGA */
    { 0x07e1, 0x03b1 }, /*                 Greek_alpha α GREEK SMALL LETTER ALPHA */
    { 0x07e2, 0x03b2 }, /*                  Greek_beta β GREEK SMALL LETTER BETA */
    { 0x07e3, 0x03b3 }, /*                 Greek_gamma γ GREEK SMALL LETTER GAMMA */
    { 0x07e4, 0x03b4 }, /*                 Greek_delta δ GREEK SMALL LETTER DELTA */
    { 0x07e5, 0x03b5 }, /*               Greek_epsilon ε GREEK SMALL LETTER EPSILON */
    { 0x07e6, 0x03b6 }, /*                  Greek_zeta ζ GREEK SMALL LETTER ZETA */
    { 0x07e7, 0x03b7 }, /*                   Greek_eta η GREEK SMALL LETTER ETA */
    { 0x07e8, 0x03b8 }, /*                 Greek_theta θ GREEK SMALL LETTER THETA */
    { 0x07e9, 0x03b9 }, /*                  Greek_iota ι GREEK SMALL LETTER IOTA */
    { 0x07ea, 0x03ba }, /*                 Greek_kappa κ GREEK SMALL LETTER KAPPA */
    { 0x07eb, 0x03bb }, /*                Greek_lambda λ GREEK SMALL LETTER LAMDA */
    { 0x07ec, 0x03bc }, /*                    Greek_mu μ GREEK SMALL LETTER MU */
    { 0x07ed, 0x03bd }, /*                    Greek_nu ν GREEK SMALL LETTER NU */
    { 0x07ee, 0x03be }, /*                    Greek_xi ξ GREEK SMALL LETTER XI */
    { 0x07ef, 0x03bf }, /*               Greek_omicron ο GREEK SMALL LETTER OMICRON */
    { 0x07f0, 0x03c0 }, /*                    Greek_pi π GREEK SMALL LETTER PI */
    { 0x07f1, 0x03c1 }, /*                   Greek_rho ρ GREEK SMALL LETTER RHO */
    { 0x07f2, 0x03c3 }, /*                 Greek_sigma σ GREEK SMALL LETTER SIGMA */
    { 0x07f3, 0x03c2 }, /*       Greek_finalsmallsigma ς GREEK SMALL LETTER FINAL SIGMA */
    { 0x07f4, 0x03c4 }, /*                   Greek_tau τ GREEK SMALL LETTER TAU */
    { 0x07f5, 0x03c5 }, /*               Greek_upsilon υ GREEK SMALL LETTER UPSILON */
    { 0x07f6, 0x03c6 }, /*                   Greek_phi φ GREEK SMALL LETTER PHI */
    { 0x07f7, 0x03c7 }, /*                   Greek_chi χ GREEK SMALL LETTER CHI */
    { 0x07f8, 0x03c8 }, /*                   Greek_psi ψ GREEK SMALL LETTER PSI */
    { 0x07f9, 0x03c9 }, /*                 Greek_omega ω GREEK SMALL LETTER OMEGA */
    { 0x08a1, 0x23b7 }, /*                 leftradical ⎷ ??? */
    { 0x08a2, 0x250c }, /*              topleftradical ┌ BOX DRAWINGS LIGHT DOWN AND RIGHT */
    { 0x08a3, 0x2500 }, /*              horizconnector ─ BOX DRAWINGS LIGHT HORIZONTAL */
    { 0x08a4, 0x2320 }, /*                 topintegral ⌠ TOP HALF INTEGRAL */
    { 0x08a5, 0x2321 }, /*                 botintegral ⌡ BOTTOM HALF INTEGRAL */
    { 0x08a6, 0x2502 }, /*               vertconnector │ BOX DRAWINGS LIGHT VERTICAL */
    { 0x08a7, 0x23a1 }, /*            topleftsqbracket ⎡ ??? */
    { 0x08a8, 0x23a3 }, /*            botleftsqbracket ⎣ ??? */
    { 0x08a9, 0x23a4 }, /*           toprightsqbracket ⎤ ??? */
    { 0x08aa, 0x23a6 }, /*           botrightsqbracket ⎦ ??? */
    { 0x08ab, 0x239b }, /*               topleftparens ⎛ ??? */
    { 0x08ac, 0x239d }, /*               botleftparens ⎝ ??? */
    { 0x08ad, 0x239e }, /*              toprightparens ⎞ ??? */
    { 0x08ae, 0x23a0 }, /*              botrightparens ⎠ ??? */
    { 0x08af, 0x23a8 }, /*        leftmiddlecurlybrace ⎨ ??? */
    { 0x08b0, 0x23ac }, /*       rightmiddlecurlybrace ⎬ ??? */
    /*  0x08b1                          topleftsummation ? ??? */
    /*  0x08b2                          botleftsummation ? ??? */
    /*  0x08b3                 topvertsummationconnector ? ??? */
    /*  0x08b4                 botvertsummationconnector ? ??? */
    /*  0x08b5                         toprightsummation ? ??? */
    /*  0x08b6                         botrightsummation ? ??? */
    /*  0x08b7                      rightmiddlesummation ? ??? */
    { 0x08bc, 0x2264 }, /*               lessthanequal ≤ LESS-THAN OR EQUAL TO */
    { 0x08bd, 0x2260 }, /*                    notequal ≠ NOT EQUAL TO */
    { 0x08be, 0x2265 }, /*            greaterthanequal ≥ GREATER-THAN OR EQUAL TO */
    { 0x08bf, 0x222b }, /*                    integral ∫ INTEGRAL */
    { 0x08c0, 0x2234 }, /*                   therefore ∴ THEREFORE */
    { 0x08c1, 0x221d }, /*                   variation ∝ PROPORTIONAL TO */
    { 0x08c2, 0x221e }, /*                    infinity ∞ INFINITY */
    { 0x08c5, 0x2207 }, /*                       nabla ∇ NABLA */
    { 0x08c8, 0x223c }, /*                 approximate ∼ TILDE OPERATOR */
    { 0x08c9, 0x2243 }, /*                similarequal ≃ ASYMPTOTICALLY EQUAL TO */
    { 0x08cd, 0x21d4 }, /*                    ifonlyif ⇔ LEFT RIGHT DOUBLE ARROW */
    { 0x08ce, 0x21d2 }, /*                     implies ⇒ RIGHTWARDS DOUBLE ARROW */
    { 0x08cf, 0x2261 }, /*                   identical ≡ IDENTICAL TO */
    { 0x08d6, 0x221a }, /*                     radical √ SQUARE ROOT */
    { 0x08da, 0x2282 }, /*                  includedin ⊂ SUBSET OF */
    { 0x08db, 0x2283 }, /*                    includes ⊃ SUPERSET OF */
    { 0x08dc, 0x2229 }, /*                intersection ∩ INTERSECTION */
    { 0x08dd, 0x222a }, /*                       union ∪ UNION */
    { 0x08de, 0x2227 }, /*                  logicaland ∧ LOGICAL AND */
    { 0x08df, 0x2228 }, /*                   logicalor ∨ LOGICAL OR */
    { 0x08ef, 0x2202 }, /*           partialderivative ∂ PARTIAL DIFFERENTIAL */
    { 0x08f6, 0x0192 }, /*                    function ƒ LATIN SMALL LETTER F WITH HOOK */
    { 0x08fb, 0x2190 }, /*                   leftarrow ← LEFTWARDS ARROW */
    { 0x08fc, 0x2191 }, /*                     uparrow ↑ UPWARDS ARROW */
    { 0x08fd, 0x2192 }, /*                  rightarrow → RIGHTWARDS ARROW */
    { 0x08fe, 0x2193 }, /*                   downarrow ↓ DOWNWARDS ARROW */
    /*  0x09df                                     blank ? ??? */
    { 0x09e0, 0x25c6 }, /*                soliddiamond ◆ BLACK DIAMOND */
    { 0x09e1, 0x2592 }, /*                checkerboard ▒ MEDIUM SHADE */
    { 0x09e2, 0x2409 }, /*                          ht ␉ SYMBOL FOR HORIZONTAL TABULATION */
    { 0x09e3, 0x240c }, /*                          ff ␌ SYMBOL FOR FORM FEED */
    { 0x09e4, 0x240d }, /*                          cr ␍ SYMBOL FOR CARRIAGE RETURN */
    { 0x09e5, 0x240a }, /*                          lf ␊ SYMBOL FOR LINE FEED */
    { 0x09e8, 0x2424 }, /*                          nl ␤ SYMBOL FOR NEWLINE */
    { 0x09e9, 0x240b }, /*                          vt ␋ SYMBOL FOR VERTICAL TABULATION */
    { 0x09ea, 0x2518 }, /*              lowrightcorner ┘ BOX DRAWINGS LIGHT UP AND LEFT */
    { 0x09eb, 0x2510 }, /*               uprightcorner ┐ BOX DRAWINGS LIGHT DOWN AND LEFT */
    { 0x09ec, 0x250c }, /*                upleftcorner ┌ BOX DRAWINGS LIGHT DOWN AND RIGHT */
    { 0x09ed, 0x2514 }, /*               lowleftcorner └ BOX DRAWINGS LIGHT UP AND RIGHT */
    { 0x09ee, 0x253c }, /*               crossinglines ┼ BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
    { 0x09ef, 0x23ba }, /*              horizlinescan1 ⎺ HORIZONTAL SCAN LINE-1 (Unicode 3.2 draft) */
    { 0x09f0, 0x23bb }, /*              horizlinescan3 ⎻ HORIZONTAL SCAN LINE-3 (Unicode 3.2 draft) */
    { 0x09f1, 0x2500 }, /*              horizlinescan5 ─ BOX DRAWINGS LIGHT HORIZONTAL */
    { 0x09f2, 0x23bc }, /*              horizlinescan7 ⎼ HORIZONTAL SCAN LINE-7 (Unicode 3.2 draft) */
    { 0x09f3, 0x23bd }, /*              horizlinescan9 ⎽ HORIZONTAL SCAN LINE-9 (Unicode 3.2 draft) */
    { 0x09f4, 0x251c }, /*                       leftt ├ BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
    { 0x09f5, 0x2524 }, /*                      rightt ┤ BOX DRAWINGS LIGHT VERTICAL AND LEFT */
    { 0x09f6, 0x2534 }, /*                        bott ┴ BOX DRAWINGS LIGHT UP AND HORIZONTAL */
    { 0x09f7, 0x252c }, /*                        topt ┬ BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
    { 0x09f8, 0x2502 }, /*                     vertbar │ BOX DRAWINGS LIGHT VERTICAL */
    { 0x0aa1, 0x2003 }, /*                     emspace   EM SPACE */
    { 0x0aa2, 0x2002 }, /*                     enspace   EN SPACE */
    { 0x0aa3, 0x2004 }, /*                    em3space   THREE-PER-EM SPACE */
    { 0x0aa4, 0x2005 }, /*                    em4space   FOUR-PER-EM SPACE */
    { 0x0aa5, 0x2007 }, /*                  digitspace   FIGURE SPACE */
    { 0x0aa6, 0x2008 }, /*                  punctspace   PUNCTUATION SPACE */
    { 0x0aa7, 0x2009 }, /*                   thinspace   THIN SPACE */
    { 0x0aa8, 0x200a }, /*                   hairspace   HAIR SPACE */
    { 0x0aa9, 0x2014 }, /*                      emdash — EM DASH */
    { 0x0aaa, 0x2013 }, /*                      endash – EN DASH */
    /*  0x0aac                               signifblank ? ??? */
    { 0x0aae, 0x2026 }, /*                    ellipsis … HORIZONTAL ELLIPSIS */
    { 0x0aaf, 0x2025 }, /*             doubbaselinedot ‥ TWO DOT LEADER */
    { 0x0ab0, 0x2153 }, /*                    onethird ⅓ VULGAR FRACTION ONE THIRD */
    { 0x0ab1, 0x2154 }, /*                   twothirds ⅔ VULGAR FRACTION TWO THIRDS */
    { 0x0ab2, 0x2155 }, /*                    onefifth ⅕ VULGAR FRACTION ONE FIFTH */
    { 0x0ab3, 0x2156 }, /*                   twofifths ⅖ VULGAR FRACTION TWO FIFTHS */
    { 0x0ab4, 0x2157 }, /*                 threefifths ⅗ VULGAR FRACTION THREE FIFTHS */
    { 0x0ab5, 0x2158 }, /*                  fourfifths ⅘ VULGAR FRACTION FOUR FIFTHS */
    { 0x0ab6, 0x2159 }, /*                    onesixth ⅙ VULGAR FRACTION ONE SIXTH */
    { 0x0ab7, 0x215a }, /*                  fivesixths ⅚ VULGAR FRACTION FIVE SIXTHS */
    { 0x0ab8, 0x2105 }, /*                      careof ℅ CARE OF */
    { 0x0abb, 0x2012 }, /*                     figdash ‒ FIGURE DASH */
    { 0x0abc, 0x2329 }, /*            leftanglebracket 〈 LEFT-POINTING ANGLE BRACKET */
    /*  0x0abd                              decimalpoint ? ??? */
    { 0x0abe, 0x232a }, /*           rightanglebracket 〉 RIGHT-POINTING ANGLE BRACKET */
    /*  0x0abf                                    marker ? ??? */
    { 0x0ac3, 0x215b }, /*                   oneeighth ⅛ VULGAR FRACTION ONE EIGHTH */
    { 0x0ac4, 0x215c }, /*                threeeighths ⅜ VULGAR FRACTION THREE EIGHTHS */
    { 0x0ac5, 0x215d }, /*                 fiveeighths ⅝ VULGAR FRACTION FIVE EIGHTHS */
    { 0x0ac6, 0x215e }, /*                seveneighths ⅞ VULGAR FRACTION SEVEN EIGHTHS */
    { 0x0ac9, 0x2122 }, /*                   trademark ™ TRADE MARK SIGN */
    { 0x0aca, 0x2613 }, /*               signaturemark ☓ SALTIRE */
    /*  0x0acb                         trademarkincircle ? ??? */
    { 0x0acc, 0x25c1 }, /*            leftopentriangle ◁ WHITE LEFT-POINTING TRIANGLE */
    { 0x0acd, 0x25b7 }, /*           rightopentriangle ▷ WHITE RIGHT-POINTING TRIANGLE */
    { 0x0ace, 0x25cb }, /*                emopencircle ○ WHITE CIRCLE */
    { 0x0acf, 0x25af }, /*             emopenrectangle ▯ WHITE VERTICAL RECTANGLE */
    { 0x0ad0, 0x2018 }, /*         leftsinglequotemark ‘ LEFT SINGLE QUOTATION MARK */
    { 0x0ad1, 0x2019 }, /*        rightsinglequotemark ’ RIGHT SINGLE QUOTATION MARK */
    { 0x0ad2, 0x201c }, /*         leftdoublequotemark “ LEFT DOUBLE QUOTATION MARK */
    { 0x0ad3, 0x201d }, /*        rightdoublequotemark ” RIGHT DOUBLE QUOTATION MARK */
    { 0x0ad4, 0x211e }, /*                prescription ℞ PRESCRIPTION TAKE */
    { 0x0ad6, 0x2032 }, /*                     minutes ′ PRIME */
    { 0x0ad7, 0x2033 }, /*                     seconds ″ DOUBLE PRIME */
    { 0x0ad9, 0x271d }, /*                  latincross ✝ LATIN CROSS */
    /*  0x0ada                                  hexagram ? ??? */
    { 0x0adb, 0x25ac }, /*            filledrectbullet ▬ BLACK RECTANGLE */
    { 0x0adc, 0x25c0 }, /*         filledlefttribullet ◀ BLACK LEFT-POINTING TRIANGLE */
    { 0x0add, 0x25b6 }, /*        filledrighttribullet ▶ BLACK RIGHT-POINTING TRIANGLE */
    { 0x0ade, 0x25cf }, /*              emfilledcircle ● BLACK CIRCLE */
    { 0x0adf, 0x25ae }, /*                emfilledrect ▮ BLACK VERTICAL RECTANGLE */
    { 0x0ae0, 0x25e6 }, /*            enopencircbullet ◦ WHITE BULLET */
    { 0x0ae1, 0x25ab }, /*          enopensquarebullet ▫ WHITE SMALL SQUARE */
    { 0x0ae2, 0x25ad }, /*              openrectbullet ▭ WHITE RECTANGLE */
    { 0x0ae3, 0x25b3 }, /*             opentribulletup △ WHITE UP-POINTING TRIANGLE */
    { 0x0ae4, 0x25bd }, /*           opentribulletdown ▽ WHITE DOWN-POINTING TRIANGLE */
    { 0x0ae5, 0x2606 }, /*                    openstar ☆ WHITE STAR */
    { 0x0ae6, 0x2022 }, /*          enfilledcircbullet • BULLET */
    { 0x0ae7, 0x25aa }, /*            enfilledsqbullet ▪ BLACK SMALL SQUARE */
    { 0x0ae8, 0x25b2 }, /*           filledtribulletup ▲ BLACK UP-POINTING TRIANGLE */
    { 0x0ae9, 0x25bc }, /*         filledtribulletdown ▼ BLACK DOWN-POINTING TRIANGLE */
    { 0x0aea, 0x261c }, /*                 leftpointer ☜ WHITE LEFT POINTING INDEX */
    { 0x0aeb, 0x261e }, /*                rightpointer ☞ WHITE RIGHT POINTING INDEX */
    { 0x0aec, 0x2663 }, /*                        club ♣ BLACK CLUB SUIT */
    { 0x0aed, 0x2666 }, /*                     diamond ♦ BLACK DIAMOND SUIT */
    { 0x0aee, 0x2665 }, /*                       heart ♥ BLACK HEART SUIT */
    { 0x0af0, 0x2720 }, /*                maltesecross ✠ MALTESE CROSS */
    { 0x0af1, 0x2020 }, /*                      dagger † DAGGER */
    { 0x0af2, 0x2021 }, /*                doubledagger ‡ DOUBLE DAGGER */
    { 0x0af3, 0x2713 }, /*                   checkmark ✓ CHECK MARK */
    { 0x0af4, 0x2717 }, /*                 ballotcross ✗ BALLOT X */
    { 0x0af5, 0x266f }, /*                musicalsharp ♯ MUSIC SHARP SIGN */
    { 0x0af6, 0x266d }, /*                 musicalflat ♭ MUSIC FLAT SIGN */
    { 0x0af7, 0x2642 }, /*                  malesymbol ♂ MALE SIGN */
    { 0x0af8, 0x2640 }, /*                femalesymbol ♀ FEMALE SIGN */
    { 0x0af9, 0x260e }, /*                   telephone ☎ BLACK TELEPHONE */
    { 0x0afa, 0x2315 }, /*           telephonerecorder ⌕ TELEPHONE RECORDER */
    { 0x0afb, 0x2117 }, /*         phonographcopyright ℗ SOUND RECORDING COPYRIGHT */
    { 0x0afc, 0x2038 }, /*                       caret ‸ CARET */
    { 0x0afd, 0x201a }, /*          singlelowquotemark ‚ SINGLE LOW-9 QUOTATION MARK */
    { 0x0afe, 0x201e }, /*          doublelowquotemark „ DOUBLE LOW-9 QUOTATION MARK */
    /*  0x0aff                                    cursor ? ??? */
    { 0x0ba3, 0x003c }, /*                   leftcaret < LESS-THAN SIGN */
    { 0x0ba6, 0x003e }, /*                  rightcaret > GREATER-THAN SIGN */
    { 0x0ba8, 0x2228 }, /*                   downcaret ∨ LOGICAL OR */
    { 0x0ba9, 0x2227 }, /*                     upcaret ∧ LOGICAL AND */
    { 0x0bc0, 0x00af }, /*                     overbar ¯ MACRON */
    { 0x0bc2, 0x22a5 }, /*                    downtack ⊥ UP TACK */
    { 0x0bc3, 0x2229 }, /*                      upshoe ∩ INTERSECTION */
    { 0x0bc4, 0x230a }, /*                   downstile ⌊ LEFT FLOOR */
    { 0x0bc6, 0x005f }, /*                    underbar _ LOW LINE */
    { 0x0bca, 0x2218 }, /*                         jot ∘ RING OPERATOR */
    { 0x0bcc, 0x2395 }, /*                        quad ⎕ APL FUNCTIONAL SYMBOL QUAD */
    { 0x0bce, 0x22a4 }, /*                      uptack ⊤ DOWN TACK */
    { 0x0bcf, 0x25cb }, /*                      circle ○ WHITE CIRCLE */
    { 0x0bd3, 0x2308 }, /*                     upstile ⌈ LEFT CEILING */
    { 0x0bd6, 0x222a }, /*                    downshoe ∪ UNION */
    { 0x0bd8, 0x2283 }, /*                   rightshoe ⊃ SUPERSET OF */
    { 0x0bda, 0x2282 }, /*                    leftshoe ⊂ SUBSET OF */
    { 0x0bdc, 0x22a2 }, /*                    lefttack ⊢ RIGHT TACK */
    { 0x0bfc, 0x22a3 }, /*                   righttack ⊣ LEFT TACK */
    { 0x0cdf, 0x2017 }, /*        hebrew_doublelowline ‗ DOUBLE LOW LINE */
    { 0x0ce0, 0x05d0 }, /*                hebrew_aleph א HEBREW LETTER ALEF */
    { 0x0ce1, 0x05d1 }, /*                  hebrew_bet ב HEBREW LETTER BET */
    { 0x0ce2, 0x05d2 }, /*                hebrew_gimel ג HEBREW LETTER GIMEL */
    { 0x0ce3, 0x05d3 }, /*                hebrew_dalet ד HEBREW LETTER DALET */
    { 0x0ce4, 0x05d4 }, /*                   hebrew_he ה HEBREW LETTER HE */
    { 0x0ce5, 0x05d5 }, /*                  hebrew_waw ו HEBREW LETTER VAV */
    { 0x0ce6, 0x05d6 }, /*                 hebrew_zain ז HEBREW LETTER ZAYIN */
    { 0x0ce7, 0x05d7 }, /*                 hebrew_chet ח HEBREW LETTER HET */
    { 0x0ce8, 0x05d8 }, /*                  hebrew_tet ט HEBREW LETTER TET */
    { 0x0ce9, 0x05d9 }, /*                  hebrew_yod י HEBREW LETTER YOD */
    { 0x0cea, 0x05da }, /*            hebrew_finalkaph ך HEBREW LETTER FINAL KAF */
    { 0x0ceb, 0x05db }, /*                 hebrew_kaph כ HEBREW LETTER KAF */
    { 0x0cec, 0x05dc }, /*                hebrew_lamed ל HEBREW LETTER LAMED */
    { 0x0ced, 0x05dd }, /*             hebrew_finalmem ם HEBREW LETTER FINAL MEM */
    { 0x0cee, 0x05de }, /*                  hebrew_mem מ HEBREW LETTER MEM */
    { 0x0cef, 0x05df }, /*             hebrew_finalnun ן HEBREW LETTER FINAL NUN */
    { 0x0cf0, 0x05e0 }, /*                  hebrew_nun נ HEBREW LETTER NUN */
    { 0x0cf1, 0x05e1 }, /*               hebrew_samech ס HEBREW LETTER SAMEKH */
    { 0x0cf2, 0x05e2 }, /*                 hebrew_ayin ע HEBREW LETTER AYIN */
    { 0x0cf3, 0x05e3 }, /*              hebrew_finalpe ף HEBREW LETTER FINAL PE */
    { 0x0cf4, 0x05e4 }, /*                   hebrew_pe פ HEBREW LETTER PE */
    { 0x0cf5, 0x05e5 }, /*            hebrew_finalzade ץ HEBREW LETTER FINAL TSADI */
    { 0x0cf6, 0x05e6 }, /*                 hebrew_zade צ HEBREW LETTER TSADI */
    { 0x0cf7, 0x05e7 }, /*                 hebrew_qoph ק HEBREW LETTER QOF */
    { 0x0cf8, 0x05e8 }, /*                 hebrew_resh ר HEBREW LETTER RESH */
    { 0x0cf9, 0x05e9 }, /*                 hebrew_shin ש HEBREW LETTER SHIN */
    { 0x0cfa, 0x05ea }, /*                  hebrew_taw ת HEBREW LETTER TAV */
    { 0x0da1, 0x0e01 }, /*                  Thai_kokai ก THAI CHARACTER KO KAI */
    { 0x0da2, 0x0e02 }, /*                Thai_khokhai ข THAI CHARACTER KHO KHAI */
    { 0x0da3, 0x0e03 }, /*               Thai_khokhuat ฃ THAI CHARACTER KHO KHUAT */
    { 0x0da4, 0x0e04 }, /*               Thai_khokhwai ค THAI CHARACTER KHO KHWAI */
    { 0x0da5, 0x0e05 }, /*                Thai_khokhon ฅ THAI CHARACTER KHO KHON */
    { 0x0da6, 0x0e06 }, /*             Thai_khorakhang ฆ THAI CHARACTER KHO RAKHANG */
    { 0x0da7, 0x0e07 }, /*                 Thai_ngongu ง THAI CHARACTER NGO NGU */
    { 0x0da8, 0x0e08 }, /*                Thai_chochan จ THAI CHARACTER CHO CHAN */
    { 0x0da9, 0x0e09 }, /*               Thai_choching ฉ THAI CHARACTER CHO CHING */
    { 0x0daa, 0x0e0a }, /*               Thai_chochang ช THAI CHARACTER CHO CHANG */
    { 0x0dab, 0x0e0b }, /*                   Thai_soso ซ THAI CHARACTER SO SO */
    { 0x0dac, 0x0e0c }, /*                Thai_chochoe ฌ THAI CHARACTER CHO CHOE */
    { 0x0dad, 0x0e0d }, /*                 Thai_yoying ญ THAI CHARACTER YO YING */
    { 0x0dae, 0x0e0e }, /*                Thai_dochada ฎ THAI CHARACTER DO CHADA */
    { 0x0daf, 0x0e0f }, /*                Thai_topatak ฏ THAI CHARACTER TO PATAK */
    { 0x0db0, 0x0e10 }, /*                Thai_thothan ฐ THAI CHARACTER THO THAN */
    { 0x0db1, 0x0e11 }, /*          Thai_thonangmontho ฑ THAI CHARACTER THO NANGMONTHO */
    { 0x0db2, 0x0e12 }, /*             Thai_thophuthao ฒ THAI CHARACTER THO PHUTHAO */
    { 0x0db3, 0x0e13 }, /*                  Thai_nonen ณ THAI CHARACTER NO NEN */
    { 0x0db4, 0x0e14 }, /*                  Thai_dodek ด THAI CHARACTER DO DEK */
    { 0x0db5, 0x0e15 }, /*                  Thai_totao ต THAI CHARACTER TO TAO */
    { 0x0db6, 0x0e16 }, /*               Thai_thothung ถ THAI CHARACTER THO THUNG */
    { 0x0db7, 0x0e17 }, /*              Thai_thothahan ท THAI CHARACTER THO THAHAN */
    { 0x0db8, 0x0e18 }, /*               Thai_thothong ธ THAI CHARACTER THO THONG */
    { 0x0db9, 0x0e19 }, /*                   Thai_nonu น THAI CHARACTER NO NU */
    { 0x0dba, 0x0e1a }, /*               Thai_bobaimai บ THAI CHARACTER BO BAIMAI */
    { 0x0dbb, 0x0e1b }, /*                  Thai_popla ป THAI CHARACTER PO PLA */
    { 0x0dbc, 0x0e1c }, /*               Thai_phophung ผ THAI CHARACTER PHO PHUNG */
    { 0x0dbd, 0x0e1d }, /*                   Thai_fofa ฝ THAI CHARACTER FO FA */
    { 0x0dbe, 0x0e1e }, /*                Thai_phophan พ THAI CHARACTER PHO PHAN */
    { 0x0dbf, 0x0e1f }, /*                  Thai_fofan ฟ THAI CHARACTER FO FAN */
    { 0x0dc0, 0x0e20 }, /*             Thai_phosamphao ภ THAI CHARACTER PHO SAMPHAO */
    { 0x0dc1, 0x0e21 }, /*                   Thai_moma ม THAI CHARACTER MO MA */
    { 0x0dc2, 0x0e22 }, /*                  Thai_yoyak ย THAI CHARACTER YO YAK */
    { 0x0dc3, 0x0e23 }, /*                  Thai_rorua ร THAI CHARACTER RO RUA */
    { 0x0dc4, 0x0e24 }, /*                     Thai_ru ฤ THAI CHARACTER RU */
    { 0x0dc5, 0x0e25 }, /*                 Thai_loling ล THAI CHARACTER LO LING */
    { 0x0dc6, 0x0e26 }, /*                     Thai_lu ฦ THAI CHARACTER LU */
    { 0x0dc7, 0x0e27 }, /*                 Thai_wowaen ว THAI CHARACTER WO WAEN */
    { 0x0dc8, 0x0e28 }, /*                 Thai_sosala ศ THAI CHARACTER SO SALA */
    { 0x0dc9, 0x0e29 }, /*                 Thai_sorusi ษ THAI CHARACTER SO RUSI */
    { 0x0dca, 0x0e2a }, /*                  Thai_sosua ส THAI CHARACTER SO SUA */
    { 0x0dcb, 0x0e2b }, /*                  Thai_hohip ห THAI CHARACTER HO HIP */
    { 0x0dcc, 0x0e2c }, /*                Thai_lochula ฬ THAI CHARACTER LO CHULA */
    { 0x0dcd, 0x0e2d }, /*                   Thai_oang อ THAI CHARACTER O ANG */
    { 0x0dce, 0x0e2e }, /*               Thai_honokhuk ฮ THAI CHARACTER HO NOKHUK */
    { 0x0dcf, 0x0e2f }, /*              Thai_paiyannoi ฯ THAI CHARACTER PAIYANNOI */
    { 0x0dd0, 0x0e30 }, /*                  Thai_saraa ะ THAI CHARACTER SARA A */
    { 0x0dd1, 0x0e31 }, /*             Thai_maihanakat ั THAI CHARACTER MAI HAN-AKAT */
    { 0x0dd2, 0x0e32 }, /*                 Thai_saraaa า THAI CHARACTER SARA AA */
    { 0x0dd3, 0x0e33 }, /*                 Thai_saraam ำ THAI CHARACTER SARA AM */
    { 0x0dd4, 0x0e34 }, /*                  Thai_sarai ิ THAI CHARACTER SARA I */
    { 0x0dd5, 0x0e35 }, /*                 Thai_saraii ี THAI CHARACTER SARA II */
    { 0x0dd6, 0x0e36 }, /*                 Thai_saraue ึ THAI CHARACTER SARA UE */
    { 0x0dd7, 0x0e37 }, /*                Thai_sarauee ื THAI CHARACTER SARA UEE */
    { 0x0dd8, 0x0e38 }, /*                  Thai_sarau ุ THAI CHARACTER SARA U */
    { 0x0dd9, 0x0e39 }, /*                 Thai_sarauu ู THAI CHARACTER SARA UU */
    { 0x0dda, 0x0e3a }, /*                Thai_phinthu ฺ THAI CHARACTER PHINTHU */
    /*  0x0dde                    Thai_maihanakat_maitho ? ??? */
    { 0x0ddf, 0x0e3f }, /*                   Thai_baht ฿ THAI CURRENCY SYMBOL BAHT */
    { 0x0de0, 0x0e40 }, /*                  Thai_sarae เ THAI CHARACTER SARA E */
    { 0x0de1, 0x0e41 }, /*                 Thai_saraae แ THAI CHARACTER SARA AE */
    { 0x0de2, 0x0e42 }, /*                  Thai_sarao โ THAI CHARACTER SARA O */
    { 0x0de3, 0x0e43 }, /*          Thai_saraaimaimuan ใ THAI CHARACTER SARA AI MAIMUAN */
    { 0x0de4, 0x0e44 }, /*         Thai_saraaimaimalai ไ THAI CHARACTER SARA AI MAIMALAI */
    { 0x0de5, 0x0e45 }, /*            Thai_lakkhangyao ๅ THAI CHARACTER LAKKHANGYAO */
    { 0x0de6, 0x0e46 }, /*               Thai_maiyamok ๆ THAI CHARACTER MAIYAMOK */
    { 0x0de7, 0x0e47 }, /*              Thai_maitaikhu ็ THAI CHARACTER MAITAIKHU */
    { 0x0de8, 0x0e48 }, /*                  Thai_maiek ่ THAI CHARACTER MAI EK */
    { 0x0de9, 0x0e49 }, /*                 Thai_maitho ้ THAI CHARACTER MAI THO */
    { 0x0dea, 0x0e4a }, /*                 Thai_maitri ๊ THAI CHARACTER MAI TRI */
    { 0x0deb, 0x0e4b }, /*            Thai_maichattawa ๋ THAI CHARACTER MAI CHATTAWA */
    { 0x0dec, 0x0e4c }, /*            Thai_thanthakhat ์ THAI CHARACTER THANTHAKHAT */
    { 0x0ded, 0x0e4d }, /*               Thai_nikhahit ํ THAI CHARACTER NIKHAHIT */
    { 0x0df0, 0x0e50 }, /*                 Thai_leksun ๐ THAI DIGIT ZERO */
    { 0x0df1, 0x0e51 }, /*                Thai_leknung ๑ THAI DIGIT ONE */
    { 0x0df2, 0x0e52 }, /*                Thai_leksong ๒ THAI DIGIT TWO */
    { 0x0df3, 0x0e53 }, /*                 Thai_leksam ๓ THAI DIGIT THREE */
    { 0x0df4, 0x0e54 }, /*                  Thai_leksi ๔ THAI DIGIT FOUR */
    { 0x0df5, 0x0e55 }, /*                  Thai_lekha ๕ THAI DIGIT FIVE */
    { 0x0df6, 0x0e56 }, /*                 Thai_lekhok ๖ THAI DIGIT SIX */
    { 0x0df7, 0x0e57 }, /*                Thai_lekchet ๗ THAI DIGIT SEVEN */
    { 0x0df8, 0x0e58 }, /*                Thai_lekpaet ๘ THAI DIGIT EIGHT */
    { 0x0df9, 0x0e59 }, /*                 Thai_lekkao ๙ THAI DIGIT NINE */
    { 0x0ea1, 0x3131 }, /*               Hangul_Kiyeog ㄱ HANGUL LETTER KIYEOK */
    { 0x0ea2, 0x3132 }, /*          Hangul_SsangKiyeog ㄲ HANGUL LETTER SSANGKIYEOK */
    { 0x0ea3, 0x3133 }, /*           Hangul_KiyeogSios ㄳ HANGUL LETTER KIYEOK-SIOS */
    { 0x0ea4, 0x3134 }, /*                Hangul_Nieun ㄴ HANGUL LETTER NIEUN */
    { 0x0ea5, 0x3135 }, /*           Hangul_NieunJieuj ㄵ HANGUL LETTER NIEUN-CIEUC */
    { 0x0ea6, 0x3136 }, /*           Hangul_NieunHieuh ㄶ HANGUL LETTER NIEUN-HIEUH */
    { 0x0ea7, 0x3137 }, /*               Hangul_Dikeud ㄷ HANGUL LETTER TIKEUT */
    { 0x0ea8, 0x3138 }, /*          Hangul_SsangDikeud ㄸ HANGUL LETTER SSANGTIKEUT */
    { 0x0ea9, 0x3139 }, /*                Hangul_Rieul ㄹ HANGUL LETTER RIEUL */
    { 0x0eaa, 0x313a }, /*          Hangul_RieulKiyeog ㄺ HANGUL LETTER RIEUL-KIYEOK */
    { 0x0eab, 0x313b }, /*           Hangul_RieulMieum ㄻ HANGUL LETTER RIEUL-MIEUM */
    { 0x0eac, 0x313c }, /*           Hangul_RieulPieub ㄼ HANGUL LETTER RIEUL-PIEUP */
    { 0x0ead, 0x313d }, /*            Hangul_RieulSios ㄽ HANGUL LETTER RIEUL-SIOS */
    { 0x0eae, 0x313e }, /*           Hangul_RieulTieut ㄾ HANGUL LETTER RIEUL-THIEUTH */
    { 0x0eaf, 0x313f }, /*          Hangul_RieulPhieuf ㄿ HANGUL LETTER RIEUL-PHIEUPH */
    { 0x0eb0, 0x3140 }, /*           Hangul_RieulHieuh ㅀ HANGUL LETTER RIEUL-HIEUH */
    { 0x0eb1, 0x3141 }, /*                Hangul_Mieum ㅁ HANGUL LETTER MIEUM */
    { 0x0eb2, 0x3142 }, /*                Hangul_Pieub ㅂ HANGUL LETTER PIEUP */
    { 0x0eb3, 0x3143 }, /*           Hangul_SsangPieub ㅃ HANGUL LETTER SSANGPIEUP */
    { 0x0eb4, 0x3144 }, /*            Hangul_PieubSios ㅄ HANGUL LETTER PIEUP-SIOS */
    { 0x0eb5, 0x3145 }, /*                 Hangul_Sios ㅅ HANGUL LETTER SIOS */
    { 0x0eb6, 0x3146 }, /*            Hangul_SsangSios ㅆ HANGUL LETTER SSANGSIOS */
    { 0x0eb7, 0x3147 }, /*                Hangul_Ieung ㅇ HANGUL LETTER IEUNG */
    { 0x0eb8, 0x3148 }, /*                Hangul_Jieuj ㅈ HANGUL LETTER CIEUC */
    { 0x0eb9, 0x3149 }, /*           Hangul_SsangJieuj ㅉ HANGUL LETTER SSANGCIEUC */
    { 0x0eba, 0x314a }, /*                Hangul_Cieuc ㅊ HANGUL LETTER CHIEUCH */
    { 0x0ebb, 0x314b }, /*               Hangul_Khieuq ㅋ HANGUL LETTER KHIEUKH */
    { 0x0ebc, 0x314c }, /*                Hangul_Tieut ㅌ HANGUL LETTER THIEUTH */
    { 0x0ebd, 0x314d }, /*               Hangul_Phieuf ㅍ HANGUL LETTER PHIEUPH */
    { 0x0ebe, 0x314e }, /*                Hangul_Hieuh ㅎ HANGUL LETTER HIEUH */
    { 0x0ebf, 0x314f }, /*                    Hangul_A ㅏ HANGUL LETTER A */
    { 0x0ec0, 0x3150 }, /*                   Hangul_AE ㅐ HANGUL LETTER AE */
    { 0x0ec1, 0x3151 }, /*                   Hangul_YA ㅑ HANGUL LETTER YA */
    { 0x0ec2, 0x3152 }, /*                  Hangul_YAE ㅒ HANGUL LETTER YAE */
    { 0x0ec3, 0x3153 }, /*                   Hangul_EO ㅓ HANGUL LETTER EO */
    { 0x0ec4, 0x3154 }, /*                    Hangul_E ㅔ HANGUL LETTER E */
    { 0x0ec5, 0x3155 }, /*                  Hangul_YEO ㅕ HANGUL LETTER YEO */
    { 0x0ec6, 0x3156 }, /*                   Hangul_YE ㅖ HANGUL LETTER YE */
    { 0x0ec7, 0x3157 }, /*                    Hangul_O ㅗ HANGUL LETTER O */
    { 0x0ec8, 0x3158 }, /*                   Hangul_WA ㅘ HANGUL LETTER WA */
    { 0x0ec9, 0x3159 }, /*                  Hangul_WAE ㅙ HANGUL LETTER WAE */
    { 0x0eca, 0x315a }, /*                   Hangul_OE ㅚ HANGUL LETTER OE */
    { 0x0ecb, 0x315b }, /*                   Hangul_YO ㅛ HANGUL LETTER YO */
    { 0x0ecc, 0x315c }, /*                    Hangul_U ㅜ HANGUL LETTER U */
    { 0x0ecd, 0x315d }, /*                  Hangul_WEO ㅝ HANGUL LETTER WEO */
    { 0x0ece, 0x315e }, /*                   Hangul_WE ㅞ HANGUL LETTER WE */
    { 0x0ecf, 0x315f }, /*                   Hangul_WI ㅟ HANGUL LETTER WI */
    { 0x0ed0, 0x3160 }, /*                   Hangul_YU ㅠ HANGUL LETTER YU */
    { 0x0ed1, 0x3161 }, /*                   Hangul_EU ㅡ HANGUL LETTER EU */
    { 0x0ed2, 0x3162 }, /*                   Hangul_YI ㅢ HANGUL LETTER YI */
    { 0x0ed3, 0x3163 }, /*                    Hangul_I ㅣ HANGUL LETTER I */
    { 0x0ed4, 0x11a8 }, /*             Hangul_J_Kiyeog ᆨ HANGUL JONGSEONG KIYEOK */
    { 0x0ed5, 0x11a9 }, /*        Hangul_J_SsangKiyeog ᆩ HANGUL JONGSEONG SSANGKIYEOK */
    { 0x0ed6, 0x11aa }, /*         Hangul_J_KiyeogSios ᆪ HANGUL JONGSEONG KIYEOK-SIOS */
    { 0x0ed7, 0x11ab }, /*              Hangul_J_Nieun ᆫ HANGUL JONGSEONG NIEUN */
    { 0x0ed8, 0x11ac }, /*         Hangul_J_NieunJieuj ᆬ HANGUL JONGSEONG NIEUN-CIEUC */
    { 0x0ed9, 0x11ad }, /*         Hangul_J_NieunHieuh ᆭ HANGUL JONGSEONG NIEUN-HIEUH */
    { 0x0eda, 0x11ae }, /*             Hangul_J_Dikeud ᆮ HANGUL JONGSEONG TIKEUT */
    { 0x0edb, 0x11af }, /*              Hangul_J_Rieul ᆯ HANGUL JONGSEONG RIEUL */
    { 0x0edc, 0x11b0 }, /*        Hangul_J_RieulKiyeog ᆰ HANGUL JONGSEONG RIEUL-KIYEOK */
    { 0x0edd, 0x11b1 }, /*         Hangul_J_RieulMieum ᆱ HANGUL JONGSEONG RIEUL-MIEUM */
    { 0x0ede, 0x11b2 }, /*         Hangul_J_RieulPieub ᆲ HANGUL JONGSEONG RIEUL-PIEUP */
    { 0x0edf, 0x11b3 }, /*          Hangul_J_RieulSios ᆳ HANGUL JONGSEONG RIEUL-SIOS */
    { 0x0ee0, 0x11b4 }, /*         Hangul_J_RieulTieut ᆴ HANGUL JONGSEONG RIEUL-THIEUTH */
    { 0x0ee1, 0x11b5 }, /*        Hangul_J_RieulPhieuf ᆵ HANGUL JONGSEONG RIEUL-PHIEUPH */
    { 0x0ee2, 0x11b6 }, /*         Hangul_J_RieulHieuh ᆶ HANGUL JONGSEONG RIEUL-HIEUH */
    { 0x0ee3, 0x11b7 }, /*              Hangul_J_Mieum ᆷ HANGUL JONGSEONG MIEUM */
    { 0x0ee4, 0x11b8 }, /*              Hangul_J_Pieub ᆸ HANGUL JONGSEONG PIEUP */
    { 0x0ee5, 0x11b9 }, /*          Hangul_J_PieubSios ᆹ HANGUL JONGSEONG PIEUP-SIOS */
    { 0x0ee6, 0x11ba }, /*               Hangul_J_Sios ᆺ HANGUL JONGSEONG SIOS */
    { 0x0ee7, 0x11bb }, /*          Hangul_J_SsangSios ᆻ HANGUL JONGSEONG SSANGSIOS */
    { 0x0ee8, 0x11bc }, /*              Hangul_J_Ieung ᆼ HANGUL JONGSEONG IEUNG */
    { 0x0ee9, 0x11bd }, /*              Hangul_J_Jieuj ᆽ HANGUL JONGSEONG CIEUC */
    { 0x0eea, 0x11be }, /*              Hangul_J_Cieuc ᆾ HANGUL JONGSEONG CHIEUCH */
    { 0x0eeb, 0x11bf }, /*             Hangul_J_Khieuq ᆿ HANGUL JONGSEONG KHIEUKH */
    { 0x0eec, 0x11c0 }, /*              Hangul_J_Tieut ᇀ HANGUL JONGSEONG THIEUTH */
    { 0x0eed, 0x11c1 }, /*             Hangul_J_Phieuf ᇁ HANGUL JONGSEONG PHIEUPH */
    { 0x0eee, 0x11c2 }, /*              Hangul_J_Hieuh ᇂ HANGUL JONGSEONG HIEUH */
    { 0x0eef, 0x316d }, /*     Hangul_RieulYeorinHieuh ㅭ HANGUL LETTER RIEUL-YEORINHIEUH */
    { 0x0ef0, 0x3171 }, /*    Hangul_SunkyeongeumMieum ㅱ HANGUL LETTER KAPYEOUNMIEUM */
    { 0x0ef1, 0x3178 }, /*    Hangul_SunkyeongeumPieub ㅸ HANGUL LETTER KAPYEOUNPIEUP */
    { 0x0ef2, 0x317f }, /*              Hangul_PanSios ㅿ HANGUL LETTER PANSIOS */
    { 0x0ef3, 0x3181 }, /*    Hangul_KkogjiDalrinIeung ㆁ HANGUL LETTER YESIEUNG */
    { 0x0ef4, 0x3184 }, /*   Hangul_SunkyeongeumPhieuf ㆄ HANGUL LETTER KAPYEOUNPHIEUPH */
    { 0x0ef5, 0x3186 }, /*          Hangul_YeorinHieuh ㆆ HANGUL LETTER YEORINHIEUH */
    { 0x0ef6, 0x318d }, /*                Hangul_AraeA ㆍ HANGUL LETTER ARAEA */
    { 0x0ef7, 0x318e }, /*               Hangul_AraeAE ㆎ HANGUL LETTER ARAEAE */
    { 0x0ef8, 0x11eb }, /*            Hangul_J_PanSios ᇫ HANGUL JONGSEONG PANSIOS */
    { 0x0ef9, 0x11f0 }, /*  Hangul_J_KkogjiDalrinIeung ᇰ HANGUL JONGSEONG YESIEUNG */
    { 0x0efa, 0x11f9 }, /*        Hangul_J_YeorinHieuh ᇹ HANGUL JONGSEONG YEORINHIEUH */
    { 0x0eff, 0x20a9 }, /*                  Korean_Won ₩ WON SIGN */
    { 0x13a4, 0x20ac }, /*                        Euro € EURO SIGN */
    { 0x13bc, 0x0152 }, /*                          OE Œ LATIN CAPITAL LIGATURE OE */
    { 0x13bd, 0x0153 }, /*                          oe œ LATIN SMALL LIGATURE OE */
    { 0x13be, 0x0178 }, /*                  Ydiaeresis Ÿ LATIN CAPITAL LETTER Y WITH DIAERESIS */
    { 0x20ac, 0x20ac }, /*                    EuroSign € EURO SIGN */
    };

struct codepairbyucs {
    unsigned short keysym;
    unsigned short ucs;
} keysymtabbyucs[] = {
    { 0xff08, 0x0008 }, /* f BackSpace */
    { 0xff09, 0x0009 }, /* f Tab */
    { 0xff0d, 0x000a }, /* f Linefeed */
    { 0xff0b, 0x000b }, /* f Clear */
    { 0xff0d, 0x000d }, /* f Return */
    { 0xff13, 0x0013 }, /* f Pause */
    { 0xff14, 0x0014 }, /* f Scroll_Lock */
    { 0xff15, 0x0015 }, /* f Sys_Req */
    { 0xff1b, 0x001b }, /* f Escape */
    { 0x0020, 0x0020 }, /* . space */
    { 0x0021, 0x0021 }, /* . exclam */
    { 0x0022, 0x0022 }, /* . quotedbl */
    { 0x0023, 0x0023 }, /* . numbersign */
    { 0x0024, 0x0024 }, /* . dollar */
    { 0x0025, 0x0025 }, /* . percent */
    { 0x0026, 0x0026 }, /* . ampersand */
    { 0x0027, 0x0027 }, /* . apostrophe, quoteright */
    { 0x0028, 0x0028 }, /* . parenleft */
    { 0x0029, 0x0029 }, /* . parenright */
    { 0x002a, 0x002a }, /* . asterisk */
    { 0x002b, 0x002b }, /* . plus */
    { 0x002c, 0x002c }, /* . comma */
    { 0x002d, 0x002d }, /* . minus */
    { 0x002e, 0x002e }, /* . period */
    { 0x002f, 0x002f }, /* . slash */
    { 0x0030, 0x0030 }, /* . 0 */
    { 0x0031, 0x0031 }, /* . 1 */
    { 0x0032, 0x0032 }, /* . 2 */
    { 0x0033, 0x0033 }, /* . 3 */
    { 0x0034, 0x0034 }, /* . 4 */
    { 0x0035, 0x0035 }, /* . 5 */
    { 0x0036, 0x0036 }, /* . 6 */
    { 0x0037, 0x0037 }, /* . 7 */
    { 0x0038, 0x0038 }, /* . 8 */
    { 0x0039, 0x0039 }, /* . 9 */
    { 0x003a, 0x003a }, /* . colon */
    { 0x003b, 0x003b }, /* . semicolon */
    { 0x0ba3, 0x003c }, /*                   leftcaret < LESS-THAN SIGN */
    { 0x003d, 0x003d }, /* . equal */
    { 0x0ba6, 0x003e }, /*                  rightcaret > GREATER-THAN SIGN */
    { 0x003f, 0x003f }, /* . question */
    { 0x0040, 0x0040 }, /* . at */
    { 0x0041, 0x0041 }, /* . A */
    { 0x0042, 0x0042 }, /* . B */
    { 0x0043, 0x0043 }, /* . C */
    { 0x0044, 0x0044 }, /* . D */
    { 0x0045, 0x0045 }, /* . E */
    { 0x0046, 0x0046 }, /* . F */
    { 0x0047, 0x0047 }, /* . G */
    { 0x0048, 0x0048 }, /* . H */
    { 0x0049, 0x0049 }, /* . I */
    { 0x004a, 0x004a }, /* . J */
    { 0x004b, 0x004b }, /* . K */
    { 0x004c, 0x004c }, /* . L */
    { 0x004d, 0x004d }, /* . M */
    { 0x004e, 0x004e }, /* . N */
    { 0x004f, 0x004f }, /* . O */
    { 0x0050, 0x0050 }, /* . P */
    { 0x0051, 0x0051 }, /* . Q */
    { 0x0052, 0x0052 }, /* . R */
    { 0x0053, 0x0053 }, /* . S */
    { 0x0054, 0x0054 }, /* . T */
    { 0x0055, 0x0055 }, /* . U */
    { 0x0056, 0x0056 }, /* . V */
    { 0x0057, 0x0057 }, /* . W */
    { 0x0058, 0x0058 }, /* . X */
    { 0x0059, 0x0059 }, /* . Y */
    { 0x005a, 0x005a }, /* . Z */
    { 0x005b, 0x005b }, /* . bracketleft */
    { 0x005c, 0x005c }, /* . backslash */
    { 0x005d, 0x005d }, /* . bracketright */
    { 0x005e, 0x005e }, /* . asciicircum */
    { 0x005f, 0x005f }, /* . underscore */
    { 0x0060, 0x0060 }, /* . grave/quoteleft */
    { 0x0061, 0x0061 }, /* . a */
    { 0x0062, 0x0062 }, /* . b */
    { 0x0063, 0x0063 }, /* . c */
    { 0x0064, 0x0064 }, /* . d */
    { 0x0065, 0x0065 }, /* . e */
    { 0x0066, 0x0066 }, /* . f */
    { 0x0067, 0x0067 }, /* . g */
    { 0x0068, 0x0068 }, /* . h */
    { 0x0069, 0x0069 }, /* . i */
    { 0x006a, 0x006a }, /* . j */
    { 0x006b, 0x006b }, /* . k */
    { 0x006c, 0x006c }, /* . l */
    { 0x006d, 0x006d }, /* . m */
    { 0x006e, 0x006e }, /* . n */
    { 0x006f, 0x006f }, /* . o */
    { 0x0070, 0x0070 }, /* . p */
    { 0x0071, 0x0071 }, /* . q */
    { 0x0072, 0x0072 }, /* . r */
    { 0x0073, 0x0073 }, /* . s */
    { 0x0074, 0x0074 }, /* . t */
    { 0x0075, 0x0075 }, /* . u */
    { 0x0076, 0x0076 }, /* . v */
    { 0x0077, 0x0077 }, /* . w */
    { 0x0078, 0x0078 }, /* . x */
    { 0x0079, 0x0079 }, /* . y */
    { 0x007a, 0x007a }, /* . z */
    { 0x007b, 0x007b }, /* . braceleft */
    { 0x007c, 0x007c }, /* . bar */
    { 0x007d, 0x007d }, /* . braceright */
    { 0x007e, 0x007e }, /* . asciitilde */
    { 0x00a0, 0x00a0 }, /* . nobreakspace */
    { 0x00a1, 0x00a1 }, /* . exclamdown */
    { 0x00a2, 0x00a2 }, /* . cent */
    { 0x00a3, 0x00a3 }, /* . sterling */
    { 0x00a4, 0x00a4 }, /* . currency */
    { 0x00a5, 0x00a5 }, /* . yen */
    { 0x00a6, 0x00a6 }, /* . brokenbar */
    { 0x00a7, 0x00a7 }, /* . section */
    { 0x00a8, 0x00a8 }, /* . diaeresis */
    { 0x00a9, 0x00a9 }, /* . copyright */
    { 0x00aa, 0x00aa }, /* . ordfeminine */
    { 0x00ab, 0x00ab }, /* . guillemotleft */
    { 0x00ac, 0x00ac }, /* . notsign */
    { 0x00ad, 0x00ad }, /* . hyphen */
    { 0x00ae, 0x00ae }, /* . registered */
    { 0x00af, 0x00af }, /* . macron */
    { 0x00b0, 0x00b0 }, /* . degree */
    { 0x00b1, 0x00b1 }, /* . plusminus */
    { 0x00b2, 0x00b2 }, /* . twosuperior */
    { 0x00b3, 0x00b3 }, /* . threesuperior */
    { 0x00b4, 0x00b4 }, /* . acute */
    { 0x00b5, 0x00b5 }, /* . mu */
    { 0x00b6, 0x00b6 }, /* . paragraph */
    { 0x00b7, 0x00b7 }, /* . periodcentered */
    { 0x00b8, 0x00b8 }, /* . cedilla */
    { 0x00b9, 0x00b9 }, /* . onesuperior */
    { 0x00ba, 0x00ba }, /* . masculine */
    { 0x00bb, 0x00bb }, /* . guillemotright */
    { 0x00bc, 0x00bc }, /* . onequarter */
    { 0x00bd, 0x00bd }, /* . onehalf */
    { 0x00be, 0x00be }, /* . threequarters */
    { 0x00bf, 0x00bf }, /* . questiondown */
    { 0x00c0, 0x00c0 }, /* . Agrave */
    { 0x00c1, 0x00c1 }, /* . Aacute */
    { 0x00c2, 0x00c2 }, /* . Acircumflex */
    { 0x00c3, 0x00c3 }, /* . Atilde */
    { 0x00c4, 0x00c4 }, /* . Adiaeresis */
    { 0x00c5, 0x00c5 }, /* . Aring */
    { 0x00c6, 0x00c6 }, /* . AE */
    { 0x00c7, 0x00c7 }, /* . Ccedilla */
    { 0x00c8, 0x00c8 }, /* . Egrave */
    { 0x00c9, 0x00c9 }, /* . Eacute */
    { 0x00ca, 0x00ca }, /* . Ecircumflex */
    { 0x00cb, 0x00cb }, /* . Ediaeresis */
    { 0x00cc, 0x00cc }, /* . Igrave */
    { 0x00cd, 0x00cd }, /* . Iacute */
    { 0x00ce, 0x00ce }, /* . Icircumflex */
    { 0x00cf, 0x00cf }, /* . Idiaeresis */
    { 0x00d0, 0x00d0 }, /* . ETH */
    { 0x00d1, 0x00d1 }, /* . Ntilde */
    { 0x00d2, 0x00d2 }, /* . Ograve */
    { 0x00d3, 0x00d3 }, /* . Oacute */
    { 0x00d4, 0x00d4 }, /* . Ocircumflex */
    { 0x00d5, 0x00d5 }, /* . Otilde */
    { 0x00d6, 0x00d6 }, /* . Odiaeresis */
    { 0x00d7, 0x00d7 }, /* . multiply */
    { 0x00d8, 0x00d8 }, /* . Ooblique */
    { 0x00d9, 0x00d9 }, /* . Ugrave */
    { 0x00da, 0x00da }, /* . Uacute */
    { 0x00db, 0x00db }, /* . Ucircumflex */
    { 0x00dc, 0x00dc }, /* . Udiaeresis */
    { 0x00dd, 0x00dd }, /* . Yacute */
    { 0x00de, 0x00de }, /* . Thorn */
    { 0x00df, 0x00df }, /* . ssharp */
    { 0x00e0, 0x00e0 }, /* . agrave */
    { 0x00e1, 0x00e1 }, /* . aacute */
    { 0x00e2, 0x00e2 }, /* . acircumflex */
    { 0x00e3, 0x00e3 }, /* . atilde */
    { 0x00e4, 0x00e4 }, /* . adiaeresis */
    { 0x00e5, 0x00e5 }, /* . aring */
    { 0x00e6, 0x00e6 }, /* . ae */
    { 0x00e7, 0x00e7 }, /* . ccedilla */
    { 0x00e8, 0x00e8 }, /* . egrave */
    { 0x00e9, 0x00e9 }, /* . eacute */
    { 0x00ea, 0x00ea }, /* . ecircumflex */
    { 0x00eb, 0x00eb }, /* . ediaeresis */
    { 0x00ec, 0x00ec }, /* . igrave */
    { 0x00ed, 0x00ed }, /* . iacute */
    { 0x00ee, 0x00ee }, /* . icircumflex */
    { 0x00ef, 0x00ef }, /* . idiaeresis */
    { 0x00f0, 0x00f0 }, /* . eth */
    { 0x00f1, 0x00f1 }, /* . ntilde */
    { 0x00f2, 0x00f2 }, /* . ograve */
    { 0x00f3, 0x00f3 }, /* . oacute */
    { 0x00f4, 0x00f4 }, /* . ocircumflex */
    { 0x00f5, 0x00f5 }, /* . otilde */
    { 0x00f6, 0x00f6 }, /* . odiaeresis */
    { 0x00f7, 0x00f7 }, /* . division */
    { 0x00f8, 0x00f8 }, /* . oslash */
    { 0x00f9, 0x00f9 }, /* . ugrave */
    { 0x00fa, 0x00fa }, /* . uacute */
    { 0x00fb, 0x00fb }, /* . ucircumflex */
    { 0x00fc, 0x00fc }, /* . udiaeresis */
    { 0x00fd, 0x00fd }, /* . yacute */
    { 0x00fe, 0x00fe }, /* . thorn */
    { 0x00ff, 0x00ff }, /* . ydiaeresis */
    { 0x03c0, 0x0100 }, /*                     Amacron Ā LATIN CAPITAL LETTER A WITH MACRON */
    { 0x03e0, 0x0101 }, /*                     amacron ā LATIN SMALL LETTER A WITH MACRON */
    { 0x01c3, 0x0102 }, /*                      Abreve Ă LATIN CAPITAL LETTER A WITH BREVE */
    { 0x01e3, 0x0103 }, /*                      abreve ă LATIN SMALL LETTER A WITH BREVE */
    { 0x01a1, 0x0104 }, /*                     Aogonek Ą LATIN CAPITAL LETTER A WITH OGONEK */
    { 0x01b1, 0x0105 }, /*                     aogonek ą LATIN SMALL LETTER A WITH OGONEK */
    { 0x01c6, 0x0106 }, /*                      Cacute Ć LATIN CAPITAL LETTER C WITH ACUTE */
    { 0x01e6, 0x0107 }, /*                      cacute ć LATIN SMALL LETTER C WITH ACUTE */
    { 0x02c6, 0x0108 }, /*                 Ccircumflex Ĉ LATIN CAPITAL LETTER C WITH CIRCUMFLEX */
    { 0x02e6, 0x0109 }, /*                 ccircumflex ĉ LATIN SMALL LETTER C WITH CIRCUMFLEX */
    { 0x02c5, 0x010a }, /*                   Cabovedot Ċ LATIN CAPITAL LETTER C WITH DOT ABOVE */
    { 0x02e5, 0x010b }, /*                   cabovedot ċ LATIN SMALL LETTER C WITH DOT ABOVE */
    { 0x01c8, 0x010c }, /*                      Ccaron Č LATIN CAPITAL LETTER C WITH CARON */
    { 0x01e8, 0x010d }, /*                      ccaron č LATIN SMALL LETTER C WITH CARON */
    { 0x01cf, 0x010e }, /*                      Dcaron Ď LATIN CAPITAL LETTER D WITH CARON */
    { 0x01ef, 0x010f }, /*                      dcaron ď LATIN SMALL LETTER D WITH CARON */
    { 0x01d0, 0x0110 }, /*                     Dstroke Đ LATIN CAPITAL LETTER D WITH STROKE */
    { 0x01f0, 0x0111 }, /*                     dstroke đ LATIN SMALL LETTER D WITH STROKE */
    { 0x03aa, 0x0112 }, /*                     Emacron Ē LATIN CAPITAL LETTER E WITH MACRON */
    { 0x03ba, 0x0113 }, /*                     emacron ē LATIN SMALL LETTER E WITH MACRON */
    { 0x03cc, 0x0116 }, /*                   Eabovedot Ė LATIN CAPITAL LETTER E WITH DOT ABOVE */
    { 0x03ec, 0x0117 }, /*                   eabovedot ė LATIN SMALL LETTER E WITH DOT ABOVE */
    { 0x01ca, 0x0118 }, /*                     Eogonek Ę LATIN CAPITAL LETTER E WITH OGONEK */
    { 0x01ea, 0x0119 }, /*                     eogonek ę LATIN SMALL LETTER E WITH OGONEK */
    { 0x01cc, 0x011a }, /*                      Ecaron Ě LATIN CAPITAL LETTER E WITH CARON */
    { 0x01ec, 0x011b }, /*                      ecaron ě LATIN SMALL LETTER E WITH CARON */
    { 0x02d8, 0x011c }, /*                 Gcircumflex Ĝ LATIN CAPITAL LETTER G WITH CIRCUMFLEX */
    { 0x02f8, 0x011d }, /*                 gcircumflex ĝ LATIN SMALL LETTER G WITH CIRCUMFLEX */
    { 0x02ab, 0x011e }, /*                      Gbreve Ğ LATIN CAPITAL LETTER G WITH BREVE */
    { 0x02bb, 0x011f }, /*                      gbreve ğ LATIN SMALL LETTER G WITH BREVE */
    { 0x02d5, 0x0120 }, /*                   Gabovedot Ġ LATIN CAPITAL LETTER G WITH DOT ABOVE */
    { 0x02f5, 0x0121 }, /*                   gabovedot ġ LATIN SMALL LETTER G WITH DOT ABOVE */
    { 0x03ab, 0x0122 }, /*                    Gcedilla Ģ LATIN CAPITAL LETTER G WITH CEDILLA */
    { 0x03bb, 0x0123 }, /*                    gcedilla ģ LATIN SMALL LETTER G WITH CEDILLA */
    { 0x02a6, 0x0124 }, /*                 Hcircumflex Ĥ LATIN CAPITAL LETTER H WITH CIRCUMFLEX */
    { 0x02b6, 0x0125 }, /*                 hcircumflex ĥ LATIN SMALL LETTER H WITH CIRCUMFLEX */
    { 0x02a1, 0x0126 }, /*                     Hstroke Ħ LATIN CAPITAL LETTER H WITH STROKE */
    { 0x02b1, 0x0127 }, /*                     hstroke ħ LATIN SMALL LETTER H WITH STROKE */
    { 0x03a5, 0x0128 }, /*                      Itilde Ĩ LATIN CAPITAL LETTER I WITH TILDE */
    { 0x03b5, 0x0129 }, /*                      itilde ĩ LATIN SMALL LETTER I WITH TILDE */
    { 0x03cf, 0x012a }, /*                     Imacron Ī LATIN CAPITAL LETTER I WITH MACRON */
    { 0x03ef, 0x012b }, /*                     imacron ī LATIN SMALL LETTER I WITH MACRON */
    { 0x16a6, 0x012c }, /* u Ibreve */
    { 0x16b6, 0x012d }, /* u ibreve */
    { 0x03c7, 0x012e }, /*                     Iogonek Į LATIN CAPITAL LETTER I WITH OGONEK */
    { 0x03e7, 0x012f }, /*                     iogonek į LATIN SMALL LETTER I WITH OGONEK */
    { 0x02a9, 0x0130 }, /*                   Iabovedot İ LATIN CAPITAL LETTER I WITH DOT ABOVE */
    { 0x02b9, 0x0131 }, /*                    idotless ı LATIN SMALL LETTER DOTLESS I */
    { 0x02ac, 0x0134 }, /*                 Jcircumflex Ĵ LATIN CAPITAL LETTER J WITH CIRCUMFLEX */
    { 0x02bc, 0x0135 }, /*                 jcircumflex ĵ LATIN SMALL LETTER J WITH CIRCUMFLEX */
    { 0x03d3, 0x0136 }, /*                    Kcedilla Ķ LATIN CAPITAL LETTER K WITH CEDILLA */
    { 0x03f3, 0x0137 }, /*                    kcedilla ķ LATIN SMALL LETTER K WITH CEDILLA */
    { 0x03a2, 0x0138 }, /*                         kra ĸ LATIN SMALL LETTER KRA */
    { 0x01c5, 0x0139 }, /*                      Lacute Ĺ LATIN CAPITAL LETTER L WITH ACUTE */
    { 0x01e5, 0x013a }, /*                      lacute ĺ LATIN SMALL LETTER L WITH ACUTE */
    { 0x03a6, 0x013b }, /*                    Lcedilla Ļ LATIN CAPITAL LETTER L WITH CEDILLA */
    { 0x03b6, 0x013c }, /*                    lcedilla ļ LATIN SMALL LETTER L WITH CEDILLA */
    { 0x01a5, 0x013d }, /*                      Lcaron Ľ LATIN CAPITAL LETTER L WITH CARON */
    { 0x01b5, 0x013e }, /*                      lcaron ľ LATIN SMALL LETTER L WITH CARON */
    { 0x01a3, 0x0141 }, /*                     Lstroke Ł LATIN CAPITAL LETTER L WITH STROKE */
    { 0x01b3, 0x0142 }, /*                     lstroke ł LATIN SMALL LETTER L WITH STROKE */
    { 0x01d1, 0x0143 }, /*                      Nacute Ń LATIN CAPITAL LETTER N WITH ACUTE */
    { 0x01f1, 0x0144 }, /*                      nacute ń LATIN SMALL LETTER N WITH ACUTE */
    { 0x03d1, 0x0145 }, /*                    Ncedilla Ņ LATIN CAPITAL LETTER N WITH CEDILLA */
    { 0x03f1, 0x0146 }, /*                    ncedilla ņ LATIN SMALL LETTER N WITH CEDILLA */
    { 0x01d2, 0x0147 }, /*                      Ncaron Ň LATIN CAPITAL LETTER N WITH CARON */
    { 0x01f2, 0x0148 }, /*                      ncaron ň LATIN SMALL LETTER N WITH CARON */
    { 0x03bd, 0x014a }, /*                         ENG Ŋ LATIN CAPITAL LETTER ENG */
    { 0x03bf, 0x014b }, /*                         eng ŋ LATIN SMALL LETTER ENG */
    { 0x03d2, 0x014c }, /*                     Omacron Ō LATIN CAPITAL LETTER O WITH MACRON */
    { 0x03f2, 0x014d }, /*                     omacron ō LATIN SMALL LETTER O WITH MACRON */
    { 0x01d5, 0x0150 }, /*                Odoubleacute Ő LATIN CAPITAL LETTER O WITH DOUBLE ACUTE */
    { 0x01f5, 0x0151 }, /*                odoubleacute ő LATIN SMALL LETTER O WITH DOUBLE ACUTE */
    { 0x13bc, 0x0152 }, /*                          OE Œ LATIN CAPITAL LIGATURE OE */
    { 0x13bd, 0x0153 }, /*                          oe œ LATIN SMALL LIGATURE OE */
    { 0x01c0, 0x0154 }, /*                      Racute Ŕ LATIN CAPITAL LETTER R WITH ACUTE */
    { 0x01e0, 0x0155 }, /*                      racute ŕ LATIN SMALL LETTER R WITH ACUTE */
    { 0x03a3, 0x0156 }, /*                    Rcedilla Ŗ LATIN CAPITAL LETTER R WITH CEDILLA */
    { 0x03b3, 0x0157 }, /*                    rcedilla ŗ LATIN SMALL LETTER R WITH CEDILLA */
    { 0x01d8, 0x0158 }, /*                      Rcaron Ř LATIN CAPITAL LETTER R WITH CARON */
    { 0x01f8, 0x0159 }, /*                      rcaron ř LATIN SMALL LETTER R WITH CARON */
    { 0x01a6, 0x015a }, /*                      Sacute Ś LATIN CAPITAL LETTER S WITH ACUTE */
    { 0x01b6, 0x015b }, /*                      sacute ś LATIN SMALL LETTER S WITH ACUTE */
    { 0x02de, 0x015c }, /*                 Scircumflex Ŝ LATIN CAPITAL LETTER S WITH CIRCUMFLEX */
    { 0x02fe, 0x015d }, /*                 scircumflex ŝ LATIN SMALL LETTER S WITH CIRCUMFLEX */
    { 0x01aa, 0x015e }, /*                    Scedilla Ş LATIN CAPITAL LETTER S WITH CEDILLA */
    { 0x01ba, 0x015f }, /*                    scedilla ş LATIN SMALL LETTER S WITH CEDILLA */
    { 0x01a9, 0x0160 }, /*                      Scaron Š LATIN CAPITAL LETTER S WITH CARON */
    { 0x01b9, 0x0161 }, /*                      scaron š LATIN SMALL LETTER S WITH CARON */
    { 0x01de, 0x0162 }, /*                    Tcedilla Ţ LATIN CAPITAL LETTER T WITH CEDILLA */
    { 0x01fe, 0x0163 }, /*                    tcedilla ţ LATIN SMALL LETTER T WITH CEDILLA */
    { 0x01ab, 0x0164 }, /*                      Tcaron Ť LATIN CAPITAL LETTER T WITH CARON */
    { 0x01bb, 0x0165 }, /*                      tcaron ť LATIN SMALL LETTER T WITH CARON */
    { 0x03ac, 0x0166 }, /*                      Tslash Ŧ LATIN CAPITAL LETTER T WITH STROKE */
    { 0x03bc, 0x0167 }, /*                      tslash ŧ LATIN SMALL LETTER T WITH STROKE */
    { 0x03dd, 0x0168 }, /*                      Utilde Ũ LATIN CAPITAL LETTER U WITH TILDE */
    { 0x03fd, 0x0169 }, /*                      utilde ũ LATIN SMALL LETTER U WITH TILDE */
    { 0x03de, 0x016a }, /*                     Umacron Ū LATIN CAPITAL LETTER U WITH MACRON */
    { 0x03fe, 0x016b }, /*                     umacron ū LATIN SMALL LETTER U WITH MACRON */
    { 0x02dd, 0x016c }, /*                      Ubreve Ŭ LATIN CAPITAL LETTER U WITH BREVE */
    { 0x02fd, 0x016d }, /*                      ubreve ŭ LATIN SMALL LETTER U WITH BREVE */
    { 0x01d9, 0x016e }, /*                       Uring Ů LATIN CAPITAL LETTER U WITH RING ABOVE */
    { 0x01f9, 0x016f }, /*                       uring ů LATIN SMALL LETTER U WITH RING ABOVE */
    { 0x01db, 0x0170 }, /*                Udoubleacute Ű LATIN CAPITAL LETTER U WITH DOUBLE ACUTE */
    { 0x01fb, 0x0171 }, /*                udoubleacute ű LATIN SMALL LETTER U WITH DOUBLE ACUTE */
    { 0x03d9, 0x0172 }, /*                     Uogonek Ų LATIN CAPITAL LETTER U WITH OGONEK */
    { 0x03f9, 0x0173 }, /*                     uogonek ų LATIN SMALL LETTER U WITH OGONEK */
    { 0x12d0, 0x0174 }, /* u Wcircumflex */
    { 0x12f0, 0x0175 }, /* u wcircumflex */
    { 0x12de, 0x0176 }, /* u Ycircumflex */
    { 0x12fe, 0x0177 }, /* u ycircumflex */
    { 0x13be, 0x0178 }, /*                  Ydiaeresis Ÿ LATIN CAPITAL LETTER Y WITH DIAERESIS */
    { 0x01ac, 0x0179 }, /*                      Zacute Ź LATIN CAPITAL LETTER Z WITH ACUTE */
    { 0x01bc, 0x017a }, /*                      zacute ź LATIN SMALL LETTER Z WITH ACUTE */
    { 0x01af, 0x017b }, /*                   Zabovedot Ż LATIN CAPITAL LETTER Z WITH DOT ABOVE */
    { 0x01bf, 0x017c }, /*                   zabovedot ż LATIN SMALL LETTER Z WITH DOT ABOVE */
    { 0x01ae, 0x017d }, /*                      Zcaron Ž LATIN CAPITAL LETTER Z WITH CARON */
    { 0x01be, 0x017e }, /*                      zcaron ž LATIN SMALL LETTER Z WITH CARON */
    { 0x16c6, 0x018f }, /* u SCHWA */
    { 0x08f6, 0x0192 }, /*                    function ƒ LATIN SMALL LETTER F WITH HOOK */
    { 0x16af, 0x019f }, /* u Obarred */
    { 0x1efa, 0x01a0 }, /* u Ohorn */
    { 0x1efb, 0x01a1 }, /* u ohorn */
    { 0x1efc, 0x01af }, /* u Uhorn */
    { 0x1efd, 0x01b0 }, /* u uhorn */
    { 0x16a9, 0x01b5 }, /* u Zstroke */
    { 0x16b9, 0x01b6 }, /* u zstroke */
    { 0x16bd, 0x01d2 }, /* u ocaron */
    { 0x16aa, 0x01e6 }, /* u Gcaron */
    { 0x16ba, 0x01e7 }, /* u gcaron */
    { 0x16f6, 0x0259 }, /* u schwa */
    { 0x16bf, 0x0275 }, /* u obarred */
    { 0x01b7, 0x02c7 }, /*                       caron ˇ CARON */
    { 0x01a2, 0x02d8 }, /*                       breve ˘ BREVE */
    { 0x01ff, 0x02d9 }, /*                    abovedot ˙ DOT ABOVE */
    { 0x01b2, 0x02db }, /*                      ogonek ˛ OGONEK */
    { 0x01bd, 0x02dd }, /*                 doubleacute ˝ DOUBLE ACUTE ACCENT */
    { 0xfe50, 0x0300 }, /* f dead_grave */
    { 0xfe51, 0x0301 }, /* f dead_acute */
    { 0xfe52, 0x0302 }, /* f dead_circumflex */
    { 0xfe53, 0x0303 }, /* f dead_tilde */
    { 0xfe54, 0x0304 }, /* f dead_macron */
    { 0xfe55, 0x0306 }, /* f dead_breve */
    { 0xfe56, 0x0307 }, /* f dead_abovedot */
    { 0xfe57, 0x0308 }, /* f dead_diaeresis */
    { 0xfe61, 0x0309 }, /* f dead_hook */
    { 0xfe58, 0x030a }, /* f dead_abovering */
    { 0xfe59, 0x030b }, /* f dead_doubleacute */
    { 0xfe5a, 0x030c }, /* f dead_caron */
    { 0xfe62, 0x031b }, /* f dead_horn */
    { 0xfe60, 0x0323 }, /* f dead_belowdot */
    { 0xfe5b, 0x0327 }, /* f dead_cedilla */
    { 0xfe5c, 0x0328 }, /* f dead_ogonek */
    { 0xfe5d, 0x0345 }, /* f dead_iota */
    { 0x07ae, 0x0385 }, /*        Greek_accentdieresis ΅ GREEK DIALYTIKA TONOS */
    { 0x07a1, 0x0386 }, /*           Greek_ALPHAaccent Ά GREEK CAPITAL LETTER ALPHA WITH TONOS */
    { 0x07a2, 0x0388 }, /*         Greek_EPSILONaccent Έ GREEK CAPITAL LETTER EPSILON WITH TONOS */
    { 0x07a3, 0x0389 }, /*             Greek_ETAaccent Ή GREEK CAPITAL LETTER ETA WITH TONOS */
    { 0x07a4, 0x038a }, /*            Greek_IOTAaccent Ί GREEK CAPITAL LETTER IOTA WITH TONOS */
    { 0x07a7, 0x038c }, /*         Greek_OMICRONaccent Ό GREEK CAPITAL LETTER OMICRON WITH TONOS */
    { 0x07a8, 0x038e }, /*         Greek_UPSILONaccent Ύ GREEK CAPITAL LETTER UPSILON WITH TONOS */
    { 0x07ab, 0x038f }, /*           Greek_OMEGAaccent Ώ GREEK CAPITAL LETTER OMEGA WITH TONOS */
    { 0x07b6, 0x0390 }, /*    Greek_iotaaccentdieresis ΐ GREEK SMALL LETTER IOTA WITH DIALYTIKA AND TONOS */
    { 0x07c1, 0x0391 }, /*                 Greek_ALPHA Α GREEK CAPITAL LETTER ALPHA */
    { 0x07c2, 0x0392 }, /*                  Greek_BETA Β GREEK CAPITAL LETTER BETA */
    { 0x07c3, 0x0393 }, /*                 Greek_GAMMA Γ GREEK CAPITAL LETTER GAMMA */
    { 0x07c4, 0x0394 }, /*                 Greek_DELTA Δ GREEK CAPITAL LETTER DELTA */
    { 0x07c5, 0x0395 }, /*               Greek_EPSILON Ε GREEK CAPITAL LETTER EPSILON */
    { 0x07c6, 0x0396 }, /*                  Greek_ZETA Ζ GREEK CAPITAL LETTER ZETA */
    { 0x07c7, 0x0397 }, /*                   Greek_ETA Η GREEK CAPITAL LETTER ETA */
    { 0x07c8, 0x0398 }, /*                 Greek_THETA Θ GREEK CAPITAL LETTER THETA */
    { 0x07c9, 0x0399 }, /*                  Greek_IOTA Ι GREEK CAPITAL LETTER IOTA */
    { 0x07ca, 0x039a }, /*                 Greek_KAPPA Κ GREEK CAPITAL LETTER KAPPA */
    { 0x07cb, 0x039b }, /*                Greek_LAMBDA Λ GREEK CAPITAL LETTER LAMDA */
    { 0x07cc, 0x039c }, /*                    Greek_MU Μ GREEK CAPITAL LETTER MU */
    { 0x07cd, 0x039d }, /*                    Greek_NU Ν GREEK CAPITAL LETTER NU */
    { 0x07ce, 0x039e }, /*                    Greek_XI Ξ GREEK CAPITAL LETTER XI */
    { 0x07cf, 0x039f }, /*               Greek_OMICRON Ο GREEK CAPITAL LETTER OMICRON */
    { 0x07d0, 0x03a0 }, /*                    Greek_PI Π GREEK CAPITAL LETTER PI */
    { 0x07d1, 0x03a1 }, /*                   Greek_RHO Ρ GREEK CAPITAL LETTER RHO */
    { 0x07d2, 0x03a3 }, /*                 Greek_SIGMA Σ GREEK CAPITAL LETTER SIGMA */
    { 0x07d4, 0x03a4 }, /*                   Greek_TAU Τ GREEK CAPITAL LETTER TAU */
    { 0x07d5, 0x03a5 }, /*               Greek_UPSILON Υ GREEK CAPITAL LETTER UPSILON */
    { 0x07d6, 0x03a6 }, /*                   Greek_PHI Φ GREEK CAPITAL LETTER PHI */
    { 0x07d7, 0x03a7 }, /*                   Greek_CHI Χ GREEK CAPITAL LETTER CHI */
    { 0x07d8, 0x03a8 }, /*                   Greek_PSI Ψ GREEK CAPITAL LETTER PSI */
    { 0x07d9, 0x03a9 }, /*                 Greek_OMEGA Ω GREEK CAPITAL LETTER OMEGA */
    { 0x07a5, 0x03aa }, /*         Greek_IOTAdiaeresis Ϊ GREEK CAPITAL LETTER IOTA WITH DIALYTIKA */
    { 0x07a9, 0x03ab }, /*       Greek_UPSILONdieresis Ϋ GREEK CAPITAL LETTER UPSILON WITH DIALYTIKA */
    { 0x07b1, 0x03ac }, /*           Greek_alphaaccent ά GREEK SMALL LETTER ALPHA WITH TONOS */
    { 0x07b2, 0x03ad }, /*         Greek_epsilonaccent έ GREEK SMALL LETTER EPSILON WITH TONOS */
    { 0x07b3, 0x03ae }, /*             Greek_etaaccent ή GREEK SMALL LETTER ETA WITH TONOS */
    { 0x07b4, 0x03af }, /*            Greek_iotaaccent ί GREEK SMALL LETTER IOTA WITH TONOS */
    { 0x07ba, 0x03b0 }, /* Greek_upsilonaccentdieresis ΰ GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS */
    { 0x07e1, 0x03b1 }, /*                 Greek_alpha α GREEK SMALL LETTER ALPHA */
    { 0x07e2, 0x03b2 }, /*                  Greek_beta β GREEK SMALL LETTER BETA */
    { 0x07e3, 0x03b3 }, /*                 Greek_gamma γ GREEK SMALL LETTER GAMMA */
    { 0x07e4, 0x03b4 }, /*                 Greek_delta δ GREEK SMALL LETTER DELTA */
    { 0x07e5, 0x03b5 }, /*               Greek_epsilon ε GREEK SMALL LETTER EPSILON */
    { 0x07e6, 0x03b6 }, /*                  Greek_zeta ζ GREEK SMALL LETTER ZETA */
    { 0x07e7, 0x03b7 }, /*                   Greek_eta η GREEK SMALL LETTER ETA */
    { 0x07e8, 0x03b8 }, /*                 Greek_theta θ GREEK SMALL LETTER THETA */
    { 0x07e9, 0x03b9 }, /*                  Greek_iota ι GREEK SMALL LETTER IOTA */
    { 0x07ea, 0x03ba }, /*                 Greek_kappa κ GREEK SMALL LETTER KAPPA */
    { 0x07eb, 0x03bb }, /*                Greek_lambda λ GREEK SMALL LETTER LAMDA */
    { 0x07ec, 0x03bc }, /*                    Greek_mu μ GREEK SMALL LETTER MU */
    { 0x07ed, 0x03bd }, /*                    Greek_nu ν GREEK SMALL LETTER NU */
    { 0x07ee, 0x03be }, /*                    Greek_xi ξ GREEK SMALL LETTER XI */
    { 0x07ef, 0x03bf }, /*               Greek_omicron ο GREEK SMALL LETTER OMICRON */
    { 0x07f0, 0x03c0 }, /*                    Greek_pi π GREEK SMALL LETTER PI */
    { 0x07f1, 0x03c1 }, /*                   Greek_rho ρ GREEK SMALL LETTER RHO */
    { 0x07f3, 0x03c2 }, /*       Greek_finalsmallsigma ς GREEK SMALL LETTER FINAL SIGMA */
    { 0x07f2, 0x03c3 }, /*                 Greek_sigma σ GREEK SMALL LETTER SIGMA */
    { 0x07f4, 0x03c4 }, /*                   Greek_tau τ GREEK SMALL LETTER TAU */
    { 0x07f5, 0x03c5 }, /*               Greek_upsilon υ GREEK SMALL LETTER UPSILON */
    { 0x07f6, 0x03c6 }, /*                   Greek_phi φ GREEK SMALL LETTER PHI */
    { 0x07f7, 0x03c7 }, /*                   Greek_chi χ GREEK SMALL LETTER CHI */
    { 0x07f8, 0x03c8 }, /*                   Greek_psi ψ GREEK SMALL LETTER PSI */
    { 0x07f9, 0x03c9 }, /*                 Greek_omega ω GREEK SMALL LETTER OMEGA */
    { 0x07b5, 0x03ca }, /*          Greek_iotadieresis ϊ GREEK SMALL LETTER IOTA WITH DIALYTIKA */
    { 0x07b9, 0x03cb }, /*       Greek_upsilondieresis ϋ GREEK SMALL LETTER UPSILON WITH DIALYTIKA */
    { 0x07b7, 0x03cc }, /*         Greek_omicronaccent ό GREEK SMALL LETTER OMICRON WITH TONOS */
    { 0x07b8, 0x03cd }, /*         Greek_upsilonaccent ύ GREEK SMALL LETTER UPSILON WITH TONOS */
    { 0x07bb, 0x03ce }, /*           Greek_omegaaccent ώ GREEK SMALL LETTER OMEGA WITH TONOS */
    { 0x06b3, 0x0401 }, /*                 Cyrillic_IO Ё CYRILLIC CAPITAL LETTER IO */
    { 0x06b1, 0x0402 }, /*                 Serbian_DJE Ђ CYRILLIC CAPITAL LETTER DJE */
    { 0x06b2, 0x0403 }, /*               Macedonia_GJE Ѓ CYRILLIC CAPITAL LETTER GJE */
    { 0x06b4, 0x0404 }, /*                Ukrainian_IE Є CYRILLIC CAPITAL LETTER UKRAINIAN IE */
    { 0x06b5, 0x0405 }, /*               Macedonia_DSE Ѕ CYRILLIC CAPITAL LETTER DZE */
    { 0x06b6, 0x0406 }, /*                 Ukrainian_I І CYRILLIC CAPITAL LETTER BYELORUSSIAN-UKRAINIAN I */
    { 0x06b7, 0x0407 }, /*                Ukrainian_YI Ї CYRILLIC CAPITAL LETTER YI */
    { 0x06b8, 0x0408 }, /*                 Cyrillic_JE Ј CYRILLIC CAPITAL LETTER JE */
    { 0x06b9, 0x0409 }, /*                Cyrillic_LJE Љ CYRILLIC CAPITAL LETTER LJE */
    { 0x06ba, 0x040a }, /*                Cyrillic_NJE Њ CYRILLIC CAPITAL LETTER NJE */
    { 0x06bb, 0x040b }, /*                Serbian_TSHE Ћ CYRILLIC CAPITAL LETTER TSHE */
    { 0x06bc, 0x040c }, /*               Macedonia_KJE Ќ CYRILLIC CAPITAL LETTER KJE */
    { 0x06be, 0x040e }, /*         Byelorussian_SHORTU Ў CYRILLIC CAPITAL LETTER SHORT U */
    { 0x06bf, 0x040f }, /*               Cyrillic_DZHE Џ CYRILLIC CAPITAL LETTER DZHE */
    { 0x06e1, 0x0410 }, /*                  Cyrillic_A А CYRILLIC CAPITAL LETTER A */
    { 0x06e2, 0x0411 }, /*                 Cyrillic_BE Б CYRILLIC CAPITAL LETTER BE */
    { 0x06f7, 0x0412 }, /*                 Cyrillic_VE В CYRILLIC CAPITAL LETTER VE */
    { 0x06e7, 0x0413 }, /*                Cyrillic_GHE Г CYRILLIC CAPITAL LETTER GHE */
    { 0x06e4, 0x0414 }, /*                 Cyrillic_DE Д CYRILLIC CAPITAL LETTER DE */
    { 0x06e5, 0x0415 }, /*                 Cyrillic_IE Е CYRILLIC CAPITAL LETTER IE */
    { 0x06f6, 0x0416 }, /*                Cyrillic_ZHE Ж CYRILLIC CAPITAL LETTER ZHE */
    { 0x06fa, 0x0417 }, /*                 Cyrillic_ZE З CYRILLIC CAPITAL LETTER ZE */
    { 0x06e9, 0x0418 }, /*                  Cyrillic_I И CYRILLIC CAPITAL LETTER I */
    { 0x06ea, 0x0419 }, /*             Cyrillic_SHORTI Й CYRILLIC CAPITAL LETTER SHORT I */
    { 0x06eb, 0x041a }, /*                 Cyrillic_KA К CYRILLIC CAPITAL LETTER KA */
    { 0x06ec, 0x041b }, /*                 Cyrillic_EL Л CYRILLIC CAPITAL LETTER EL */
    { 0x06ed, 0x041c }, /*                 Cyrillic_EM М CYRILLIC CAPITAL LETTER EM */
    { 0x06ee, 0x041d }, /*                 Cyrillic_EN Н CYRILLIC CAPITAL LETTER EN */
    { 0x06ef, 0x041e }, /*                  Cyrillic_O О CYRILLIC CAPITAL LETTER O */
    { 0x06f0, 0x041f }, /*                 Cyrillic_PE П CYRILLIC CAPITAL LETTER PE */
    { 0x06f2, 0x0420 }, /*                 Cyrillic_ER Р CYRILLIC CAPITAL LETTER ER */
    { 0x06f3, 0x0421 }, /*                 Cyrillic_ES С CYRILLIC CAPITAL LETTER ES */
    { 0x06f4, 0x0422 }, /*                 Cyrillic_TE Т CYRILLIC CAPITAL LETTER TE */
    { 0x06f5, 0x0423 }, /*                  Cyrillic_U У CYRILLIC CAPITAL LETTER U */
    { 0x06e6, 0x0424 }, /*                 Cyrillic_EF Ф CYRILLIC CAPITAL LETTER EF */
    { 0x06e8, 0x0425 }, /*                 Cyrillic_HA Х CYRILLIC CAPITAL LETTER HA */
    { 0x06e3, 0x0426 }, /*                Cyrillic_TSE Ц CYRILLIC CAPITAL LETTER TSE */
    { 0x06fe, 0x0427 }, /*                Cyrillic_CHE Ч CYRILLIC CAPITAL LETTER CHE */
    { 0x06fb, 0x0428 }, /*                Cyrillic_SHA Ш CYRILLIC CAPITAL LETTER SHA */
    { 0x06fd, 0x0429 }, /*              Cyrillic_SHCHA Щ CYRILLIC CAPITAL LETTER SHCHA */
    { 0x06ff, 0x042a }, /*           Cyrillic_HARDSIGN Ъ CYRILLIC CAPITAL LETTER HARD SIGN */
    { 0x06f9, 0x042b }, /*               Cyrillic_YERU Ы CYRILLIC CAPITAL LETTER YERU */
    { 0x06f8, 0x042c }, /*           Cyrillic_SOFTSIGN Ь CYRILLIC CAPITAL LETTER SOFT SIGN */
    { 0x06fc, 0x042d }, /*                  Cyrillic_E Э CYRILLIC CAPITAL LETTER E */
    { 0x06e0, 0x042e }, /*                 Cyrillic_YU Ю CYRILLIC CAPITAL LETTER YU */
    { 0x06f1, 0x042f }, /*                 Cyrillic_YA Я CYRILLIC CAPITAL LETTER YA */
    { 0x06c1, 0x0430 }, /*                  Cyrillic_a а CYRILLIC SMALL LETTER A */
    { 0x06c2, 0x0431 }, /*                 Cyrillic_be б CYRILLIC SMALL LETTER BE */
    { 0x06d7, 0x0432 }, /*                 Cyrillic_ve в CYRILLIC SMALL LETTER VE */
    { 0x06c7, 0x0433 }, /*                Cyrillic_ghe г CYRILLIC SMALL LETTER GHE */
    { 0x06c4, 0x0434 }, /*                 Cyrillic_de д CYRILLIC SMALL LETTER DE */
    { 0x06c5, 0x0435 }, /*                 Cyrillic_ie е CYRILLIC SMALL LETTER IE */
    { 0x06d6, 0x0436 }, /*                Cyrillic_zhe ж CYRILLIC SMALL LETTER ZHE */
    { 0x06da, 0x0437 }, /*                 Cyrillic_ze з CYRILLIC SMALL LETTER ZE */
    { 0x06c9, 0x0438 }, /*                  Cyrillic_i и CYRILLIC SMALL LETTER I */
    { 0x06ca, 0x0439 }, /*             Cyrillic_shorti й CYRILLIC SMALL LETTER SHORT I */
    { 0x06cb, 0x043a }, /*                 Cyrillic_ka к CYRILLIC SMALL LETTER KA */
    { 0x06cc, 0x043b }, /*                 Cyrillic_el л CYRILLIC SMALL LETTER EL */
    { 0x06cd, 0x043c }, /*                 Cyrillic_em м CYRILLIC SMALL LETTER EM */
    { 0x06ce, 0x043d }, /*                 Cyrillic_en н CYRILLIC SMALL LETTER EN */
    { 0x06cf, 0x043e }, /*                  Cyrillic_o о CYRILLIC SMALL LETTER O */
    { 0x06d0, 0x043f }, /*                 Cyrillic_pe п CYRILLIC SMALL LETTER PE */
    { 0x06d2, 0x0440 }, /*                 Cyrillic_er р CYRILLIC SMALL LETTER ER */
    { 0x06d3, 0x0441 }, /*                 Cyrillic_es с CYRILLIC SMALL LETTER ES */
    { 0x06d4, 0x0442 }, /*                 Cyrillic_te т CYRILLIC SMALL LETTER TE */
    { 0x06d5, 0x0443 }, /*                  Cyrillic_u у CYRILLIC SMALL LETTER U */
    { 0x06c6, 0x0444 }, /*                 Cyrillic_ef ф CYRILLIC SMALL LETTER EF */
    { 0x06c8, 0x0445 }, /*                 Cyrillic_ha х CYRILLIC SMALL LETTER HA */
    { 0x06c3, 0x0446 }, /*                Cyrillic_tse ц CYRILLIC SMALL LETTER TSE */
    { 0x06de, 0x0447 }, /*                Cyrillic_che ч CYRILLIC SMALL LETTER CHE */
    { 0x06db, 0x0448 }, /*                Cyrillic_sha ш CYRILLIC SMALL LETTER SHA */
    { 0x06dd, 0x0449 }, /*              Cyrillic_shcha щ CYRILLIC SMALL LETTER SHCHA */
    { 0x06df, 0x044a }, /*           Cyrillic_hardsign ъ CYRILLIC SMALL LETTER HARD SIGN */
    { 0x06d9, 0x044b }, /*               Cyrillic_yeru ы CYRILLIC SMALL LETTER YERU */
    { 0x06d8, 0x044c }, /*           Cyrillic_softsign ь CYRILLIC SMALL LETTER SOFT SIGN */
    { 0x06dc, 0x044d }, /*                  Cyrillic_e э CYRILLIC SMALL LETTER E */
    { 0x06c0, 0x044e }, /*                 Cyrillic_yu ю CYRILLIC SMALL LETTER YU */
    { 0x06d1, 0x044f }, /*                 Cyrillic_ya я CYRILLIC SMALL LETTER YA */
    { 0x06a3, 0x0451 }, /*                 Cyrillic_io ё CYRILLIC SMALL LETTER IO */
    { 0x06a1, 0x0452 }, /*                 Serbian_dje ђ CYRILLIC SMALL LETTER DJE */
    { 0x06a2, 0x0453 }, /*               Macedonia_gje ѓ CYRILLIC SMALL LETTER GJE */
    { 0x06a4, 0x0454 }, /*                Ukrainian_ie є CYRILLIC SMALL LETTER UKRAINIAN IE */
    { 0x06a5, 0x0455 }, /*               Macedonia_dse ѕ CYRILLIC SMALL LETTER DZE */
    { 0x06a6, 0x0456 }, /*                 Ukrainian_i і CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I */
    { 0x06a7, 0x0457 }, /*                Ukrainian_yi ї CYRILLIC SMALL LETTER YI */
    { 0x06a8, 0x0458 }, /*                 Cyrillic_je ј CYRILLIC SMALL LETTER JE */
    { 0x06a9, 0x0459 }, /*                Cyrillic_lje љ CYRILLIC SMALL LETTER LJE */
    { 0x06aa, 0x045a }, /*                Cyrillic_nje њ CYRILLIC SMALL LETTER NJE */
    { 0x06ab, 0x045b }, /*                Serbian_tshe ћ CYRILLIC SMALL LETTER TSHE */
    { 0x06ac, 0x045c }, /*               Macedonia_kje ќ CYRILLIC SMALL LETTER KJE */
    { 0x06ae, 0x045e }, /*         Byelorussian_shortu ў CYRILLIC SMALL LETTER SHORT U */
    { 0x06af, 0x045f }, /*               Cyrillic_dzhe џ CYRILLIC SMALL LETTER DZHE */
    { 0x06bd, 0x0490 }, /* . Ukrainian_GHE_WITH_UPTURN */
    { 0x06ad, 0x0491 }, /* . Ukrainian_ghe_with_upturn */
    { 0x0680, 0x0492 }, /* u Cyrillic_GHE_bar */
    { 0x0690, 0x0493 }, /* u Cyrillic_ghe_bar */
    { 0x0681, 0x0496 }, /* u Cyrillic_ZHE_descender */
    { 0x0691, 0x0497 }, /* u Cyrillic_zhe_descender */
    { 0x0682, 0x049a }, /* u Cyrillic_KA_descender */
    { 0x0692, 0x049b }, /* u Cyrillic_ka_descender */
    { 0x0683, 0x049c }, /* u Cyrillic_KA_vertstroke */
    { 0x0693, 0x049d }, /* u Cyrillic_ka_vertstroke */
    { 0x0684, 0x04a2 }, /* u Cyrillic_EN_descender */
    { 0x0694, 0x04a3 }, /* u Cyrillic_en_descender */
    { 0x0685, 0x04ae }, /* u Cyrillic_U_straight */
    { 0x0695, 0x04af }, /* u Cyrillic_u_straight */
    { 0x0686, 0x04b0 }, /* u Cyrillic_U_straight_bar */
    { 0x0696, 0x04b1 }, /* u Cyrillic_u_straight_bar */
    { 0x0687, 0x04b2 }, /* u Cyrillic_HA_descender */
    { 0x0697, 0x04b3 }, /* u Cyrillic_ha_descender */
    { 0x0688, 0x04b6 }, /* u Cyrillic_CHE_descender */
    { 0x0698, 0x04b7 }, /* u Cyrillic_che_descender */
    { 0x0689, 0x04b8 }, /* u Cyrillic_CHE_vertstroke */
    { 0x0699, 0x04b9 }, /* u Cyrillic_che_vertstroke */
    { 0x068a, 0x04ba }, /* u Cyrillic_SHHA */
    { 0x069a, 0x04bb }, /* u Cyrillic_shha */
    { 0x068c, 0x04d8 }, /* u Cyrillic_SCHWA */
    { 0x069c, 0x04d9 }, /* u Cyrillic_schwa */
    { 0x068d, 0x04e2 }, /* u Cyrillic_I_macron */
    { 0x069d, 0x04e3 }, /* u Cyrillic_i_macron */
    { 0x068e, 0x04e8 }, /* u Cyrillic_O_bar */
    { 0x069e, 0x04e9 }, /* u Cyrillic_o_bar */
    { 0x068f, 0x04ee }, /* u Cyrillic_U_macron */
    { 0x069f, 0x04ef }, /* u Cyrillic_u_macron */
    { 0x14b2, 0x0531 }, /* u Armenian_AYB */
    { 0x14b4, 0x0532 }, /* u Armenian_BEN */
    { 0x14b6, 0x0533 }, /* u Armenian_GIM */
    { 0x14b8, 0x0534 }, /* u Armenian_DA */
    { 0x14ba, 0x0535 }, /* u Armenian_YECH */
    { 0x14bc, 0x0536 }, /* u Armenian_ZA */
    { 0x14be, 0x0537 }, /* u Armenian_E */
    { 0x14c0, 0x0538 }, /* u Armenian_AT */
    { 0x14c2, 0x0539 }, /* u Armenian_TO */
    { 0x14c4, 0x053a }, /* u Armenian_ZHE */
    { 0x14c6, 0x053b }, /* u Armenian_INI */
    { 0x14c8, 0x053c }, /* u Armenian_LYUN */
    { 0x14ca, 0x053d }, /* u Armenian_KHE */
    { 0x14cc, 0x053e }, /* u Armenian_TSA */
    { 0x14ce, 0x053f }, /* u Armenian_KEN */
    { 0x14d0, 0x0540 }, /* u Armenian_HO */
    { 0x14d2, 0x0541 }, /* u Armenian_DZA */
    { 0x14d4, 0x0542 }, /* u Armenian_GHAT */
    { 0x14d6, 0x0543 }, /* u Armenian_TCHE */
    { 0x14d8, 0x0544 }, /* u Armenian_MEN */
    { 0x14da, 0x0545 }, /* u Armenian_HI */
    { 0x14dc, 0x0546 }, /* u Armenian_NU */
    { 0x14de, 0x0547 }, /* u Armenian_SHA */
    { 0x14e0, 0x0548 }, /* u Armenian_VO */
    { 0x14e2, 0x0549 }, /* u Armenian_CHA */
    { 0x14e4, 0x054a }, /* u Armenian_PE */
    { 0x14e6, 0x054b }, /* u Armenian_JE */
    { 0x14e8, 0x054c }, /* u Armenian_RA */
    { 0x14ea, 0x054d }, /* u Armenian_SE */
    { 0x14ec, 0x054e }, /* u Armenian_VEV */
    { 0x14ee, 0x054f }, /* u Armenian_TYUN */
    { 0x14f0, 0x0550 }, /* u Armenian_RE */
    { 0x14f2, 0x0551 }, /* u Armenian_TSO */
    { 0x14f4, 0x0552 }, /* u Armenian_VYUN */
    { 0x14f6, 0x0553 }, /* u Armenian_PYUR */
    { 0x14f8, 0x0554 }, /* u Armenian_KE */
    { 0x14fa, 0x0555 }, /* u Armenian_O */
    { 0x14fc, 0x0556 }, /* u Armenian_FE */
    { 0x14fe, 0x055a }, /* u Armenian_apostrophe */
    { 0x14b0, 0x055b }, /* u Armenian_shesht */
    { 0x14af, 0x055c }, /* u Armenian_amanak */
    { 0x14aa, 0x055d }, /* u Armenian_but */
    { 0x14b1, 0x055e }, /* u Armenian_paruyk */
    { 0x14b3, 0x0561 }, /* u Armenian_ayb */
    { 0x14b5, 0x0562 }, /* u Armenian_ben */
    { 0x14b7, 0x0563 }, /* u Armenian_gim */
    { 0x14b9, 0x0564 }, /* u Armenian_da */
    { 0x14bb, 0x0565 }, /* u Armenian_yech */
    { 0x14bd, 0x0566 }, /* u Armenian_za */
    { 0x14bf, 0x0567 }, /* u Armenian_e */
    { 0x14c1, 0x0568 }, /* u Armenian_at */
    { 0x14c3, 0x0569 }, /* u Armenian_to */
    { 0x14c5, 0x056a }, /* u Armenian_zhe */
    { 0x14c7, 0x056b }, /* u Armenian_ini */
    { 0x14c9, 0x056c }, /* u Armenian_lyun */
    { 0x14cb, 0x056d }, /* u Armenian_khe */
    { 0x14cd, 0x056e }, /* u Armenian_tsa */
    { 0x14cf, 0x056f }, /* u Armenian_ken */
    { 0x14d1, 0x0570 }, /* u Armenian_ho */
    { 0x14d3, 0x0571 }, /* u Armenian_dza */
    { 0x14d5, 0x0572 }, /* u Armenian_ghat */
    { 0x14d7, 0x0573 }, /* u Armenian_tche */
    { 0x14d9, 0x0574 }, /* u Armenian_men */
    { 0x14db, 0x0575 }, /* u Armenian_hi */
    { 0x14dd, 0x0576 }, /* u Armenian_nu */
    { 0x14df, 0x0577 }, /* u Armenian_sha */
    { 0x14e1, 0x0578 }, /* u Armenian_vo */
    { 0x14e3, 0x0579 }, /* u Armenian_cha */
    { 0x14e5, 0x057a }, /* u Armenian_pe */
    { 0x14e7, 0x057b }, /* u Armenian_je */
    { 0x14e9, 0x057c }, /* u Armenian_ra */
    { 0x14eb, 0x057d }, /* u Armenian_se */
    { 0x14ed, 0x057e }, /* u Armenian_vev */
    { 0x14ef, 0x057f }, /* u Armenian_tyun */
    { 0x14f1, 0x0580 }, /* u Armenian_re */
    { 0x14f3, 0x0581 }, /* u Armenian_tso */
    { 0x14f5, 0x0582 }, /* u Armenian_vyun */
    { 0x14f7, 0x0583 }, /* u Armenian_pyur */
    { 0x14f9, 0x0584 }, /* u Armenian_ke */
    { 0x14fb, 0x0585 }, /* u Armenian_o */
    { 0x14fd, 0x0586 }, /* u Armenian_fe */
    { 0x14a2, 0x0587 }, /* u Armenian_ligature_ew */
    { 0x14a3, 0x0589 }, /* u Armenian_verjaket */
    { 0x14ad, 0x058a }, /* u Armenian_yentamna */
    { 0x0ce0, 0x05d0 }, /*                hebrew_aleph א HEBREW LETTER ALEF */
    { 0x0ce1, 0x05d1 }, /*                  hebrew_bet ב HEBREW LETTER BET */
    { 0x0ce2, 0x05d2 }, /*                hebrew_gimel ג HEBREW LETTER GIMEL */
    { 0x0ce3, 0x05d3 }, /*                hebrew_dalet ד HEBREW LETTER DALET */
    { 0x0ce4, 0x05d4 }, /*                   hebrew_he ה HEBREW LETTER HE */
    { 0x0ce5, 0x05d5 }, /*                  hebrew_waw ו HEBREW LETTER VAV */
    { 0x0ce6, 0x05d6 }, /*                 hebrew_zain ז HEBREW LETTER ZAYIN */
    { 0x0ce7, 0x05d7 }, /*                 hebrew_chet ח HEBREW LETTER HET */
    { 0x0ce8, 0x05d8 }, /*                  hebrew_tet ט HEBREW LETTER TET */
    { 0x0ce9, 0x05d9 }, /*                  hebrew_yod י HEBREW LETTER YOD */
    { 0x0cea, 0x05da }, /*            hebrew_finalkaph ך HEBREW LETTER FINAL KAF */
    { 0x0ceb, 0x05db }, /*                 hebrew_kaph כ HEBREW LETTER KAF */
    { 0x0cec, 0x05dc }, /*                hebrew_lamed ל HEBREW LETTER LAMED */
    { 0x0ced, 0x05dd }, /*             hebrew_finalmem ם HEBREW LETTER FINAL MEM */
    { 0x0cee, 0x05de }, /*                  hebrew_mem מ HEBREW LETTER MEM */
    { 0x0cef, 0x05df }, /*             hebrew_finalnun ן HEBREW LETTER FINAL NUN */
    { 0x0cf0, 0x05e0 }, /*                  hebrew_nun נ HEBREW LETTER NUN */
    { 0x0cf1, 0x05e1 }, /*               hebrew_samech ס HEBREW LETTER SAMEKH */
    { 0x0cf2, 0x05e2 }, /*                 hebrew_ayin ע HEBREW LETTER AYIN */
    { 0x0cf3, 0x05e3 }, /*              hebrew_finalpe ף HEBREW LETTER FINAL PE */
    { 0x0cf4, 0x05e4 }, /*                   hebrew_pe פ HEBREW LETTER PE */
    { 0x0cf5, 0x05e5 }, /*            hebrew_finalzade ץ HEBREW LETTER FINAL TSADI */
    { 0x0cf6, 0x05e6 }, /*                 hebrew_zade צ HEBREW LETTER TSADI */
    { 0x0cf7, 0x05e7 }, /*                 hebrew_qoph ק HEBREW LETTER QOF */
    { 0x0cf8, 0x05e8 }, /*                 hebrew_resh ר HEBREW LETTER RESH */
    { 0x0cf9, 0x05e9 }, /*                 hebrew_shin ש HEBREW LETTER SHIN */
    { 0x0cfa, 0x05ea }, /*                  hebrew_taw ת HEBREW LETTER TAV */
    { 0x05ac, 0x060c }, /*                Arabic_comma ، ARABIC COMMA */
    { 0x05bb, 0x061b }, /*            Arabic_semicolon ؛ ARABIC SEMICOLON */
    { 0x05bf, 0x061f }, /*        Arabic_question_mark ؟ ARABIC QUESTION MARK */
    { 0x05c1, 0x0621 }, /*                Arabic_hamza ء ARABIC LETTER HAMZA */
    { 0x05c2, 0x0622 }, /*          Arabic_maddaonalef آ ARABIC LETTER ALEF WITH MADDA ABOVE */
    { 0x05c3, 0x0623 }, /*          Arabic_hamzaonalef أ ARABIC LETTER ALEF WITH HAMZA ABOVE */
    { 0x05c4, 0x0624 }, /*           Arabic_hamzaonwaw ؤ ARABIC LETTER WAW WITH HAMZA ABOVE */
    { 0x05c5, 0x0625 }, /*       Arabic_hamzaunderalef إ ARABIC LETTER ALEF WITH HAMZA BELOW */
    { 0x05c6, 0x0626 }, /*           Arabic_hamzaonyeh ئ ARABIC LETTER YEH WITH HAMZA ABOVE */
    { 0x05c7, 0x0627 }, /*                 Arabic_alef ا ARABIC LETTER ALEF */
    { 0x05c8, 0x0628 }, /*                  Arabic_beh ب ARABIC LETTER BEH */
    { 0x05c9, 0x0629 }, /*           Arabic_tehmarbuta ة ARABIC LETTER TEH MARBUTA */
    { 0x05ca, 0x062a }, /*                  Arabic_teh ت ARABIC LETTER TEH */
    { 0x05cb, 0x062b }, /*                 Arabic_theh ث ARABIC LETTER THEH */
    { 0x05cc, 0x062c }, /*                 Arabic_jeem ج ARABIC LETTER JEEM */
    { 0x05cd, 0x062d }, /*                  Arabic_hah ح ARABIC LETTER HAH */
    { 0x05ce, 0x062e }, /*                 Arabic_khah خ ARABIC LETTER KHAH */
    { 0x05cf, 0x062f }, /*                  Arabic_dal د ARABIC LETTER DAL */
    { 0x05d0, 0x0630 }, /*                 Arabic_thal ذ ARABIC LETTER THAL */
    { 0x05d1, 0x0631 }, /*                   Arabic_ra ر ARABIC LETTER REH */
    { 0x05d2, 0x0632 }, /*                 Arabic_zain ز ARABIC LETTER ZAIN */
    { 0x05d3, 0x0633 }, /*                 Arabic_seen س ARABIC LETTER SEEN */
    { 0x05d4, 0x0634 }, /*                Arabic_sheen ش ARABIC LETTER SHEEN */
    { 0x05d5, 0x0635 }, /*                  Arabic_sad ص ARABIC LETTER SAD */
    { 0x05d6, 0x0636 }, /*                  Arabic_dad ض ARABIC LETTER DAD */
    { 0x05d7, 0x0637 }, /*                  Arabic_tah ط ARABIC LETTER TAH */
    { 0x05d8, 0x0638 }, /*                  Arabic_zah ظ ARABIC LETTER ZAH */
    { 0x05d9, 0x0639 }, /*                  Arabic_ain ع ARABIC LETTER AIN */
    { 0x05da, 0x063a }, /*                Arabic_ghain غ ARABIC LETTER GHAIN */
    { 0x05e0, 0x0640 }, /*              Arabic_tatweel ـ ARABIC TATWEEL */
    { 0x05e1, 0x0641 }, /*                  Arabic_feh ف ARABIC LETTER FEH */
    { 0x05e2, 0x0642 }, /*                  Arabic_qaf ق ARABIC LETTER QAF */
    { 0x05e3, 0x0643 }, /*                  Arabic_kaf ك ARABIC LETTER KAF */
    { 0x05e4, 0x0644 }, /*                  Arabic_lam ل ARABIC LETTER LAM */
    { 0x05e5, 0x0645 }, /*                 Arabic_meem م ARABIC LETTER MEEM */
    { 0x05e6, 0x0646 }, /*                 Arabic_noon ن ARABIC LETTER NOON */
    { 0x05e7, 0x0647 }, /*                   Arabic_ha ه ARABIC LETTER HEH */
    { 0x05e8, 0x0648 }, /*                  Arabic_waw و ARABIC LETTER WAW */
    { 0x05e9, 0x0649 }, /*          Arabic_alefmaksura ى ARABIC LETTER ALEF MAKSURA */
    { 0x05ea, 0x064a }, /*                  Arabic_yeh ي ARABIC LETTER YEH */
    { 0x05eb, 0x064b }, /*             Arabic_fathatan ً ARABIC FATHATAN */
    { 0x05ec, 0x064c }, /*             Arabic_dammatan ٌ ARABIC DAMMATAN */
    { 0x05ed, 0x064d }, /*             Arabic_kasratan ٍ ARABIC KASRATAN */
    { 0x05ee, 0x064e }, /*                Arabic_fatha َ ARABIC FATHA */
    { 0x05ef, 0x064f }, /*                Arabic_damma ُ ARABIC DAMMA */
    { 0x05f0, 0x0650 }, /*                Arabic_kasra ِ ARABIC KASRA */
    { 0x05f1, 0x0651 }, /*               Arabic_shadda ّ ARABIC SHADDA */
    { 0x05f2, 0x0652 }, /*                Arabic_sukun ْ ARABIC SUKUN */
    { 0x05f3, 0x0653 }, /* u Arabic_madda_above */
    { 0x05f4, 0x0654 }, /* u Arabic_hamza_above */
    { 0x05f5, 0x0655 }, /* u Arabic_hamza_below */
    { 0x05b0, 0x0660 }, /* u Arabic_0 */
    { 0x05b1, 0x0661 }, /* u Arabic_1 */
    { 0x05b2, 0x0662 }, /* u Arabic_2 */
    { 0x05b3, 0x0663 }, /* u Arabic_3 */
    { 0x05b4, 0x0664 }, /* u Arabic_4 */
    { 0x05b5, 0x0665 }, /* u Arabic_5 */
    { 0x05b6, 0x0666 }, /* u Arabic_6 */
    { 0x05b7, 0x0667 }, /* u Arabic_7 */
    { 0x05b8, 0x0668 }, /* u Arabic_8 */
    { 0x05b9, 0x0669 }, /* u Arabic_9 */
    { 0x05a5, 0x066a }, /* u Arabic_percent */
    { 0x05a6, 0x0670 }, /* u Arabic_superscript_alef */
    { 0x05a7, 0x0679 }, /* u Arabic_tteh */
    { 0x05a8, 0x067e }, /* u Arabic_peh */
    { 0x05a9, 0x0686 }, /* u Arabic_tcheh */
    { 0x05aa, 0x0688 }, /* u Arabic_ddal */
    { 0x05ab, 0x0691 }, /* u Arabic_rreh */
    { 0x05f6, 0x0698 }, /* u Arabic_jeh */
    { 0x05f7, 0x06a4 }, /* u Arabic_veh */
    { 0x05f8, 0x06a9 }, /* u Arabic_keheh */
    { 0x05f9, 0x06af }, /* u Arabic_gaf */
    { 0x05fa, 0x06ba }, /* u Arabic_noon_ghunna */
    { 0x05fb, 0x06be }, /* u Arabic_heh_doachashmee */
    { 0x05fe, 0x06c1 }, /* u Arabic_heh_goal */
    { 0x05fc, 0x06cc }, /* u Farsi_yeh */
    { 0x05fd, 0x06d2 }, /* u Arabic_yeh_baree */
    { 0x05ae, 0x06d4 }, /* u Arabic_fullstop */
    { 0x0590, 0x06f0 }, /* u Farsi_0 */
    { 0x0591, 0x06f1 }, /* u Farsi_1 */
    { 0x0592, 0x06f2 }, /* u Farsi_2 */
    { 0x0593, 0x06f3 }, /* u Farsi_3 */
    { 0x0594, 0x06f4 }, /* u Farsi_4 */
    { 0x0595, 0x06f5 }, /* u Farsi_5 */
    { 0x0596, 0x06f6 }, /* u Farsi_6 */
    { 0x0597, 0x06f7 }, /* u Farsi_7 */
    { 0x0598, 0x06f8 }, /* u Farsi_8 */
    { 0x0599, 0x06f9 }, /* u Farsi_9 */
    { 0x0da1, 0x0e01 }, /*                  Thai_kokai ก THAI CHARACTER KO KAI */
    { 0x0da2, 0x0e02 }, /*                Thai_khokhai ข THAI CHARACTER KHO KHAI */
    { 0x0da3, 0x0e03 }, /*               Thai_khokhuat ฃ THAI CHARACTER KHO KHUAT */
    { 0x0da4, 0x0e04 }, /*               Thai_khokhwai ค THAI CHARACTER KHO KHWAI */
    { 0x0da5, 0x0e05 }, /*                Thai_khokhon ฅ THAI CHARACTER KHO KHON */
    { 0x0da6, 0x0e06 }, /*             Thai_khorakhang ฆ THAI CHARACTER KHO RAKHANG */
    { 0x0da7, 0x0e07 }, /*                 Thai_ngongu ง THAI CHARACTER NGO NGU */
    { 0x0da8, 0x0e08 }, /*                Thai_chochan จ THAI CHARACTER CHO CHAN */
    { 0x0da9, 0x0e09 }, /*               Thai_choching ฉ THAI CHARACTER CHO CHING */
    { 0x0daa, 0x0e0a }, /*               Thai_chochang ช THAI CHARACTER CHO CHANG */
    { 0x0dab, 0x0e0b }, /*                   Thai_soso ซ THAI CHARACTER SO SO */
    { 0x0dac, 0x0e0c }, /*                Thai_chochoe ฌ THAI CHARACTER CHO CHOE */
    { 0x0dad, 0x0e0d }, /*                 Thai_yoying ญ THAI CHARACTER YO YING */
    { 0x0dae, 0x0e0e }, /*                Thai_dochada ฎ THAI CHARACTER DO CHADA */
    { 0x0daf, 0x0e0f }, /*                Thai_topatak ฏ THAI CHARACTER TO PATAK */
    { 0x0db0, 0x0e10 }, /*                Thai_thothan ฐ THAI CHARACTER THO THAN */
    { 0x0db1, 0x0e11 }, /*          Thai_thonangmontho ฑ THAI CHARACTER THO NANGMONTHO */
    { 0x0db2, 0x0e12 }, /*             Thai_thophuthao ฒ THAI CHARACTER THO PHUTHAO */
    { 0x0db3, 0x0e13 }, /*                  Thai_nonen ณ THAI CHARACTER NO NEN */
    { 0x0db4, 0x0e14 }, /*                  Thai_dodek ด THAI CHARACTER DO DEK */
    { 0x0db5, 0x0e15 }, /*                  Thai_totao ต THAI CHARACTER TO TAO */
    { 0x0db6, 0x0e16 }, /*               Thai_thothung ถ THAI CHARACTER THO THUNG */
    { 0x0db7, 0x0e17 }, /*              Thai_thothahan ท THAI CHARACTER THO THAHAN */
    { 0x0db8, 0x0e18 }, /*               Thai_thothong ธ THAI CHARACTER THO THONG */
    { 0x0db9, 0x0e19 }, /*                   Thai_nonu น THAI CHARACTER NO NU */
    { 0x0dba, 0x0e1a }, /*               Thai_bobaimai บ THAI CHARACTER BO BAIMAI */
    { 0x0dbb, 0x0e1b }, /*                  Thai_popla ป THAI CHARACTER PO PLA */
    { 0x0dbc, 0x0e1c }, /*               Thai_phophung ผ THAI CHARACTER PHO PHUNG */
    { 0x0dbd, 0x0e1d }, /*                   Thai_fofa ฝ THAI CHARACTER FO FA */
    { 0x0dbe, 0x0e1e }, /*                Thai_phophan พ THAI CHARACTER PHO PHAN */
    { 0x0dbf, 0x0e1f }, /*                  Thai_fofan ฟ THAI CHARACTER FO FAN */
    { 0x0dc0, 0x0e20 }, /*             Thai_phosamphao ภ THAI CHARACTER PHO SAMPHAO */
    { 0x0dc1, 0x0e21 }, /*                   Thai_moma ม THAI CHARACTER MO MA */
    { 0x0dc2, 0x0e22 }, /*                  Thai_yoyak ย THAI CHARACTER YO YAK */
    { 0x0dc3, 0x0e23 }, /*                  Thai_rorua ร THAI CHARACTER RO RUA */
    { 0x0dc4, 0x0e24 }, /*                     Thai_ru ฤ THAI CHARACTER RU */
    { 0x0dc5, 0x0e25 }, /*                 Thai_loling ล THAI CHARACTER LO LING */
    { 0x0dc6, 0x0e26 }, /*                     Thai_lu ฦ THAI CHARACTER LU */
    { 0x0dc7, 0x0e27 }, /*                 Thai_wowaen ว THAI CHARACTER WO WAEN */
    { 0x0dc8, 0x0e28 }, /*                 Thai_sosala ศ THAI CHARACTER SO SALA */
    { 0x0dc9, 0x0e29 }, /*                 Thai_sorusi ษ THAI CHARACTER SO RUSI */
    { 0x0dca, 0x0e2a }, /*                  Thai_sosua ส THAI CHARACTER SO SUA */
    { 0x0dcb, 0x0e2b }, /*                  Thai_hohip ห THAI CHARACTER HO HIP */
    { 0x0dcc, 0x0e2c }, /*                Thai_lochula ฬ THAI CHARACTER LO CHULA */
    { 0x0dcd, 0x0e2d }, /*                   Thai_oang อ THAI CHARACTER O ANG */
    { 0x0dce, 0x0e2e }, /*               Thai_honokhuk ฮ THAI CHARACTER HO NOKHUK */
    { 0x0dcf, 0x0e2f }, /*              Thai_paiyannoi ฯ THAI CHARACTER PAIYANNOI */
    { 0x0dd0, 0x0e30 }, /*                  Thai_saraa ะ THAI CHARACTER SARA A */
    { 0x0dd1, 0x0e31 }, /*             Thai_maihanakat ั THAI CHARACTER MAI HAN-AKAT */
    { 0x0dd2, 0x0e32 }, /*                 Thai_saraaa า THAI CHARACTER SARA AA */
    { 0x0dd3, 0x0e33 }, /*                 Thai_saraam ำ THAI CHARACTER SARA AM */
    { 0x0dd4, 0x0e34 }, /*                  Thai_sarai ิ THAI CHARACTER SARA I */
    { 0x0dd5, 0x0e35 }, /*                 Thai_saraii ี THAI CHARACTER SARA II */
    { 0x0dd6, 0x0e36 }, /*                 Thai_saraue ึ THAI CHARACTER SARA UE */
    { 0x0dd7, 0x0e37 }, /*                Thai_sarauee ื THAI CHARACTER SARA UEE */
    { 0x0dd8, 0x0e38 }, /*                  Thai_sarau ุ THAI CHARACTER SARA U */
    { 0x0dd9, 0x0e39 }, /*                 Thai_sarauu ู THAI CHARACTER SARA UU */
    { 0x0dda, 0x0e3a }, /*                Thai_phinthu ฺ THAI CHARACTER PHINTHU */
    { 0x0ddf, 0x0e3f }, /*                   Thai_baht ฿ THAI CURRENCY SYMBOL BAHT */
    { 0x0de0, 0x0e40 }, /*                  Thai_sarae เ THAI CHARACTER SARA E */
    { 0x0de1, 0x0e41 }, /*                 Thai_saraae แ THAI CHARACTER SARA AE */
    { 0x0de2, 0x0e42 }, /*                  Thai_sarao โ THAI CHARACTER SARA O */
    { 0x0de3, 0x0e43 }, /*          Thai_saraaimaimuan ใ THAI CHARACTER SARA AI MAIMUAN */
    { 0x0de4, 0x0e44 }, /*         Thai_saraaimaimalai ไ THAI CHARACTER SARA AI MAIMALAI */
    { 0x0de5, 0x0e45 }, /*            Thai_lakkhangyao ๅ THAI CHARACTER LAKKHANGYAO */
    { 0x0de6, 0x0e46 }, /*               Thai_maiyamok ๆ THAI CHARACTER MAIYAMOK */
    { 0x0de7, 0x0e47 }, /*              Thai_maitaikhu ็ THAI CHARACTER MAITAIKHU */
    { 0x0de8, 0x0e48 }, /*                  Thai_maiek ่ THAI CHARACTER MAI EK */
    { 0x0de9, 0x0e49 }, /*                 Thai_maitho ้ THAI CHARACTER MAI THO */
    { 0x0dea, 0x0e4a }, /*                 Thai_maitri ๊ THAI CHARACTER MAI TRI */
    { 0x0deb, 0x0e4b }, /*            Thai_maichattawa ๋ THAI CHARACTER MAI CHATTAWA */
    { 0x0dec, 0x0e4c }, /*            Thai_thanthakhat ์ THAI CHARACTER THANTHAKHAT */
    { 0x0ded, 0x0e4d }, /*               Thai_nikhahit ํ THAI CHARACTER NIKHAHIT */
    { 0x0df0, 0x0e50 }, /*                 Thai_leksun ๐ THAI DIGIT ZERO */
    { 0x0df1, 0x0e51 }, /*                Thai_leknung ๑ THAI DIGIT ONE */
    { 0x0df2, 0x0e52 }, /*                Thai_leksong ๒ THAI DIGIT TWO */
    { 0x0df3, 0x0e53 }, /*                 Thai_leksam ๓ THAI DIGIT THREE */
    { 0x0df4, 0x0e54 }, /*                  Thai_leksi ๔ THAI DIGIT FOUR */
    { 0x0df5, 0x0e55 }, /*                  Thai_lekha ๕ THAI DIGIT FIVE */
    { 0x0df6, 0x0e56 }, /*                 Thai_lekhok ๖ THAI DIGIT SIX */
    { 0x0df7, 0x0e57 }, /*                Thai_lekchet ๗ THAI DIGIT SEVEN */
    { 0x0df8, 0x0e58 }, /*                Thai_lekpaet ๘ THAI DIGIT EIGHT */
    { 0x0df9, 0x0e59 }, /*                 Thai_lekkao ๙ THAI DIGIT NINE */
    { 0x15d0, 0x10d0 }, /* u Georgian_an */
    { 0x15d1, 0x10d1 }, /* u Georgian_ban */
    { 0x15d2, 0x10d2 }, /* u Georgian_gan */
    { 0x15d3, 0x10d3 }, /* u Georgian_don */
    { 0x15d4, 0x10d4 }, /* u Georgian_en */
    { 0x15d5, 0x10d5 }, /* u Georgian_vin */
    { 0x15d6, 0x10d6 }, /* u Georgian_zen */
    { 0x15d7, 0x10d7 }, /* u Georgian_tan */
    { 0x15d8, 0x10d8 }, /* u Georgian_in */
    { 0x15d9, 0x10d9 }, /* u Georgian_kan */
    { 0x15da, 0x10da }, /* u Georgian_las */
    { 0x15db, 0x10db }, /* u Georgian_man */
    { 0x15dc, 0x10dc }, /* u Georgian_nar */
    { 0x15dd, 0x10dd }, /* u Georgian_on */
    { 0x15de, 0x10de }, /* u Georgian_par */
    { 0x15df, 0x10df }, /* u Georgian_zhar */
    { 0x15e0, 0x10e0 }, /* u Georgian_rae */
    { 0x15e1, 0x10e1 }, /* u Georgian_san */
    { 0x15e2, 0x10e2 }, /* u Georgian_tar */
    { 0x15e3, 0x10e3 }, /* u Georgian_un */
    { 0x15e4, 0x10e4 }, /* u Georgian_phar */
    { 0x15e5, 0x10e5 }, /* u Georgian_khar */
    { 0x15e6, 0x10e6 }, /* u Georgian_ghan */
    { 0x15e7, 0x10e7 }, /* u Georgian_qar */
    { 0x15e8, 0x10e8 }, /* u Georgian_shin */
    { 0x15e9, 0x10e9 }, /* u Georgian_chin */
    { 0x15ea, 0x10ea }, /* u Georgian_can */
    { 0x15eb, 0x10eb }, /* u Georgian_jil */
    { 0x15ec, 0x10ec }, /* u Georgian_cil */
    { 0x15ed, 0x10ed }, /* u Georgian_char */
    { 0x15ee, 0x10ee }, /* u Georgian_xan */
    { 0x15ef, 0x10ef }, /* u Georgian_jhan */
    { 0x15f0, 0x10f0 }, /* u Georgian_hae */
    { 0x15f1, 0x10f1 }, /* u Georgian_he */
    { 0x15f2, 0x10f2 }, /* u Georgian_hie */
    { 0x15f3, 0x10f3 }, /* u Georgian_we */
    { 0x15f4, 0x10f4 }, /* u Georgian_har */
    { 0x15f5, 0x10f5 }, /* u Georgian_hoe */
    { 0x15f6, 0x10f6 }, /* u Georgian_fi */
    { 0x0ed4, 0x11a8 }, /*             Hangul_J_Kiyeog ᆨ HANGUL JONGSEONG KIYEOK */
    { 0x0ed5, 0x11a9 }, /*        Hangul_J_SsangKiyeog ᆩ HANGUL JONGSEONG SSANGKIYEOK */
    { 0x0ed6, 0x11aa }, /*         Hangul_J_KiyeogSios ᆪ HANGUL JONGSEONG KIYEOK-SIOS */
    { 0x0ed7, 0x11ab }, /*              Hangul_J_Nieun ᆫ HANGUL JONGSEONG NIEUN */
    { 0x0ed8, 0x11ac }, /*         Hangul_J_NieunJieuj ᆬ HANGUL JONGSEONG NIEUN-CIEUC */
    { 0x0ed9, 0x11ad }, /*         Hangul_J_NieunHieuh ᆭ HANGUL JONGSEONG NIEUN-HIEUH */
    { 0x0eda, 0x11ae }, /*             Hangul_J_Dikeud ᆮ HANGUL JONGSEONG TIKEUT */
    { 0x0edb, 0x11af }, /*              Hangul_J_Rieul ᆯ HANGUL JONGSEONG RIEUL */
    { 0x0edc, 0x11b0 }, /*        Hangul_J_RieulKiyeog ᆰ HANGUL JONGSEONG RIEUL-KIYEOK */
    { 0x0edd, 0x11b1 }, /*         Hangul_J_RieulMieum ᆱ HANGUL JONGSEONG RIEUL-MIEUM */
    { 0x0ede, 0x11b2 }, /*         Hangul_J_RieulPieub ᆲ HANGUL JONGSEONG RIEUL-PIEUP */
    { 0x0edf, 0x11b3 }, /*          Hangul_J_RieulSios ᆳ HANGUL JONGSEONG RIEUL-SIOS */
    { 0x0ee0, 0x11b4 }, /*         Hangul_J_RieulTieut ᆴ HANGUL JONGSEONG RIEUL-THIEUTH */
    { 0x0ee1, 0x11b5 }, /*        Hangul_J_RieulPhieuf ᆵ HANGUL JONGSEONG RIEUL-PHIEUPH */
    { 0x0ee2, 0x11b6 }, /*         Hangul_J_RieulHieuh ᆶ HANGUL JONGSEONG RIEUL-HIEUH */
    { 0x0ee3, 0x11b7 }, /*              Hangul_J_Mieum ᆷ HANGUL JONGSEONG MIEUM */
    { 0x0ee4, 0x11b8 }, /*              Hangul_J_Pieub ᆸ HANGUL JONGSEONG PIEUP */
    { 0x0ee5, 0x11b9 }, /*          Hangul_J_PieubSios ᆹ HANGUL JONGSEONG PIEUP-SIOS */
    { 0x0ee6, 0x11ba }, /*               Hangul_J_Sios ᆺ HANGUL JONGSEONG SIOS */
    { 0x0ee7, 0x11bb }, /*          Hangul_J_SsangSios ᆻ HANGUL JONGSEONG SSANGSIOS */
    { 0x0ee8, 0x11bc }, /*              Hangul_J_Ieung ᆼ HANGUL JONGSEONG IEUNG */
    { 0x0ee9, 0x11bd }, /*              Hangul_J_Jieuj ᆽ HANGUL JONGSEONG CIEUC */
    { 0x0eea, 0x11be }, /*              Hangul_J_Cieuc ᆾ HANGUL JONGSEONG CHIEUCH */
    { 0x0eeb, 0x11bf }, /*             Hangul_J_Khieuq ᆿ HANGUL JONGSEONG KHIEUKH */
    { 0x0eec, 0x11c0 }, /*              Hangul_J_Tieut ᇀ HANGUL JONGSEONG THIEUTH */
    { 0x0eed, 0x11c1 }, /*             Hangul_J_Phieuf ᇁ HANGUL JONGSEONG PHIEUPH */
    { 0x0eee, 0x11c2 }, /*              Hangul_J_Hieuh ᇂ HANGUL JONGSEONG HIEUH */
    { 0x0ef8, 0x11eb }, /*            Hangul_J_PanSios ᇫ HANGUL JONGSEONG PANSIOS */
    { 0x0ef9, 0x11f0 }, /*  Hangul_J_KkogjiDalrinIeung ᇰ HANGUL JONGSEONG YESIEUNG */
    { 0x0efa, 0x11f9 }, /*        Hangul_J_YeorinHieuh ᇹ HANGUL JONGSEONG YEORINHIEUH */
    { 0x12a1, 0x1e02 }, /* u Babovedot */
    { 0x12a2, 0x1e03 }, /* u babovedot */
    { 0x12a6, 0x1e0a }, /* u Dabovedot */
    { 0x12ab, 0x1e0b }, /* u dabovedot */
    { 0x12b0, 0x1e1e }, /* u Fabovedot */
    { 0x12b1, 0x1e1f }, /* u fabovedot */
    { 0x16d1, 0x1e36 }, /* u Lbelowdot */
    { 0x16e1, 0x1e37 }, /* u lbelowdot */
    { 0x12b4, 0x1e40 }, /* u Mabovedot */
    { 0x12b5, 0x1e41 }, /* u mabovedot */
    { 0x12b7, 0x1e56 }, /* u Pabovedot */
    { 0x12b9, 0x1e57 }, /* u pabovedot */
    { 0x12bb, 0x1e60 }, /* u Sabovedot */
    { 0x12bf, 0x1e61 }, /* u sabovedot */
    { 0x12d7, 0x1e6a }, /* u Tabovedot */
    { 0x12f7, 0x1e6b }, /* u tabovedot */
    { 0x12a8, 0x1e80 }, /* u Wgrave */
    { 0x12b8, 0x1e81 }, /* u wgrave */
    { 0x12aa, 0x1e82 }, /* u Wacute */
    { 0x12ba, 0x1e83 }, /* u wacute */
    { 0x12bd, 0x1e84 }, /* u Wdiaeresis */
    { 0x12be, 0x1e85 }, /* u wdiaeresis */
    { 0x16a3, 0x1e8a }, /* u Xabovedot */
    { 0x16b3, 0x1e8b }, /* u xabovedot */
    { 0x1ea0, 0x1ea0 }, /* u Abelowdot */
    { 0x1ea1, 0x1ea1 }, /* u abelowdot */
    { 0x1ea2, 0x1ea2 }, /* u Ahook */
    { 0x1ea3, 0x1ea3 }, /* u ahook */
    { 0x1ea4, 0x1ea4 }, /* u Acircumflexacute */
    { 0x1ea5, 0x1ea5 }, /* u acircumflexacute */
    { 0x1ea6, 0x1ea6 }, /* u Acircumflexgrave */
    { 0x1ea7, 0x1ea7 }, /* u acircumflexgrave */
    { 0x1ea8, 0x1ea8 }, /* u Acircumflexhook */
    { 0x1ea9, 0x1ea9 }, /* u acircumflexhook */
    { 0x1eaa, 0x1eaa }, /* u Acircumflextilde */
    { 0x1eab, 0x1eab }, /* u acircumflextilde */
    { 0x1eac, 0x1eac }, /* u Acircumflexbelowdot */
    { 0x1ead, 0x1ead }, /* u acircumflexbelowdot */
    { 0x1eae, 0x1eae }, /* u Abreveacute */
    { 0x1eaf, 0x1eaf }, /* u abreveacute */
    { 0x1eb0, 0x1eb0 }, /* u Abrevegrave */
    { 0x1eb1, 0x1eb1 }, /* u abrevegrave */
    { 0x1eb2, 0x1eb2 }, /* u Abrevehook */
    { 0x1eb3, 0x1eb3 }, /* u abrevehook */
    { 0x1eb4, 0x1eb4 }, /* u Abrevetilde */
    { 0x1eb5, 0x1eb5 }, /* u abrevetilde */
    { 0x1eb6, 0x1eb6 }, /* u Abrevebelowdot */
    { 0x1eb7, 0x1eb7 }, /* u abrevebelowdot */
    { 0x1eb8, 0x1eb8 }, /* u Ebelowdot */
    { 0x1eb9, 0x1eb9 }, /* u ebelowdot */
    { 0x1eba, 0x1eba }, /* u Ehook */
    { 0x1ebb, 0x1ebb }, /* u ehook */
    { 0x1ebc, 0x1ebc }, /* u Etilde */
    { 0x1ebd, 0x1ebd }, /* u etilde */
    { 0x1ebe, 0x1ebe }, /* u Ecircumflexacute */
    { 0x1ebf, 0x1ebf }, /* u ecircumflexacute */
    { 0x1ec0, 0x1ec0 }, /* u Ecircumflexgrave */
    { 0x1ec1, 0x1ec1 }, /* u ecircumflexgrave */
    { 0x1ec2, 0x1ec2 }, /* u Ecircumflexhook */
    { 0x1ec3, 0x1ec3 }, /* u ecircumflexhook */
    { 0x1ec4, 0x1ec4 }, /* u Ecircumflextilde */
    { 0x1ec5, 0x1ec5 }, /* u ecircumflextilde */
    { 0x1ec6, 0x1ec6 }, /* u Ecircumflexbelowdot */
    { 0x1ec7, 0x1ec7 }, /* u ecircumflexbelowdot */
    { 0x1ec8, 0x1ec8 }, /* u Ihook */
    { 0x1ec9, 0x1ec9 }, /* u ihook */
    { 0x1eca, 0x1eca }, /* u Ibelowdot */
    { 0x1ecb, 0x1ecb }, /* u ibelowdot */
    { 0x1ecc, 0x1ecc }, /* u Obelowdot */
    { 0x1ecd, 0x1ecd }, /* u obelowdot */
    { 0x1ece, 0x1ece }, /* u Ohook */
    { 0x1ecf, 0x1ecf }, /* u ohook */
    { 0x1ed0, 0x1ed0 }, /* u Ocircumflexacute */
    { 0x1ed1, 0x1ed1 }, /* u ocircumflexacute */
    { 0x1ed2, 0x1ed2 }, /* u Ocircumflexgrave */
    { 0x1ed3, 0x1ed3 }, /* u ocircumflexgrave */
    { 0x1ed4, 0x1ed4 }, /* u Ocircumflexhook */
    { 0x1ed5, 0x1ed5 }, /* u ocircumflexhook */
    { 0x1ed6, 0x1ed6 }, /* u Ocircumflextilde */
    { 0x1ed7, 0x1ed7 }, /* u ocircumflextilde */
    { 0x1ed8, 0x1ed8 }, /* u Ocircumflexbelowdot */
    { 0x1ed9, 0x1ed9 }, /* u ocircumflexbelowdot */
    { 0x1eda, 0x1eda }, /* u Ohornacute */
    { 0x1edb, 0x1edb }, /* u ohornacute */
    { 0x1edc, 0x1edc }, /* u Ohorngrave */
    { 0x1edd, 0x1edd }, /* u ohorngrave */
    { 0x1ede, 0x1ede }, /* u Ohornhook */
    { 0x1edf, 0x1edf }, /* u ohornhook */
    { 0x1ee0, 0x1ee0 }, /* u Ohorntilde */
    { 0x1ee1, 0x1ee1 }, /* u ohorntilde */
    { 0x1ee2, 0x1ee2 }, /* u Ohornbelowdot */
    { 0x1ee3, 0x1ee3 }, /* u ohornbelowdot */
    { 0x1ee4, 0x1ee4 }, /* u Ubelowdot */
    { 0x1ee5, 0x1ee5 }, /* u ubelowdot */
    { 0x1ee6, 0x1ee6 }, /* u Uhook */
    { 0x1ee7, 0x1ee7 }, /* u uhook */
    { 0x1ee8, 0x1ee8 }, /* u Uhornacute */
    { 0x1ee9, 0x1ee9 }, /* u uhornacute */
    { 0x1eea, 0x1eea }, /* u Uhorngrave */
    { 0x1eeb, 0x1eeb }, /* u uhorngrave */
    { 0x1eec, 0x1eec }, /* u Uhornhook */
    { 0x1eed, 0x1eed }, /* u uhornhook */
    { 0x1eee, 0x1eee }, /* u Uhorntilde */
    { 0x1eef, 0x1eef }, /* u uhorntilde */
    { 0x1ef0, 0x1ef0 }, /* u Uhornbelowdot */
    { 0x1ef1, 0x1ef1 }, /* u uhornbelowdot */
    { 0x12ac, 0x1ef2 }, /* u Ygrave */
    { 0x12bc, 0x1ef3 }, /* u ygrave */
    { 0x1ef4, 0x1ef4 }, /* u Ybelowdot */
    { 0x1ef5, 0x1ef5 }, /* u ybelowdot */
    { 0x1ef6, 0x1ef6 }, /* u Yhook */
    { 0x1ef7, 0x1ef7 }, /* u yhook */
    { 0x1ef8, 0x1ef8 }, /* u Ytilde */
    { 0x1ef9, 0x1ef9 }, /* u ytilde */
    { 0x0aa2, 0x2002 }, /*                     enspace   EN SPACE */
    { 0x0aa1, 0x2003 }, /*                     emspace   EM SPACE */
    { 0x0aa3, 0x2004 }, /*                    em3space   THREE-PER-EM SPACE */
    { 0x0aa4, 0x2005 }, /*                    em4space   FOUR-PER-EM SPACE */
    { 0x0aa5, 0x2007 }, /*                  digitspace   FIGURE SPACE */
    { 0x0aa6, 0x2008 }, /*                  punctspace   PUNCTUATION SPACE */
    { 0x0aa7, 0x2009 }, /*                   thinspace   THIN SPACE */
    { 0x0aa8, 0x200a }, /*                   hairspace   HAIR SPACE */
    { 0x0abb, 0x2012 }, /*                     figdash ‒ FIGURE DASH */
    { 0x0aaa, 0x2013 }, /*                      endash – EN DASH */
    { 0x0aa9, 0x2014 }, /*                      emdash — EM DASH */
    { 0x07af, 0x2015 }, /*              Greek_horizbar ― HORIZONTAL BAR */
    { 0x0cdf, 0x2017 }, /*        hebrew_doublelowline ‗ DOUBLE LOW LINE */
    { 0x0ad0, 0x2018 }, /*         leftsinglequotemark ‘ LEFT SINGLE QUOTATION MARK */
    { 0x0ad1, 0x2019 }, /*        rightsinglequotemark ’ RIGHT SINGLE QUOTATION MARK */
    { 0x0afd, 0x201a }, /*          singlelowquotemark ‚ SINGLE LOW-9 QUOTATION MARK */
    { 0x0ad2, 0x201c }, /*         leftdoublequotemark “ LEFT DOUBLE QUOTATION MARK */
    { 0x0ad3, 0x201d }, /*        rightdoublequotemark ” RIGHT DOUBLE QUOTATION MARK */
    { 0x0afe, 0x201e }, /*          doublelowquotemark „ DOUBLE LOW-9 QUOTATION MARK */
    { 0x0af1, 0x2020 }, /*                      dagger † DAGGER */
    { 0x0af2, 0x2021 }, /*                doubledagger ‡ DOUBLE DAGGER */
    { 0x0ae6, 0x2022 }, /*          enfilledcircbullet • BULLET */
    { 0x0aaf, 0x2025 }, /*             doubbaselinedot ‥ TWO DOT LEADER */
    { 0x0aae, 0x2026 }, /*                    ellipsis … HORIZONTAL ELLIPSIS */
    { 0x0ad6, 0x2032 }, /*                     minutes ′ PRIME */
    { 0x0ad7, 0x2033 }, /*                     seconds ″ DOUBLE PRIME */
    { 0x0afc, 0x2038 }, /*                       caret ‸ CARET */
    { 0x047e, 0x203e }, /*                    overline ‾ OVERLINE */
    { 0x20a0, 0x20a0 }, /* u EcuSign */
    { 0x20a1, 0x20a1 }, /* u ColonSign */
    { 0x20a2, 0x20a2 }, /* u CruzeiroSign */
    { 0x20a3, 0x20a3 }, /* u FFrancSign */
    { 0x20a4, 0x20a4 }, /* u LiraSign */
    { 0x20a5, 0x20a5 }, /* u MillSign */
    { 0x20a6, 0x20a6 }, /* u NairaSign */
    { 0x20a7, 0x20a7 }, /* u PesetaSign */
    { 0x20a8, 0x20a8 }, /* u RupeeSign */
    { 0x20a9, 0x20a9 }, /* u WonSign */
    { 0x20aa, 0x20aa }, /* u NewSheqelSign */
    { 0x20ab, 0x20ab }, /* u DongSign */ // 0x13a4
    /* TODO: Test { 0x13a4, 0x20ac }, //. Euro */
    /* TODO: Test { 0x20ac, 0x20ac }, //. EuroSign */
    { 0x20ac, 0x20ac }, /*                        Euro € EURO SIGN */
    { 0x0ab8, 0x2105 }, /*                      careof ℅ CARE OF */
    { 0x06b0, 0x2116 }, /*                  numerosign № NUMERO SIGN */
    { 0x0afb, 0x2117 }, /*         phonographcopyright ℗ SOUND RECORDING COPYRIGHT */
    { 0x0ad4, 0x211e }, /*                prescription ℞ PRESCRIPTION TAKE */
    { 0x0ac9, 0x2122 }, /*                   trademark ™ TRADE MARK SIGN */
    { 0x0ab0, 0x2153 }, /*                    onethird ⅓ VULGAR FRACTION ONE THIRD */
    { 0x0ab1, 0x2154 }, /*                   twothirds ⅔ VULGAR FRACTION TWO THIRDS */
    { 0x0ab2, 0x2155 }, /*                    onefifth ⅕ VULGAR FRACTION ONE FIFTH */
    { 0x0ab3, 0x2156 }, /*                   twofifths ⅖ VULGAR FRACTION TWO FIFTHS */
    { 0x0ab4, 0x2157 }, /*                 threefifths ⅗ VULGAR FRACTION THREE FIFTHS */
    { 0x0ab5, 0x2158 }, /*                  fourfifths ⅘ VULGAR FRACTION FOUR FIFTHS */
    { 0x0ab6, 0x2159 }, /*                    onesixth ⅙ VULGAR FRACTION ONE SIXTH */
    { 0x0ab7, 0x215a }, /*                  fivesixths ⅚ VULGAR FRACTION FIVE SIXTHS */
    { 0x0ac3, 0x215b }, /*                   oneeighth ⅛ VULGAR FRACTION ONE EIGHTH */
    { 0x0ac4, 0x215c }, /*                threeeighths ⅜ VULGAR FRACTION THREE EIGHTHS */
    { 0x0ac5, 0x215d }, /*                 fiveeighths ⅝ VULGAR FRACTION FIVE EIGHTHS */
    { 0x0ac6, 0x215e }, /*                seveneighths ⅞ VULGAR FRACTION SEVEN EIGHTHS */
    { 0x08fb, 0x2190 }, /*                   leftarrow ← LEFTWARDS ARROW */
    { 0x08fc, 0x2191 }, /*                     uparrow ↑ UPWARDS ARROW */
    { 0x08fd, 0x2192 }, /*                  rightarrow → RIGHTWARDS ARROW */
    { 0x08fe, 0x2193 }, /*                   downarrow ↓ DOWNWARDS ARROW */
    { 0x08ce, 0x21d2 }, /*                     implies ⇒ RIGHTWARDS DOUBLE ARROW */
    { 0x08cd, 0x21d4 }, /*                    ifonlyif ⇔ LEFT RIGHT DOUBLE ARROW */
    { 0x08ef, 0x2202 }, /*           partialderivative ∂ PARTIAL DIFFERENTIAL */
    { 0x08c5, 0x2207 }, /*                       nabla ∇ NABLA */
    { 0x0bca, 0x2218 }, /*                         jot ∘ RING OPERATOR */
    { 0x08d6, 0x221a }, /*                     radical √ SQUARE ROOT */
    { 0x08c1, 0x221d }, /*                   variation ∝ PROPORTIONAL TO */
    { 0x08c2, 0x221e }, /*                    infinity ∞ INFINITY */
    { 0x08de, 0x2227 }, /*                  logicaland ∧ LOGICAL AND */
    { 0x08df, 0x2228 }, /*                   logicalor ∨ LOGICAL OR */
    { 0x08dc, 0x2229 }, /*                intersection ∩ INTERSECTION */
    { 0x08dd, 0x222a }, /*                       union ∪ UNION */
    { 0x08bf, 0x222b }, /*                    integral ∫ INTEGRAL */
    { 0x08c0, 0x2234 }, /*                   therefore ∴ THEREFORE */
    { 0x08c8, 0x223c }, /*                 approximate ∼ TILDE OPERATOR */
    { 0x08c9, 0x2243 }, /*                similarequal ≃ ASYMPTOTICALLY EQUAL TO */
    { 0x08bd, 0x2260 }, /*                    notequal ≠ NOT EQUAL TO */
    { 0x08cf, 0x2261 }, /*                   identical ≡ IDENTICAL TO */
    { 0x08bc, 0x2264 }, /*               lessthanequal ≤ LESS-THAN OR EQUAL TO */
    { 0x08be, 0x2265 }, /*            greaterthanequal ≥ GREATER-THAN OR EQUAL TO */
    { 0x08da, 0x2282 }, /*                  includedin ⊂ SUBSET OF */
    { 0x08db, 0x2283 }, /*                    includes ⊃ SUPERSET OF */
    { 0x0bdc, 0x22a2 }, /*                    lefttack ⊢ RIGHT TACK */
    { 0x0bfc, 0x22a3 }, /*                   righttack ⊣ LEFT TACK */
    { 0x0bce, 0x22a4 }, /*                      uptack ⊤ DOWN TACK */
    { 0x0bc2, 0x22a5 }, /*                    downtack ⊥ UP TACK */
    { 0x0bd3, 0x2308 }, /*                     upstile ⌈ LEFT CEILING */
    { 0x0bc4, 0x230a }, /*                   downstile ⌊ LEFT FLOOR */
    { 0x0afa, 0x2315 }, /*           telephonerecorder ⌕ TELEPHONE RECORDER */
    { 0x08a4, 0x2320 }, /*                 topintegral ⌠ TOP HALF INTEGRAL */
    { 0x08a5, 0x2321 }, /*                 botintegral ⌡ BOTTOM HALF INTEGRAL */
    { 0x0abc, 0x2329 }, /*            leftanglebracket 〈 LEFT-POINTING ANGLE BRACKET */
    { 0x0abe, 0x232a }, /*           rightanglebracket 〉 RIGHT-POINTING ANGLE BRACKET */
    { 0x0bcc, 0x2395 }, /*                        quad ⎕ APL FUNCTIONAL SYMBOL QUAD */
    { 0x08ab, 0x239b }, /*               topleftparens ⎛ ??? */
    { 0x08ac, 0x239d }, /*               botleftparens ⎝ ??? */
    { 0x08ad, 0x239e }, /*              toprightparens ⎞ ??? */
    { 0x08ae, 0x23a0 }, /*              botrightparens ⎠ ??? */
    { 0x08a7, 0x23a1 }, /*            topleftsqbracket ⎡ ??? */
    { 0x08a8, 0x23a3 }, /*            botleftsqbracket ⎣ ??? */
    { 0x08a9, 0x23a4 }, /*           toprightsqbracket ⎤ ??? */
    { 0x08aa, 0x23a6 }, /*           botrightsqbracket ⎦ ??? */
    { 0x08af, 0x23a8 }, /*        leftmiddlecurlybrace ⎨ ??? */
    { 0x08b0, 0x23ac }, /*       rightmiddlecurlybrace ⎬ ??? */
    { 0x08a1, 0x23b7 }, /*                 leftradical ⎷ ??? */
    { 0x09ef, 0x23ba }, /*              horizlinescan1 ⎺ HORIZONTAL SCAN LINE-1 (Unicode 3.2 draft) */
    { 0x09f0, 0x23bb }, /*              horizlinescan3 ⎻ HORIZONTAL SCAN LINE-3 (Unicode 3.2 draft) */
    { 0x09f2, 0x23bc }, /*              horizlinescan7 ⎼ HORIZONTAL SCAN LINE-7 (Unicode 3.2 draft) */
    { 0x09f3, 0x23bd }, /*              horizlinescan9 ⎽ HORIZONTAL SCAN LINE-9 (Unicode 3.2 draft) */
    { 0x09e2, 0x2409 }, /*                          ht ␉ SYMBOL FOR HORIZONTAL TABULATION */
    { 0x09e5, 0x240a }, /*                          lf ␊ SYMBOL FOR LINE FEED */
    { 0x09e9, 0x240b }, /*                          vt ␋ SYMBOL FOR VERTICAL TABULATION */
    { 0x09e3, 0x240c }, /*                          ff ␌ SYMBOL FOR FORM FEED */
    { 0x09e4, 0x240d }, /*                          cr ␍ SYMBOL FOR CARRIAGE RETURN */
    { 0x0aac, 0x2423 }, /* o signifblank */
    { 0x09e8, 0x2424 }, /*                          nl ␤ SYMBOL FOR NEWLINE */
    { 0x08a3, 0x2500 }, /*              horizconnector ─ BOX DRAWINGS LIGHT HORIZONTAL */
    { 0x08a6, 0x2502 }, /*               vertconnector │ BOX DRAWINGS LIGHT VERTICAL */
    { 0x09ec, 0x250c }, /*                upleftcorner ┌ BOX DRAWINGS LIGHT DOWN AND RIGHT */
    { 0x08a2, 0x250c }, /*              topleftradical ┌ BOX DRAWINGS LIGHT DOWN AND RIGHT */
    { 0x09eb, 0x2510 }, /*               uprightcorner ┐ BOX DRAWINGS LIGHT DOWN AND LEFT */
    { 0x09ed, 0x2514 }, /*               lowleftcorner └ BOX DRAWINGS LIGHT UP AND RIGHT */
    { 0x09ea, 0x2518 }, /*              lowrightcorner ┘ BOX DRAWINGS LIGHT UP AND LEFT */
    { 0x09f4, 0x251c }, /*                       leftt ├ BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
    { 0x09f5, 0x2524 }, /*                      rightt ┤ BOX DRAWINGS LIGHT VERTICAL AND LEFT */
    { 0x09f7, 0x252c }, /*                        topt ┬ BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
    { 0x09f6, 0x2534 }, /*                        bott ┴ BOX DRAWINGS LIGHT UP AND HORIZONTAL */
    { 0x09ee, 0x253c }, /*               crossinglines ┼ BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
    { 0x09e1, 0x2592 }, /*                checkerboard ▒ MEDIUM SHADE */
    { 0x0ae7, 0x25aa }, /*            enfilledsqbullet ▪ BLACK SMALL SQUARE */
    { 0x0ae1, 0x25ab }, /*          enopensquarebullet ▫ WHITE SMALL SQUARE */
    { 0x0adb, 0x25ac }, /*            filledrectbullet ▬ BLACK RECTANGLE */
    { 0x0ae2, 0x25ad }, /*              openrectbullet ▭ WHITE RECTANGLE */
    { 0x0adf, 0x25ae }, /*                emfilledrect ▮ BLACK VERTICAL RECTANGLE */
    { 0x0acf, 0x25af }, /*             emopenrectangle ▯ WHITE VERTICAL RECTANGLE */
    { 0x0ae8, 0x25b2 }, /*           filledtribulletup ▲ BLACK UP-POINTING TRIANGLE */
    { 0x0ae3, 0x25b3 }, /*             opentribulletup △ WHITE UP-POINTING TRIANGLE */
    { 0x0add, 0x25b6 }, /*        filledrighttribullet ▶ BLACK RIGHT-POINTING TRIANGLE */
    { 0x0acd, 0x25b7 }, /*           rightopentriangle ▷ WHITE RIGHT-POINTING TRIANGLE */
    { 0x0ae9, 0x25bc }, /*         filledtribulletdown ▼ BLACK DOWN-POINTING TRIANGLE */
    { 0x0ae4, 0x25bd }, /*           opentribulletdown ▽ WHITE DOWN-POINTING TRIANGLE */
    { 0x0adc, 0x25c0 }, /*         filledlefttribullet ◀ BLACK LEFT-POINTING TRIANGLE */
    { 0x0acc, 0x25c1 }, /*            leftopentriangle ◁ WHITE LEFT-POINTING TRIANGLE */
    { 0x09e0, 0x25c6 }, /*                soliddiamond ◆ BLACK DIAMOND */
    { 0x0ace, 0x25cb }, /*                emopencircle ○ WHITE CIRCLE */
    { 0x0ade, 0x25cf }, /*              emfilledcircle ● BLACK CIRCLE */
    { 0x0ae0, 0x25e6 }, /*            enopencircbullet ◦ WHITE BULLET */
    { 0x0ae5, 0x2606 }, /* o openstar */
    { 0x0af9, 0x260e }, /* . telephone */
    { 0x0aca, 0x2613 }, /* o signaturemark */
    { 0x0aea, 0x261c }, /* o leftpointer */
    { 0x0aeb, 0x261e }, /* o rightpointer */
    { 0x0af8, 0x2640 }, /* . femalesymbol */
    { 0x0af7, 0x2642 }, /* . malesymbol */
    { 0x0aec, 0x2663 }, /* . club */
    { 0x0aee, 0x2665 }, /*                       heart ♥ BLACK HEART SUIT */
    { 0x0aed, 0x2666 }, /*                     diamond ♦ BLACK DIAMOND SUIT */
    { 0x0af6, 0x266d }, /*                 musicalflat ♭ MUSIC FLAT SIGN */
    { 0x0af5, 0x266f }, /*                musicalsharp ♯ MUSIC SHARP SIGN */
    { 0x0af3, 0x2713 }, /*                   checkmark ✓ CHECK MARK */
    { 0x0af4, 0x2717 }, /*                 ballotcross ✗ BALLOT X */
    { 0x0ad9, 0x271d }, /*                  latincross ✝ LATIN CROSS */
    { 0x0af0, 0x2720 }, /*                maltesecross ✠ MALTESE CROSS */
    { 0x0abc, 0x27e8 }, /* o leftanglebracket */
    { 0x0abe, 0x27e9 }, /* o rightanglebracket */
    { 0x04a4, 0x3001 }, /*                  kana_comma 、 IDEOGRAPHIC COMMA */
    { 0x04a1, 0x3002 }, /*               kana_fullstop 。 IDEOGRAPHIC FULL STOP */
    { 0x04a2, 0x300c }, /*         kana_openingbracket 「 LEFT CORNER BRACKET */
    { 0x04a3, 0x300d }, /*         kana_closingbracket 」 RIGHT CORNER BRACKET */
    { 0xfe5e, 0x3099 }, /* f dead_voiced_sound */
    { 0xfe5f, 0x309a }, /* f dead_semivoiced_sound */
    { 0x04de, 0x309b }, /*                 voicedsound ゛ KATAKANA-HIRAGANA VOICED SOUND MARK */
    { 0x04df, 0x309c }, /*             semivoicedsound ゜ KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK */
    { 0x04a7, 0x30a1 }, /*                      kana_a ァ KATAKANA LETTER SMALL A */
    { 0x04b1, 0x30a2 }, /*                      kana_A ア KATAKANA LETTER A */
    { 0x04a8, 0x30a3 }, /*                      kana_i ィ KATAKANA LETTER SMALL I */
    { 0x04b2, 0x30a4 }, /*                      kana_I イ KATAKANA LETTER I */
    { 0x04a9, 0x30a5 }, /*                      kana_u ゥ KATAKANA LETTER SMALL U */
    { 0x04b3, 0x30a6 }, /*                      kana_U ウ KATAKANA LETTER U */
    { 0x04aa, 0x30a7 }, /*                      kana_e ェ KATAKANA LETTER SMALL E */
    { 0x04b4, 0x30a8 }, /*                      kana_E エ KATAKANA LETTER E */
    { 0x04ab, 0x30a9 }, /*                      kana_o ォ KATAKANA LETTER SMALL O */
    { 0x04b5, 0x30aa }, /*                      kana_O オ KATAKANA LETTER O */
    { 0x04b6, 0x30ab }, /*                     kana_KA カ KATAKANA LETTER KA */
    { 0x04b7, 0x30ad }, /*                     kana_KI キ KATAKANA LETTER KI */
    { 0x04b8, 0x30af }, /*                     kana_KU ク KATAKANA LETTER KU */
    { 0x04b9, 0x30b1 }, /*                     kana_KE ケ KATAKANA LETTER KE */
    { 0x04ba, 0x30b3 }, /*                     kana_KO コ KATAKANA LETTER KO */
    { 0x04bb, 0x30b5 }, /*                     kana_SA サ KATAKANA LETTER SA */
    { 0x04bc, 0x30b7 }, /*                    kana_SHI シ KATAKANA LETTER SI */
    { 0x04bd, 0x30b9 }, /*                     kana_SU ス KATAKANA LETTER SU */
    { 0x04be, 0x30bb }, /*                     kana_SE セ KATAKANA LETTER SE */
    { 0x04bf, 0x30bd }, /*                     kana_SO ソ KATAKANA LETTER SO */
    { 0x04c0, 0x30bf }, /*                     kana_TA タ KATAKANA LETTER TA */
    { 0x04c1, 0x30c1 }, /*                    kana_CHI チ KATAKANA LETTER TI */
    { 0x04af, 0x30c3 }, /*                    kana_tsu ッ KATAKANA LETTER SMALL TU */
    { 0x04c2, 0x30c4 }, /*                    kana_TSU ツ KATAKANA LETTER TU */
    { 0x04c3, 0x30c6 }, /*                     kana_TE テ KATAKANA LETTER TE */
    { 0x04c4, 0x30c8 }, /*                     kana_TO ト KATAKANA LETTER TO */
    { 0x04c5, 0x30ca }, /*                     kana_NA ナ KATAKANA LETTER NA */
    { 0x04c6, 0x30cb }, /*                     kana_NI ニ KATAKANA LETTER NI */
    { 0x04c7, 0x30cc }, /*                     kana_NU ヌ KATAKANA LETTER NU */
    { 0x04c8, 0x30cd }, /*                     kana_NE ネ KATAKANA LETTER NE */
    { 0x04c9, 0x30ce }, /*                     kana_NO ノ KATAKANA LETTER NO */
    { 0x04ca, 0x30cf }, /*                     kana_HA ハ KATAKANA LETTER HA */
    { 0x04cb, 0x30d2 }, /*                     kana_HI ヒ KATAKANA LETTER HI */
    { 0x04cc, 0x30d5 }, /*                     kana_FU フ KATAKANA LETTER HU */
    { 0x04cd, 0x30d8 }, /*                     kana_HE ヘ KATAKANA LETTER HE */
    { 0x04ce, 0x30db }, /*                     kana_HO ホ KATAKANA LETTER HO */
    { 0x04cf, 0x30de }, /*                     kana_MA マ KATAKANA LETTER MA */
    { 0x04d0, 0x30df }, /*                     kana_MI ミ KATAKANA LETTER MI */
    { 0x04d1, 0x30e0 }, /*                     kana_MU ム KATAKANA LETTER MU */
    { 0x04d2, 0x30e1 }, /*                     kana_ME メ KATAKANA LETTER ME */
    { 0x04d3, 0x30e2 }, /*                     kana_MO モ KATAKANA LETTER MO */
    { 0x04ac, 0x30e3 }, /*                     kana_ya ャ KATAKANA LETTER SMALL YA */
    { 0x04d4, 0x30e4 }, /*                     kana_YA ヤ KATAKANA LETTER YA */
    { 0x04ad, 0x30e5 }, /*                     kana_yu ュ KATAKANA LETTER SMALL YU */
    { 0x04d5, 0x30e6 }, /*                     kana_YU ユ KATAKANA LETTER YU */
    { 0x04ae, 0x30e7 }, /*                     kana_yo ョ KATAKANA LETTER SMALL YO */
    { 0x04d6, 0x30e8 }, /*                     kana_YO ヨ KATAKANA LETTER YO */
    { 0x04d7, 0x30e9 }, /*                     kana_RA ラ KATAKANA LETTER RA */
    { 0x04d8, 0x30ea }, /*                     kana_RI リ KATAKANA LETTER RI */
    { 0x04d9, 0x30eb }, /*                     kana_RU ル KATAKANA LETTER RU */
    { 0x04da, 0x30ec }, /*                     kana_RE レ KATAKANA LETTER RE */
    { 0x04db, 0x30ed }, /*                     kana_RO ロ KATAKANA LETTER RO */
    { 0x04dc, 0x30ef }, /*                     kana_WA ワ KATAKANA LETTER WA */
    { 0x04a6, 0x30f2 }, /*                     kana_WO ヲ KATAKANA LETTER WO */
    { 0x04dd, 0x30f3 }, /*                      kana_N ン KATAKANA LETTER N */
    { 0x04a5, 0x30fb }, /*            kana_conjunctive ・ KATAKANA MIDDLE DOT */
    { 0x04b0, 0x30fc }, /*              prolongedsound ー KATAKANA-HIRAGANA PROLONGED SOUND MARK */
    { 0x0ea1, 0x3131 }, /*               Hangul_Kiyeog ㄱ HANGUL LETTER KIYEOK */
    { 0x0ea2, 0x3132 }, /*          Hangul_SsangKiyeog ㄲ HANGUL LETTER SSANGKIYEOK */
    { 0x0ea3, 0x3133 }, /*           Hangul_KiyeogSios ㄳ HANGUL LETTER KIYEOK-SIOS */
    { 0x0ea4, 0x3134 }, /*                Hangul_Nieun ㄴ HANGUL LETTER NIEUN */
    { 0x0ea5, 0x3135 }, /*           Hangul_NieunJieuj ㄵ HANGUL LETTER NIEUN-CIEUC */
    { 0x0ea6, 0x3136 }, /*           Hangul_NieunHieuh ㄶ HANGUL LETTER NIEUN-HIEUH */
    { 0x0ea7, 0x3137 }, /*               Hangul_Dikeud ㄷ HANGUL LETTER TIKEUT */
    { 0x0ea8, 0x3138 }, /*          Hangul_SsangDikeud ㄸ HANGUL LETTER SSANGTIKEUT */
    { 0x0ea9, 0x3139 }, /*                Hangul_Rieul ㄹ HANGUL LETTER RIEUL */
    { 0x0eaa, 0x313a }, /*          Hangul_RieulKiyeog ㄺ HANGUL LETTER RIEUL-KIYEOK */
    { 0x0eab, 0x313b }, /*           Hangul_RieulMieum ㄻ HANGUL LETTER RIEUL-MIEUM */
    { 0x0eac, 0x313c }, /*           Hangul_RieulPieub ㄼ HANGUL LETTER RIEUL-PIEUP */
    { 0x0ead, 0x313d }, /*            Hangul_RieulSios ㄽ HANGUL LETTER RIEUL-SIOS */
    { 0x0eae, 0x313e }, /*           Hangul_RieulTieut ㄾ HANGUL LETTER RIEUL-THIEUTH */
    { 0x0eaf, 0x313f }, /*          Hangul_RieulPhieuf ㄿ HANGUL LETTER RIEUL-PHIEUPH */
    { 0x0eb0, 0x3140 }, /*           Hangul_RieulHieuh ㅀ HANGUL LETTER RIEUL-HIEUH */
    { 0x0eb1, 0x3141 }, /*                Hangul_Mieum ㅁ HANGUL LETTER MIEUM */
    { 0x0eb2, 0x3142 }, /*                Hangul_Pieub ㅂ HANGUL LETTER PIEUP */
    { 0x0eb3, 0x3143 }, /*           Hangul_SsangPieub ㅃ HANGUL LETTER SSANGPIEUP */
    { 0x0eb4, 0x3144 }, /*            Hangul_PieubSios ㅄ HANGUL LETTER PIEUP-SIOS */
    { 0x0eb5, 0x3145 }, /*                 Hangul_Sios ㅅ HANGUL LETTER SIOS */
    { 0x0eb6, 0x3146 }, /*            Hangul_SsangSios ㅆ HANGUL LETTER SSANGSIOS */
    { 0x0eb7, 0x3147 }, /*                Hangul_Ieung ㅇ HANGUL LETTER IEUNG */
    { 0x0eb8, 0x3148 }, /*                Hangul_Jieuj ㅈ HANGUL LETTER CIEUC */
    { 0x0eb9, 0x3149 }, /*           Hangul_SsangJieuj ㅉ HANGUL LETTER SSANGCIEUC */
    { 0x0eba, 0x314a }, /*                Hangul_Cieuc ㅊ HANGUL LETTER CHIEUCH */
    { 0x0ebb, 0x314b }, /*               Hangul_Khieuq ㅋ HANGUL LETTER KHIEUKH */
    { 0x0ebc, 0x314c }, /*                Hangul_Tieut ㅌ HANGUL LETTER THIEUTH */
    { 0x0ebd, 0x314d }, /*               Hangul_Phieuf ㅍ HANGUL LETTER PHIEUPH */
    { 0x0ebe, 0x314e }, /*                Hangul_Hieuh ㅎ HANGUL LETTER HIEUH */
    { 0x0ebf, 0x314f }, /*                    Hangul_A ㅏ HANGUL LETTER A */
    { 0x0ec0, 0x3150 }, /*                   Hangul_AE ㅐ HANGUL LETTER AE */
    { 0x0ec1, 0x3151 }, /*                   Hangul_YA ㅑ HANGUL LETTER YA */
    { 0x0ec2, 0x3152 }, /*                  Hangul_YAE ㅒ HANGUL LETTER YAE */
    { 0x0ec3, 0x3153 }, /*                   Hangul_EO ㅓ HANGUL LETTER EO */
    { 0x0ec4, 0x3154 }, /*                    Hangul_E ㅔ HANGUL LETTER E */
    { 0x0ec5, 0x3155 }, /*                  Hangul_YEO ㅕ HANGUL LETTER YEO */
    { 0x0ec6, 0x3156 }, /*                   Hangul_YE ㅖ HANGUL LETTER YE */
    { 0x0ec7, 0x3157 }, /*                    Hangul_O ㅗ HANGUL LETTER O */
    { 0x0ec8, 0x3158 }, /*                   Hangul_WA ㅘ HANGUL LETTER WA */
    { 0x0ec9, 0x3159 }, /*                  Hangul_WAE ㅙ HANGUL LETTER WAE */
    { 0x0eca, 0x315a }, /*                   Hangul_OE ㅚ HANGUL LETTER OE */
    { 0x0ecb, 0x315b }, /*                   Hangul_YO ㅛ HANGUL LETTER YO */
    { 0x0ecc, 0x315c }, /*                    Hangul_U ㅜ HANGUL LETTER U */
    { 0x0ecd, 0x315d }, /*                  Hangul_WEO ㅝ HANGUL LETTER WEO */
    { 0x0ece, 0x315e }, /*                   Hangul_WE ㅞ HANGUL LETTER WE */
    { 0x0ecf, 0x315f }, /*                   Hangul_WI ㅟ HANGUL LETTER WI */
    { 0x0ed0, 0x3160 }, /*                   Hangul_YU ㅠ HANGUL LETTER YU */
    { 0x0ed1, 0x3161 }, /*                   Hangul_EU ㅡ HANGUL LETTER EU */
    { 0x0ed2, 0x3162 }, /*                   Hangul_YI ㅢ HANGUL LETTER YI */
    { 0x0ed3, 0x3163 }, /*                    Hangul_I ㅣ HANGUL LETTER I */
    { 0x0eef, 0x316d }, /*     Hangul_RieulYeorinHieuh ㅭ HANGUL LETTER RIEUL-YEORINHIEUH */
    { 0x0ef0, 0x3171 }, /*    Hangul_SunkyeongeumMieum ㅱ HANGUL LETTER KAPYEOUNMIEUM */
    { 0x0ef1, 0x3178 }, /*    Hangul_SunkyeongeumPieub ㅸ HANGUL LETTER KAPYEOUNPIEUP */
    { 0x0ef2, 0x317f }, /*              Hangul_PanSios ㅿ HANGUL LETTER PANSIOS */
    { 0x0ef3, 0x3181 }, /*    Hangul_KkogjiDalrinIeung ㆁ HANGUL LETTER YESIEUNG */
    { 0x0ef4, 0x3184 }, /*   Hangul_SunkyeongeumPhieuf ㆄ HANGUL LETTER KAPYEOUNPHIEUPH */
    { 0x0ef5, 0x3186 }, /*          Hangul_YeorinHieuh ㆆ HANGUL LETTER YEORINHIEUH */
    { 0x0ef6, 0x318d }, /*                Hangul_AraeA ㆍ HANGUL LETTER ARAEA */
    { 0x0ef7, 0x318e }  /*               Hangul_AraeAE ㆎ HANGUL LETTER ARAEAE */
};

int ucs2keysym (int ucs) {
    int min = 0;
    int max = sizeof(keysymtabbyucs) / sizeof(struct codepairbyucs) - 1;
    int mid;

    /* first check for Latin-1 characters (1:1 mapping) */
    if ((ucs >= 0x0020 && ucs <= 0x007e) ||
        (ucs >= 0x00a0 && ucs <= 0x00ff)) {
        //printf("Direct conversion to: %#06x\n", ucs);
        return ucs;
    }
    
    while (max >= min) {
        mid = (min + max) / 2;
        if (keysymtabbyucs[mid].ucs < ucs)
            min = mid + 1;
        else if (keysymtabbyucs[mid].ucs > ucs)
            max = mid - 1;
        else {
            //printf("Found keysym %#06x for unicode %#06x\n", keysymtabbyucs[mid].keysym, keysymtabbyucs[mid].ucs);
            return keysymtabbyucs[mid].keysym;
        }
    }

    /* no matching keysym value found, just return the unicode character */
    //printf("Unicode conversion failed, returning -1.\n");
    return -1;
}

long keysym2ucs(long keysym) {
    int min = 0;
    int max = sizeof(keysymtab) / sizeof(struct codepair) - 1;
    int mid;

    /* first check for Latin-1 characters (1:1 mapping) */
    if ((keysym >= 0x0020 && keysym <= 0x007e) ||
            (keysym >= 0x00a0 && keysym <= 0x00ff))
        return keysym;

    /* also check for directly encoded 24-bit UCS characters */
    if ((keysym & 0xff000000) == 0x01000000)
        return keysym & 0x00ffffff;

    /* binary search in table */
    while (max >= min) {
        mid = (min + max) / 2;
        if (keysymtab[mid].keysym < keysym)
            min = mid + 1;
        else if (keysymtab[mid].keysym > keysym)
            max = mid - 1;
        else {
            /* found it */
            return keysymtab[mid].ucs;
        }
    }

    /* no matching Unicode value found */
    return -1;
}
