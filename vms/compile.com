$ DEFINE/NOLOG VAXC$INCLUDE TWG$TCP:[NETDIST.INCLUDE], -
                            TWG$TCP:[NETDIST.INCLUDE.VMS], -
                            TWG$TCP:[NETDIST.INCLUDE.NETINET], -
                            SYS$LIBRARY
$ !
$ DEFINE/NOLOG SYS TWG$TCP:[NETDIST.INCLUDE.SYS]
$ DEFINE/NOLOG NETINET TWG$TCP:[NETDIST.INCLUDE.NETINET]
$ !
$ CC BSD.C       
$ CC C_VMS.C           
$ CC C_MSG.C           
$ CC/DEFINE=(CLIENT) CONF.C           
$ CC DEBUG.C          
$ CC EDIT.C           
$ CC HELP.C           
$ CC IGNORE.C
$ CC IRC.C            
$ CC PACKET.C         
$ CC PARSE.C          
$ CC SCREEN.C         
$ CC SEND.C
$ CC STR.C            
$ CC TERMCAP.C
$ CC NUMERIC.C
