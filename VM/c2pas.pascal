(* ***********************************************************************
 *   IRC - Internet Relay Chat, VM/c2pas.pascal
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
 *)
 
segment IRCdummy;
 
%include CMALLCL
 
type Cstring = packed array(.1..1024.) of char;
 
const
   CBUFFERspaceAVAILABLE       = '80000000'x;
   CCONNECTIONstateCHANGED     = '40000000'x;
   CDATAdelivered              = '20000000'x;
   CDATAGRAMdelivered          = '10000000'x;
   CDATAGRAMspaceAVAILABLE     = '08000000'x;
   CURGENTpending              = '04000000'x;
   CUDPdatagramDELIVERED       = '02000000'x;
   CUDPdatagramSPACEavailable  = '01000000'x;
   CEXTERNALinterrupt          = '00800000'x;
   CUSERdeliversLINE           = '00400000'x;
   CUSERwantsATTENTION         = '00200000'x;
   CTIMERexpired               = '00100000'x;
   CIUCVinterrupt              = '00080000'x;
   CRESOURCESavailable         = '00040000'x;
   CSMSGreceived               = '00000400'x;
 
function cstrlen(str: CString): integer;
var cnt: integer;
begin
cnt:=0;
while str(.cnt+1.) <> chr(0) do
      cnt:=cnt+1;
cstrlen:=cnt;
end;
 
procedure Cenableeyeucv(var env: integer); reentrant;
procedure Cenableeyeucv;
begin
enableeyeucv;
end;
 
procedure Cdisableeyeucv(var env: integer); reentrant;
procedure Cdisableeyeucv;
begin
disableeyeucv;
end;
 
static fsmode: boolean;
 
procedure Cfsstartup(var env: integer; const mode: integer;
          const line: CString); reentrant;
procedure Cfsstartup;
 
var cstr: string(256);
 
begin
   if mode=0 then
      fsmode:=false
   else
      fsmode:=true;
   cstr:=substr(str(line), 1, cstrlen(line));
   fsstartup(fsmode, cstr);
end;
 
procedure Cfsstop(var env: integer); reentrant;
procedure Cfsstop;
begin
   fsstop;
end;
 
procedure coutstrln(var env:integer; var line: Cstring); reentrant;
procedure coutstrln;
 
var cstr: string(1024);
    len:  integer;
 
begin
   len:=cstrlen(line);
   cstr:=substr(str(line), 1, len);
   if fsinuse then begin
      if fsmode then fstransout(cstr(.1.), len)
         else outstrln(cstr);
      fsflush;
      end
   else writeln(cstr);
end;
 
procedure cfsreceive(var env: integer;
            var buffer: CString; var rc:integer); reentrant;
procedure cfsreceive;
 
var  prc: integer;
 
begin
     fsreceive(buffer(.1.), prc);
     rc:=prc;
end;
 
procedure cfsblankline(var env: integer); reentrant;
procedure cfsblankline;
begin
fsblankline;
end;
 
procedure cfsseeline(var env: integer); reentrant;
procedure cfsseeline;
begin
fsseeline;
end;
 
procedure cfsflush(var env: integer); reentrant;
procedure cfsflush;
begin
fsflush;
end;
 
procedure cfsuserline(var env: integer; const line: CString;
                  f1, f2: integer); reentrant;
procedure cfsuserline;
var firstfield, secondfield: boolean;
    bodyfirst: string(256);
begin
if f1 = 0 then firstfield:=false
          else firstfield:=true;
if f2 = 0 then secondfield:=false
          else secondfield:=true;
bodyfirst:=substr(str(line), 1, cstrlen(line));
fsuserline(bodyfirst, firstfield, secondfield);
end;
 
 
procedure CMyNextNote(var env: integer; const shldwait: integer;
   var notetype: integer; var connnum: integer;
   var urgbytes: integer; var urgspan: integer; var bytesdel: integer;
   var lasturg: integer; var push: integer; var amtspc: integer;
   var newstat: integer; var reas: integer; var datm: integer;
   var assctmr: integer; var rupt: integer;
   var datalen: integer; var foraddr: integer; var forport: integer;
   var iucvbuf: IUCVPointerType;
   var retcode: integer);
   reentrant;
procedure CMyNextNote;
var
   tempnote: NotificationInfoType;
   ShouldWait: boolean;
   pascretcode: integer;
 
function RetcCtoP(Ccode: integer): CallReturnCodeType; external;
function RetcPtoC(Pcode: CallReturnCodeType): integer; external;
function MaskToNote(Mask: integer): NotificationSetType; external;
function ConnPtoC(ConnectionState: ConnectionStateType): integer;
   external;
function ConnCtoP(connstat: integer): ConnectionStateType; external;
 
begin
termin(input); termout(output);
   if shldwait = 0 then ShouldWait := false
   else ShouldWait := true;
   GetNextNote(tempnote, ShouldWait, pascretcode);
   connnum := tempnote.Connection;
   with tempnote do
   begin
   case NotificationTag of
 
      USERwantsATTENTION:
         begin
         notetype := CUSERwantsATTENTION;
         reas := ord(WhichAttentionKey);
         end;
      USERdeliversLINE:
         begin
         notetype := CUSERdeliversLINE;
         bytesdel := LengthofUserData;
         end;
      URGENTpending:
         begin
         notetype := CURGENTpending;
         urgbytes := BytesToRead;
         urgspan := UrgentSpan;
         end;
      DATAdelivered:
         begin
         notetype := CDATAdelivered;
         bytesdel := BytesDelivered;
         lasturg := LastUrgentByte;
         if PushFlag then push := 1
         else push := 0;
         end;
      BUFFERspaceAVAILABLE:
         begin
         notetype := CBUFFERspaceAVAILABLE;
         amtspc := AmountOfSpaceInBytes;
         end;
      CONNECTIONstateCHANGED:
         begin
         notetype := CCONNECTIONstateCHANGED;
         newstat := ConnPtoC(tempnote.NewState);
         reas := RetcPtoC(Reason);
         end;
      TIMERexpired:
         begin
         notetype := CTIMERexpired;
         datm := Datum;
         assctmr := ord(AssociatedTimer);
         end;
      EXTERNALinterrupt:
         begin
         notetype := CEXTERNALinterrupt;
         rupt := RuptCode;
         end;
      UDPdatagramDELIVERED:
         begin
         notetype := CUDPdatagramDELIVERED;
         datalen := DataLength;
         foraddr := ForeignSocket.Address;
         forport := ForeignSocket.Port;
         end;
      UDPdatagramSPACEavailable:
         begin
         notetype := CUDPdatagramSPACEavailable;
         end;
      IUCVinterrupt:
         begin
         notetype := CIUCVinterrupt;
         iucvbuf@ := IUCVResponseBuf;
         end;
      RESOURCESavailable:
         begin
         notetype := CRESOURCESavailable;
         end;
      SMSGreceived:
         begin
         notetype := CSMSGreceived;
         end;
      otherwise ;
      end; { case NotificationTag }
      end; { with }
retcode := RetcPtoC(pascretcode);
end;
