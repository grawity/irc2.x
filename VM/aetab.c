/************************************************************************
 *   IRC - Internet Relay Chat, VM/aetab.c
 *   Copyright (C) 1990 Armin Gruner
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  ascii - ebcdic translation tables
 */
 
  /*
  ** map nonprintable characters to SPACE
  */
  
char a2e_table[] = {
' ',      /* 000 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  '\t', '\n',     /* 010 */
' ',  ' ',  '\r', ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 020 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 030 */
' ',  ' ',  '!',  '"',  '#',  '$',  '%',  '&',  '\'', '(',      /* 040 */
')',  '*',  '+',  ',',  '-',  '.',  '/',  '0',  '1',  '2',      /* 050 */
'3',  '4',  '5',  '6',  '7',  '8',  '9',  ':',  ';',  '<',      /* 060 */
'=',  '>',  '?',  '@',  'A',  'B',  'C',  'D',  'E',  'F',      /* 070 */
'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',  'P',      /* 080 */
'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z',      /* 090 */
'[',  '\\', ']',  '^',  '_',  '`',  'a',  'b',  'c',  'd',      /* 100 */
'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',      /* 110 */
'o',  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',      /* 120 */
'y',  'z',  '{',  '|',  '}',  '~',  ' ',  ' ',  ' ',  ' ',      /* 130 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 140 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 150 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 160 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 170 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 180 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 190 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 200 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 210 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 220 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 230 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 240 */
' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',      /* 250 */
' ',  ' ',  ' ',  ' ',  ' ',  };

char e2a_table[256];
#define ASCII_SPACE 0x20
init_e2atable()
{
    int i;
    for (i = 0; i < 256; i++)
      e2a_table[i] = ASCII_SPACE;
    for (i = 0; i < 256; i++)
      e2a_table[a2e_table[i]] = (a2e_table[i] != ' ') ? i : ASCII_SPACE;
}
