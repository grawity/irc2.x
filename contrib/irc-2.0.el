;;;;;;;;;;;;;;;;;;;;;;;;;;; -*- Mode: Emacs-Lisp -*- ;;;;;;;;;;;;;;;;;;;;;;;;;;
;; irc.el --- A user interface for the Internet Relay Chat
;; Author          : David C Lawrence           <tale@pawl.rpi.edu>
;; Created On      : Wed Jun 14 22:22:57 1989
;; Last Modified By: Gnu Maint Acct
;; Last Modified On: Wed Sep 27 19:05:45 1989
;; Update Count    : 6
;; Status          : Seemingly stable.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Copyright (C) 1989  David C Lawrence

;;  This program is free software; you can redistribute it and/or modify
;;  it under the terms of the GNU General Public License as published by
;;  the Free Software Foundation; either version 1, or (at your option)
;;  any later version.

;;  This program is distributed in the hope that it will be useful,
;;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;  GNU General Public License for more details.

;;  You should have received a copy of the GNU General Public License
;;  along with this program; if not, write to the Free Software
;;  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

;; Comments and/or bug reports about this interface should be directed to:
;;     Dave Lawrence          <tale@{pawl,itsgw}.rpi.edu>
;;     76 1/2 13th Street     +1 518 273 5385
;;     Troy NY 12180          Generally available on IRC as "tale"

;; History 		
;; 27-Sep-1989		Gnu Maint Acct	(gnu at life.pawl.rpi.edu)
;;    Last Modified: Wed Sep 27 18:56:19 1989 #6
;;    * Fixed a misrefenced variable in irc-parse-server-msg so that lines
;;      which can't be parsed insert only that unparsable line.
;;    * Added irc-parse-topic and changed irc-signals and irc-notifies
;;      appropriately.
;;    * Moved setting of irc-channel to irc-parse-channel from irc-execute-join
;;      because a channel change isn't guaranteed (ie, trying to change to
;;      a channel not in 0-10 which has ten users on it).
;;    * Made irc-message-stamp 'private by default. (Geoff)
;;    * Made irc-time-stamp 0 by default.      (Also Geoff)
;;      This tickled an unseen bug in the start-up function which incremented
;;      irc-last-stamp.  It's fixed now.
;;    * Fixed a bug with interactive irc-execute-names -- prefix arg was being
;;      (list ...)ed.
;;    * Fixed a bug in irc-parse-namreply that dropped a name in the output
;;      when a line got wrapped.
;;    * Added /lusers.  I really dislike the name.
;;    * Added irc-parse-kill.
;;    * Made irc-execute-msg strip one space after the colon in messages;
;;      ie, "tale: hi" will send "hi" as a message not " hi".
;;    * Took out hardcoded width numbers for word-wrapping.  Uses
;;      (window-width) instead.
;;    * Added to the regexp for failed nickname changes in irc-parse-error;
;;      the sentences got changed in the newer servers.
;;    * Wrapped irc-check-time around display-time-filter for better accuracy
;;      if a display-time process is running.
;;    * Made the "IRC Session finished" message include the time.
;;    * Added irc-conserve-space to imitate the C client message style.  I
;;      really dislike it aesthetically, but it is handy for people running
;;      on 24 line screens and at slow baud rates. (Kimmel & others)
;;    * Support IRCNICK and IRCSERVER environment variables at load time. (Kim)
;;    * Made /WHO call /WHOIS if given a non-numeric argument.  (Nate)
;;    * Fixed a bug where an alias would be executed before an exactly matching
;;      command; ie, alias /WHOALIAS whould execute when /WHO was entered.
;;    * Runs things in auto-fill-mode with a fill-column of 75 by default.
;;    * Processes the whole input region as one line instead of line-at-a-time.
;;    * Made irc-change-alias for non-interactive setting/removing of aliases;
;;      ie, via irc-mode-hook.  No frills.  (Chris & Nate)
;;    * _Finally_ got around to adding parsing of numeric notices for new
;;      servers.

;; Defined variables
(provide 'irc)

(defvar irc-server (or (getenv "IRCSERVER") "your.server.here")
  "*A host running the IRC daemon.
IRC servers generally restrict which machines can maintain connexions with
them, so you'll probably have to find a server in your local domain.")

(defvar irc-port 6667
  "*The port on which the IRC server responds.
Many sites don't have irc as a named service (ie, it has no entry in
/etc/inetd.conf) so you might have to set this to a number; the most
common configuration is to have IRC respond on port 6667.")

(defvar irc-oops "Oops ... please ignore that."
  "*The text to send to the original IRC message recipient when using /OOPS.")

(defvar irc-message-stamp 'private
  "*Mark messages received in IRC with the time of their arrival if non-nil.
If this is the symbol 'private' or 'public' then only messages of the specified
type are marked with the time.  WALL messages are always time-stamped.")

(defvar irc-time-stamp 0
  "*How often to insert a time-stamp into *IRC* buffers.
The first time one is based from the hour the IRC process was started so that
values which divide evenly into 60 minutes (like the default of 10) will split
the hour evenly (as in 13:10, 13:20, 13:30, et cetera).  To disable the
time-stamping set this variable to 0.  This can be set with the /STAMP command.

The accuracy of the time-stamping can be improved greatly by running
M-x display-time; with default settings this guarantees that Emacs will have
some sort of predictable activity every minute.  If display-time is not running
and the IRC session is idle, the time-stamp can be up to two minutes late.")

(make-variable-buffer-local
 (defvar irc-nick (or (getenv "IRCNICK") (user-login-name))
   "*The nickname with which to enter IRC.
The default value is set from your login name.  Using /NICKNAME changes it."))

(defvar irc-spacebar-pages t
  "*When this variable is non-nil, the following keys are in effect when
point is in the output region.

SPC      scroll-forward    DEL           scroll-backward
TAB      previous-line     LFD or RET    next-line")

(defvar irc-maximum-size 20480
  "*Maximum size that the *IRC* buffer can attain, in bytes.
The default value of 20k represents an average of about 512 lines, or roughly
22 screens on a standard 24 line monitor.")

(defvar irc-mode-hook nil
  "*Hook to run after starting irc-mode but before connecting to the server.")

(defvar irc-pop-on-signal 4
  "*An integer value means to display the *IRC* buffer when a signal is issued.
The number represents roughly how much of the Emacs screen to use when
popping up the IRC window if only one window is visible.  The reciprocal
is used, so a value of 1 causes the window to appear full-screen, 2 makes
the window half of the screen, 3 makes it one third, et cetera.  If the value
is not an integer then no attempt is made to show the *IRC* buffer if it
is not already visible.")

(defvar irc-max-history 40
  "*The maximum number of messages retained by irc-mode.
This limits messages sent, not messages received.  They are stored to be
easily recalled by irc-history-prev and irc-history-next (C-c C-p and C-c C-n
by default).")

(defvar irc-conserve-space nil
  "*If this variable is set non-nil then the C client will be partially
mimicked for message insertion.  Private messages will be prefixed by
\"*SENDER*\" and public messages will be prefixed by \"<SENDER>\".  No blank
lines are put in the buffer.")

(defvar irc-confirm t
  "*If non-nil, provide confirmation for messages sent on IRC.
It should be noted that confirmation only indicates where irc-mode
tried to send the message, not whether it was actually received.
Use the /CONFIRM command to change.")

(defvar irc-processes nil
  "All currently open streams to irc-servers are kept in this list.
It is used so that the 'irc' function knows whether to start a new process
by default.")

(make-variable-buffer-local
 (defvar irc-signals '((private t) (invite t) (wall t)
                       (public) (join) (nick) (topic) (user))
   "Events in IRC that should get signalled when they occur.
Generally the signal is an audible beep.  The value of irc-signals is an
association list of events recognized by irc-mode and is maintained with
the /SIGNAL command."))

(make-variable-buffer-local
 (defvar irc-ignores nil
   "A list of users whose events will be ignored.
Messages or other actions (like joining a channel) generated by anyone in
the list will not be displayed or signalled.  This list is maintained with
the /IGNORE and /UNIGNORE commands."))

(make-variable-buffer-local
 (defvar irc-notifies '(join nick topic)
   "Events in IRC that should get notification messages.
A notification message is just one line to indicate that the event has
occurred.  The \"join\", \"nick\" and \"topic\" events are supported by the
/NOTIFY command."))

(make-variable-buffer-local
 (defvar irc-history nil
   "A list of messages which irc-mode has processed.
This includes both successfully and unsuccessfully sent messages, starting
with the most recent first.  irc-max-history limits the number of items
that can appear in the list when using irc-add-to-history."))

(make-variable-buffer-local
 (defvar irc-default-to "*;"
   "The default recipient of a message if no : or ; is provided.
\"*\" means the current channel, no matter what it is."))

(defvar irc-mode-map nil
  "The keymap which irc-mode uses.

Currently set to: \\{irc-mode-map}")

(defvar irc-alias-alist
  '(("QUERY" . "send")        ; For people used to the C client
    ("N" . "names")           ; ditto
    ("W" . "who")             ; one more time
    ("?" . "help")            ; nice abbrev
    ("TF" . "time tut.fi")    ; surprising useful-won't work if tut.fi isn't up
    ("IDLE" . "who")          ; for people like me who are too used to Connect
    ("WHAT" . "list") ("L" . "list") ; /WHAT from Connect
    ("EXIT" . "quit") ("BYE" . "quit") ("STOP" . "quit")) ; Plenty of ways out
  "An association list of command aliases used in irc-mode.
This is the first list checked when irc-mode is looking for a command and it
is maintained with the /ALIAS and /UNALIAS commands.")

(defconst irc-command-alist
  '(("WHO" . "who")           ; For a list of users and their channels
    ("HELP" . "help")         ; Get help on the /COMMANDs
    ("INFO" . "info")         ; Information about the IRC author
    ("LIST" . "list")         ; Show a list of channels and topics
    ("OOPS" . "oops")         ; Resend a misdirected message
    ("OPER" . "oper")         ; Login as an IRC operator
    ("QUIT" . "quit")         ; Exit IRC
    ("SEND" . "send")         ; Set the implicit send list for messages
    ("TIME" . "time")         ; Get the current time from a server
    ("MSG" . "privmsg")       ; Send a private message to someone
    ("ADMIN" . "admin")       ; Get information about IRC admins
    ("LINKS" . "links")       ; Show which servers are in the IRC-net
    ("NAMES" . "names")       ; Display names of users on each channel
    ("QUOTE" . "quote")       ; Send raw text to the server
    ("TOPIC" . "topic")       ; Change the topic of the current channel
    ("USERS" . "users")       ; Show users signed on at a server
    ("WHOIS" . "whois")       ; Get slightly longer information about a user
    ("STAMP" . "stamp")       ; Set time notification interval
    ("LUSERS" . "lusers")     ; Get the number of users and servers
    ("INVITE" . "invite")     ; Ask another user to join your channel
    ("NOTIFY" . "notify")     ; Change which events give notification
    ("SIGNAL" . "signal")     ; Change which events give a signal
    ("SUMMON" . "summon")     ; Ask a user not on IRC to join it
    ("NICKNAME" . "nick")     ; Change your IRC nickname
    ("CONFIRM" . "confirm")   ; Set message confirmation on or off
    ("REDIRECT" . "redirect") ; Resend the last message to someone else
    ("AWAY" . "away") ("HERE" . "here") ; Give some indication of your presence
    ("JOIN" . "join") ("LEAVE" . "leave") ; Join or leave a channel
    ("ALIAS" . "alias") ("UNALIAS" . "unalias") ; Add/remove command aliases
    ("IGNORE" . "ignore") ("UNIGNORE" . "unignore")) ; (Un)Ignore a user
  "An association list of the regular commands to which all users have access.
Form is (\"COMMAND\" . \"function\") where \"function\" is that last element in
an irc-execute-* symbol.  See also irc-alias-alist and irc-operator-alist.")

(defconst irc-operator-alist
  '(("KILL" . "kill")         ; Forcibly remove a user
    ("WALL" . "wall")         ; Send a message to everyone on IRC
    ("TRACE" . "trace")       ; Show the links between servers
    ("REHASH" . "rehash"))    ; Reread irc.conf
  "As association list of commands which only an IRC Operator can use.
It is kept as a separate list so that regular users won't wonder how
come the commands don't work for them.")

(defconst irc-version "IRC-mode Version 2.0"
  "The currently loaded version of irc-mode")

;; keymap
;; what i tend to like for keys might be very different from what most people
;; find useful; in fact i tend to type the /COMMANDs more than use any bindings

;; there are more things bound here just so people can see the different
;; things available to them
(or irc-mode-map
    (progn
      (setq irc-mode-map (make-keymap))
      (define-key irc-mode-map "\C-j" 'irc-process-input)
      (define-key irc-mode-map "\C-m" 'irc-process-input)
      (define-key irc-mode-map "\C-i"      'irc-tab)
      (define-key irc-mode-map "\C-c\C-a"  'irc-execute-alias)
      (define-key irc-mode-map "\C-c\C-c"  'irc-execute-names)
      (define-key irc-mode-map "\C-c\C-h"  'irc-execute-help)
      (define-key irc-mode-map "\C-c\C-i"  'irc-execute-invite)
      (define-key irc-mode-map "\C-c\C-j"  'irc-execute-join)
      (define-key irc-mode-map "\C-c\C-l"  'irc-execute-list)
      (define-key irc-mode-map "\C-c\C-m"  'irc-history-menu)
      (define-key irc-mode-map "\C-c\C-n"  'irc-history-next)
      (define-key irc-mode-map "\C-c\C-o"  'irc-execute-oops)
      (define-key irc-mode-map "\C-c\C-p"  'irc-history-prev)
      (define-key irc-mode-map "\C-c\C-q"  'irc-execute-leave)
      (define-key irc-mode-map "\C-c\C-r"  'irc-execute-redirect)
      (define-key irc-mode-map "\C-c\C-s"  'irc-execute-send)
      (define-key irc-mode-map "\C-c\C-u"  'irc-kill-input)
      (define-key irc-mode-map "\C-c\C-w"  'irc-execute-who)
      (define-key irc-mode-map "\C-c\C-?"  'irc-kill-input)
      ;; it's nice to bind to a key while in development, but regular users
      ;; wonder about it in production.
      ;; (define-key irc-mode-map "\C-c " 'irc-pong)
      (define-key irc-mode-map "\C-c#" 'irc-execute-lusers)
      (define-key irc-mode-map "\C-ca" 'irc-execute-admin)
      (define-key irc-mode-map "\C-ci" 'irc-execute-info)
      (define-key irc-mode-map "\C-ck" 'irc-execute-quit)
      (define-key irc-mode-map "\C-cl" 'irc-execute-links)
      (define-key irc-mode-map "\C-cn" 'irc-news)
      (define-key irc-mode-map "\C-co" 'irc-execute-oper)
      (define-key irc-mode-map "\C-cp" 'irc-yank-prev-command)
      (define-key irc-mode-map "\C-cq" 'irc-execute-quote)
      (define-key irc-mode-map "\C-cs" 'irc-execute-summon)
      (define-key irc-mode-map "\C-cu" 'irc-execute-users)
      (define-key irc-mode-map "\C-cv" 'irc-version)
      (define-key irc-mode-map "\C-?"  'irc-del-backward-char)
      ;; make any self-inserting keys call irc-self-insert
      (mapcar (function
               (lambda (key)
                 (define-key irc-mode-map key 'irc-self-insert)))
              (where-is-internal 'self-insert-command nil nil))))

;; filters (mostly irc-parse-*)
;; Filtering of server messages from reception to insertion in the buffer
;; are all done on this page.  In particular, if a new server message has
;; to be dealt with, it should be added in the irc-parse-server-msg function.
(defun irc-filter (proc str)
  "Filtering procedure for IRC server messages.
It waits until everything up to a newline is accumulated before handing the
string over to irc-parse-server-msg to be processed.  If irc-pop-on-signal
is an integer and a signal is issued then the *IRC* buffer will be displayed."
  (let* ((ibuf (process-buffer proc))
         bell irc-mark-to-point new-point old-irc-mark win)
    (save-excursion
      (set-buffer ibuf)
      ;; still can't tell why i need this; sure, i probably change point
      ;; in ibuf.  but so what?  set-window-point should clean up after that.
      ;; it works with it though and not without it, so it stays.
      (save-excursion 
        (irc-truncate-buffer irc-maximum-size) ; trim buffer if needed
        (setq irc-mark-to-point   ; so we can restore relative position later
              (- (point) (setq old-irc-mark (goto-char irc-mark)))
              ;; just glue str to the end of any partial line that's there
              irc-scratch (concat irc-scratch str))
        ;; see if it is time for a message
        (irc-check-time)
        (while (string-match "\n" irc-scratch) ; do as many lines as possible
          ;; first use irc-scratch for the dp returned by irc-parse-server-msg
          (setq irc-scratch (irc-parse-server-msg irc-scratch)
                bell (cdr irc-scratch) ; issue a signal?
                ;; now irc-scratch is what it was, minus the line parsed
                irc-scratch (car irc-scratch))
          (if (not bell) ()
            ;; issue a signal;  no need to trash someone's kbd-macro over it
            (ding 'no-terminate)
            (minibuffer-message " [Bell in %s]" (buffer-name ibuf))))
        ;; if point was in front of the irc-mark, new-point is figured relative
        ;; to the old mark, otherwise it is relative to the new one
        (setq new-point (+ (if (< irc-mark-to-point 0) old-irc-mark irc-mark)
                           irc-mark-to-point))))
    ;; update point based on whether the buffer is visible
    ;; we have a real problem here if there is more than one window displaying
    ;; the process-buffer and the user is not in the first canonical one.
    ;; i haven't yet determined a nice way to solve this
    (if (setq win (get-buffer-window ibuf))
        (set-window-point win new-point)
      (save-excursion (set-buffer ibuf) (goto-char new-point)))
    ;; if *IRC* isn't visible, a bell was issued and irc-pop-on-signal is an
    ;; integer then show the buffer.
    (if (not (and (integerp irc-pop-on-signal) bell (not win))) ()
      (setq win (selected-window))
      (if (/= (count-windows 'no-mini) 1)
          (display-buffer ibuf) ; don't futz with sizes if more than 1 window
        ;; we might be in the mininbuffer at the moment, so insure that
        ;; this happens starting in the current regular window
        (select-window (next-window win 'no-mini))
        ;; full screen doesn't get handled well by the algorithm for the rest
        (if (= irc-pop-on-signal 1)
            (set-window-buffer (selected-window) ibuf)
          (split-window nil (- (screen-height)
                               (/ (screen-height) irc-pop-on-signal)))
          (display-buffer ibuf)
          ;; perhaps we need to go back to the minibuffer
          (select-window win))))))

(defun irc-parse-server-msg (str)
  "Take the first line from STR and pass it on to the appropriate irc-parse-*
function.  If the message is one which irc-mode doesn't recognize, just display
it as the raw server message.

It returns a dotted-pair whose car is the remainder of STR after the first
newline and whose cdr is either t or nil, indicating whether a signal should
be issued."
  (let ((loc 0) (line (substring str 0 (string-match "\n" str))))
    ;; need to double % signs or formatting later down the line will attempt
    ;; to interpret them.
    (while (string-match "%" line loc)
      (setq line (concat (substring line 0 (match-end 0)) "%"
                         (substring line (match-end 0)))
            loc (1+ (match-end 0))))
    (cons
     ;; the part of str not being parsed.
     (substring str (1+ (string-match "\n" str)))
     (cond
      ;; each function here should return t or nil indicating whether
      ;; to issue a signal.  Some of these regexps are fugly because
      ;; of inconsistent protocol use by the servers.  Fortunately Jarkko
      ;; is fixing that.
      ((string-match "^:\\S +\\s +MSG" line) (irc-parse-public line))
      ((string-match "^:\\S +\\s +CHANNEL" line) (irc-parse-channel line))
      ((string-match "^:\\S +\\s +INVITE" line) (irc-parse-invite line))
      ((string-match "^:\\S +\\s +NICK" line) (irc-parse-nick line))
      ((string-match "^:\\S +\\s +WALL" line) (irc-parse-wall line))
      ((string-match "^:\\S +\\s +QUIT" line) (irc-parse-quit line))
      ((string-match "^:\\S +\\s +KILL" line) (irc-parse-kill line))
      ((string-match "^:\\S +\\s +TOPIC" line) (irc-parse-topic line))
      ((string-match "^:\\S +\\s +3[0-9]+" line) (irc-parse-RPL line))
      ((string-match "^:\\S +\\s +4[0-9]+" line) (irc-parse-ERR line))
      ;; sigh.  just ^NOTICE was fine until someone used the protocol wrong
      ((string-match "^\\(: \\)?NOTICE" line) (irc-parse-notice line))
      ;; ditto!!  private messages should just be for messages between users!!
      ((string-match "^\\(:\\S *\\s +\\)?PRIVMSG" line) (irc-parse-priv line))
      ((string-match "^PING" line) (irc-pong))
      ((string-match "^ERROR" line) (irc-parse-error line))
      ((string-match "^WHOREPLY" line) (irc-parse-whoreply line))
      ((string-match "^NAMREPLY" line) (irc-parse-namreply line))
      ((string-match "^LINREPLY" line) (irc-parse-linreply line))
      (t (irc-insert line)
         (irc-insert
          "(Please let tale@pawl.rpi.edu know about this; it might be a bug.)")
         nil)))))

(defun irc-parse-channel (str)
  "Examine a CHANNEL message from the IRC server.
CHANNEL indicates a user entering or leaving the channel which you are on.
If the user is not being ignored and 'join' is in irc-notifies, a message
is inserted indicating the change.  A message is always provided as
confirmation if the user is the irc-mode user.

This function returns t if a bell should be issued for the 'join' event,
nil otherwise."
  (let ((user (substring str 1 (string-match "\\s CHANNEL " str)))
        (channel (string-to-int (substring str (match-end 0)))))
    ;; make sure that user is in the wholist since we have to take
    ;; this sort of information where we can until Jarkko supports
    ;; global ENTER/QUIT messages (which he might not do ...)
    (irc-maintain-list 'irc-wholist user 'add)
    (if (string= user irc-nick)
        (progn
          (if (zerop channel)
              (irc-insert "You have left channel %d." irc-channel)
            (irc-insert "You are now a member of channel %d." channel))
          (setq irc-channel channel)
          nil)                  ; don't issue a bell for our own join
      (if (or (member-general user irc-ignores 'string=)
              (not (memq 'join irc-notifies))) ()
        (if (zerop channel)
            (irc-insert "*** %s has left channel %d ***" user irc-channel)
          (irc-insert "*** %s has joined channel %d ***" user channel))
        (irc-signal user 'join))))) ; check for signal for join

(defun irc-parse-invite (str)
  "Examine an INVITE message from the IRC server.
INVITE is sent when one user invites another to a channel.
If the inviter is not being ignored a message is inserted in the buffer.

This function returns t if a bell should be issued for the 'invite' event,
nil otherwise."
  (let ((user (substring str 1 (string-match "\\s +INVITE " str)))
        (to (substring str (match-end 0)
                       (string-match "\\s +" str (match-end 0))))
        (channel (substring str (match-end 0))))
    ;; glom a new name, if necessary
    (irc-maintain-list 'irc-wholist user 'add)
    (if (member-general user irc-ignores 'string=)
        (irc-send (concat "PRIVMSG " user " :You are being ignored."))
      (irc-insert "*** %s invites %s to join channel %s ***" user
                  ;; i wish the downcases weren't necessary, but the servers
                  ;; are inconsistent.  anyway, this should return "you" 99%
                  ;; of the time; if it doesn't something weird is happening.
                  (if (string= (downcase to) (downcase irc-nick)) "you" to)
                  channel)
      (irc-signal user 'invite))))

(defun irc-parse-public (str)
  "Examine a MSG message from the IRC server.
MSG is sent when someone has sent a message to a channel.  In reality,
sometimes PRIVMSG is used but irc-parse-private should hand those off to
to here.

This function returns t if a bell should be issued for the 'public' event,
nil otherwise."
  (let ((user (substring str 1 (string-match "\\s MSG :" str)))
        (msg (substring str (match-end 0)))
        (time (if (and irc-message-stamp (not (eq 'private irc-message-stamp)))
                  (concat " (" (irc-get-time) ") ")
                " ")))
    ;; even here we can't guarantee that the sender has already been noted
    ;; someplace else like join or nick -- the sender might be someplace
    ;; else and sending to this channel with PRIVMSG.
    (irc-maintain-list 'irc-wholist user 'add)
    (if (member-general user irc-ignores 'string=) ()
      (if irc-conserve-space
          (irc-insert-message (concat "<" user ">" time msg) t)
        (irc-insert "\n ->%sFrom %s to %d:" time user irc-channel)
        (irc-insert-message msg))
      (irc-signal user 'public))))

(defun irc-parse-priv (str)
  "Examine a PRIVMSG message from the IRC server.
PRIVMSG is intended to be used for private message sent between users.
This is not always the case at the moment; servers will use it like either
NOTICE or MSG on occasion.

If it really is a private message, this function returns t if a signal should
be issued for the 'private' event, nil otherwise."
  ;; This is really gross because it kludges in the fact that PRIVMSG can
  ;; be used to send notification of a change of channel topic.  Actually,
  ;; topic changes are handled poorly all around by the servers because
  ;; only the person who changed the topic gets notification.
  ;; Also have to kludge in the fact that TIME to a remote host gives back
  ;; a PRIVMSG with no sender but with a leading :.  ARGHGHGHG!!
  (let (from to msg time)
    (if (string-match "^:\\S +\\s +PRIVMSG\\s +" str)
        ;; there was a sender.  this is a real private message.
        (setq from (substring str 1 (string-match "\\s +PRIVMSG\\s +" str))
              to (substring str (match-end 0)
                            (string-match "\\s +:" str (match-end 0))))
      (setq from nil          ; no sender.  schade.  broken protocol.
            to (substring str 9 (string-match "\\s :" str))))
    (setq msg (substring str (match-end 0)))
    (if (not from)
        ;; not really a private message.  whatever it was just show it
        ;; and don't worry about signalling it.
        (progn (irc-insert msg) nil)
      (if (and (= (string-to-int to) irc-channel) (not (zerop irc-channel)))
          ;; whoops.  more brain-damage.  this really should be indicated
          ;; as a public message since it went to the channel.
          (irc-parse-public (concat ":" from " MSG :" msg))
        ;; sigh.  this function gets called too much.
        (irc-maintain-list 'irc-wholist from 'add)
        ;; skip the messages if sender is being ignored
        (if (member-general from irc-ignores 'string=)
            ;; a meager check to avoid infinite looping.  noticed this when
            ;; someone ignored himself but it could happen between 2 different
            ;; people using the client.  there should be some better form
            ;; of detection of looping probably but for now this seems
            ;; sufficient.
            (or (string= msg "You are being ignored.")
                (irc-send (concat "PRIVMSG " from " :You are being ignored.")))
          (setq irc-last-private (concat from ":")
                time (if (and irc-message-stamp
                              (not (eq 'public irc-message-stamp)))
                         (concat " (" (irc-get-time) ") ")
                       " "))
          (or irc-conserve-space
              (irc-insert "\n >>%sPrivate message from %s:" time from))
          (or (string= (downcase to) (downcase irc-nick))
              ;; this should never show up.  if it does something is broken.
              (irc-insert " (apparently to %s)" to))
          (if irc-conserve-space
              (irc-insert-message (concat "*" from "*" time msg) t)
            (irc-insert-message msg))
          (irc-signal from 'private))))))

(defun irc-parse-quit (str)
  "Examine a QUIT message from the IRC server.
QUIT is used to tell of a user's departure from IRC.  It is currently sent
by the servers to those clients which are on the same channel as the
departing user.

This function returns t if a signal should be issued for the 'join' event,
since it also signals someone leaving the channel.  It returns nil if no
bell should be issued."
  (let ((user (substring str 1 (string-match "\\s +QUIT" str))))
    (irc-maintain-list 'irc-wholist user 'remove)
    (if (member-general user irc-ignores 'string=) ()
      (irc-insert "*** %s has left IRC ***" user)
      ;; currently just the join event; some modification will need to be made
      ;; here when/if Jarkko has QUIT sent to everyone, not just the channel
      (irc-signal user 'join))))

(defun irc-parse-wall (str)
  "Examine a WALL message from the IRC server.
WALL is sent by IRC operators to everyone on IRC.  A WALL message will
always be displayed even if the sender is being ignored.

This function returns t if a signal should be issued for the 'wall' event,
nil otherwise."
  (let ((user (substring str 1 (string-match "\\s +WALL\\s +:" str)))
        (msg (substring str (match-end 0))))
    ;; sigh.  okay class, can anyone tell me why we're calling this yet again?
    (irc-maintain-list 'irc-wholist user 'add)
    (if irc-conserve-space
        (irc-insert-message (concat "#" user "# (" (irc-get-time) ") " msg) t)
      (irc-insert "\n ## Message from %s at %s to everyone:"
                  user (irc-get-time))
      (irc-insert-message msg))
    (irc-signal user 'wall)))

(defun irc-parse-nick (str)
  "Examine a NICK message from the IRC server.
NICK is sent when a user's nickname is changed, but it is only sent to the
people on the same channel as the user.  If the person changing names is
being ignored, this fact is tracked across the change.  If notification
is not enabled for 'nick' then no message is inserted.

This function returns t if a signal should be issued for the 'nick' event,
nil otherwise."
  (let ((old (substring str 1 (string-match "\\s NICK " str)))
        (new (substring str (match-end 0))))
    (irc-maintain-list 'irc-wholist old 'remove)
    (irc-maintain-list 'irc-wholist new 'add)
    (if (member-general old irc-ignores 'string=)
        ;; track the 
        (progn (irc-maintain-list 'irc-ignores old 'remove)
               (irc-maintain-list 'irc-ignores new 'add)
               nil)           ; no signal for ignored user
      (if (or (not (memq 'nick irc-notifies)) (string= new irc-nick)) () 
        (irc-insert "*** %s is now known as %s ***" old new)
        (irc-signal old 'user)))))

(defun irc-parse-error (str)
  "Examine an ERROR message from the IRC server.
ERROR is used when something bogus happens like an unparsable command
is issued to the server.  Usually this will not happen unless something
like /QUOTE is used.  This message is also used when a user attempts to
change to a name that already exists.

Returns nil; currently no signals are issued for an error."
  (string-match "\\s +:" str)
  (irc-insert (substring str (match-end 0)))
  (if (string-match
       "Nickname\\s +\\S *\\s +\\(is \\)?\\(already\\|not\\s +chan\\|in use\\)"
       str)
      (progn
        ;; either we couldn't change the current nickname
        (setq irc-nick (or (get 'irc-nick 'o-nick)
                           ;; or we never even had one
                           "NO NAME YET (/NICK to set one)"))
        (set-buffer-modified-p (buffer-modified-p))
        (irc-insert (if (get 'irc-nick 'o-nick)
                        "Hmmm ... looks like you're still \"%s\"."
                      "%s") irc-nick)))
  nil)

(defun irc-parse-notice (str)
  "Examine a NOTICE message from the IRC server.
NOTICE is the catch-all for IRC messages; if it can't be classified as
one of the other currently existing messages then the information is
sent as NOTICE.  This message is overused, even when it another could be
used instead.  For example, if an attempt is made to send to a nickname
which is not on IRC the error reply is sent via NOTICE.

No signal is issued for NOTICE because it is way too random with what it
means."
  (string-match "\\s +:" str)
  (let ((msg (substring str (match-end 0))))
    (irc-insert msg)
    (cond
     ((string-match "^\\*\\*\\* Error: No such nickname (\\(.+\\))$" msg)
      ;; oops.  we sent to someone who wasn't really there.
      (irc-maintain-list 'irc-wholist
                         (substring msg (match-beginning 1) (match-end 1))
                         'remove))
     ((string-match "^Good afternoon, gentleman\\. I am a HAL 9000" msg)
      ;; we've been granted operator priviledges.  the string is for mode-line
      (setq irc-operator " Operator")
      (set-buffer-modified-p (buffer-modified-p)))))
  nil)

(defun irc-parse-topic (str)
  "Examine a TOPIC message from the IRC server.
TOPIC is sent to all of the users on a channel when someone changes the
topic of the channel.  Secret channels can not have the topic set.  TOPIC
messages are displayed as long as 'topic' is in irc-notifies, even if the user
changing the topic is being ignored.

This function returns t if a signal should be issued for the 'topic' event,
nil otherwise."
  (let ((user (substring str 1 (string-match "\\s +TOPIC\\s +" str)))
        (topic (substring str (match-end 0))))
    (irc-maintain-list 'irc-wholist user 'add)
    (if (not (memq 'topic irc-notifies)) ()
      (irc-insert "*** %s has changed the topic of %d to \"%s\" ***"
                  user irc-channel topic)
      (irc-signal user 'topic))))

(defun irc-parse-kill (str)
  "Examine a KILL message from the IRC server.
For a client this means its connexion will be closing momentarily.  This rather
drastic turn of events will always get a signal so this function returns t."
  (let ((oper (substring str 1 (string-match "\\s +KILL\\s +" str)))
        (user (substring str (match-end 0)
                         (string-match "\\s +:" str (match-end 0))))
        (path (substring str (match-end 0))))
    ;; probably a waste, but i think wisner will have KILL messages always
    ;; being shown to operatiors.
    (irc-maintain-list 'irc-wholist oper 'add)
    (irc-maintain-list 'irc-wholist user 'remove)
    (if (string= (downcase user) (downcase irc-nick))
        (irc-insert "*** Your session has been killed by %s via path %s ***"
                    oper path)
      (irc-insert "*** %s has killed %s via path %s ***" oper user path))
    ;; ring the bloody bell.
    t))

(defun irc-parse-whoreply (str)
  "Examine a WHOREPLY message from the IRC server.
The message is formatted into a line that is more easily understood than
the raw data.  People who have marked themselves as AWAY are preceded by a
'-' (provided the AWAY message has propogated to this client's server);
users being ignored are marked by a '#'; IRC Operators are shown with a '*';
and IRC Operators who are away are shown as '='.  '#' has priority over
the others because if a user is being ignored the other information
about that user's status is not really relevant.

No signals are issued for lines from the WHOREPLY."
  (string-match "^WHOREPLY\\s +" str)
  (setq str (substring str (match-end 0)))
  (let (split) ; make this a list of strings of each data item.
    ;; the elements of 'split' are:
    ;; 0 - full name     2 - nickname     4 - hostname      6 - channel
    ;; 1 - status        3 - server       5 - login name
    (while (not (string-match "^:" str))
      (setq split (cons (substring str 0 (string-match "\\(\\s +\\|$\\)" str))
                        split)
            str (substring str (match-end 0))))
    (setq split (cons str split))
    (if (string= (nth 1 split) "S") ()
        ;; if it isn't the bogus header, add nick
        (irc-maintain-list 'irc-wholist (nth 2 split) 'add))
    (irc-insert (concat
                 (if (member-general (nth 2 split) irc-ignores 'string=) "#"
                   (cond ((string= "H"  (nth 1 split)) " ")
                         ((string= "H*" (nth 1 split)) "*")
                         ((string= "G"  (nth 1 split)) "-")
                         ((string= "G*" (nth 1 split)) "=")
                         ((string= "S"  (nth 1 split)) " ")
                         (t (nth 1 split)))) ; no clue what it is; just use it
                 (nth 2 split)
                 ;; pad some spaces in
                 (make-string (- 10 (length (nth 2 split))) 32)
                 (format "%4s " (if (zerop (string-to-int (nth 6 split)))
                                    ;; bogus header; translate * to "Chan"
                                    (if (string= "*" (nth 6 split)) "Chan" "")
                                  (nth 6 split)))
                 (nth 5 split) "@" (nth 4 split) " ("
                 (substring (car split) 1) ")")))
  nil)

(defun irc-parse-linreply (str)
  "Examine a LINREPLY message from the IRC server.
LINREPLY is used to answer a LINKS request to show all the servers on line.
\"Links\" is a bit of a misnomer since little information regarding the
actual structure of the IRCnet can be gained from these messages.

No signals are issued for lines from the LINREPLY."
  (string-match "^LINREPLY\\s +\\(\\S +\\)\\s +" str)
  (irc-insert "Server: %s (%s)"
              (substring str (match-beginning 1) (match-end 1))
              (substring str (match-end 0)))
  nil)

(defun irc-parse-namreply (str)
  "Examine a NAMREPLY message from the IRC server.
NAMREPLY is used in repsonse to NAMES to indicate what users are on what
channels.  All users on secret or private channels which the client is not
on are grouped together on one private channel.

No signals are issued for NAMREPLYs."
  (string-match "^NAMREPLY\\s +\\S +\\s +\\(\\S +\\)\\s +" str)
  (let* ((channel (substring str (match-beginning 1) (match-end 1)))
         (users (substring str (match-end 0)))
         (to-insert (format "%7s "
                            (if (string= "*" channel) "Private" channel)))
         nick)
    ;; yet another source of information for irc-wholist.
    (while (string-match "^\\(\\S +\\)\\(\\s \\|$\\)" users)
      (setq nick (substring users 0 (match-end 1))
            users (substring users (match-end 0)))
      (irc-maintain-list 'irc-wholist nick 'add)
      ;; parsing by name also means we can format a long line nicely
      ;; question: why do programmers so frequently use "we" in comments?
      (if (<= (+ (length to-insert) (length nick)) (- (window-width) 2))
          (setq to-insert (concat to-insert " " nick))
        (irc-insert to-insert)
        (setq to-insert (format "         %s" nick))))
    (irc-insert to-insert))
  nil)

(defun irc-parse-ERR (str)
  "Examine a numeric ERR_ message from the IRC server.
Numeric control messages are used by newer servers to aid in generalized
client design; while people are converting to the new servers the older
irc-parse-error, irc-parse-notice, et al, functions are redundant with
irc-parse-ERR and irc-parse-RPL.  Values used by this function are found
in the IRC source file numeric.h.

Note well that some things are still going to come out wrong because the
servers are currently still doing things inconsistently."
  (string-match "^\\S +\\s +\\(4[0-9][0-9]\\)\\s +\\S +\\s +\\(.*\\)$" str)
  ;; we assume that the server and message are consistent for us; just
  ;; worry about the numeric value and the rest of the line
  (let ((num (string-to-int (substring str (match-beginning 1) (match-end 1))))
        (txt                (substring str (match-beginning 2) (match-end 2)))
        tmp1)
    (cond
     ((= num 401)                       ; ERR_NOSUCHNICK
      (string-match "^\\S +" txt)
      (setq tmp1 (substring txt (match-beginning 0) (match-end 0)))
      (irc-maintain-list 'irc-wholist tmp1 'remove)
      (irc-insert "'%s' is not on IRC." tmp1))
     ((= num 402)                       ; ERR_NOSUCHSERVER
      (string-match "^.*\\s :" txt)
      (irc-insert "'%s' is not a server on the IRCnet now."
                  (substring txt (match-beginning 0) (- (match-end 0) 2))))
     ((= num 403)                       ; ERR_NOSUCHCHANNEL
      (string-match "^\\S +" txt)
      (irc-insert "Channel %s is not in use."
                  (substring txt (match-beginning 0) (match-end 0))))
     ((= num 411)                       ; ERR_NORECIPIENT
      (irc-insert "The last message had no recipient."))
     ((= num 412)                       ; ERR_NOTEXTTOSEND
      (irc-insert "The last message had no text to send."))
     ((= num 421)                       ; ERR_UNKNOWNCOMMAND
      (string-match "^\\(.*\\) Unknown command$" txt)
      (irc-insert "Unknown server command: '%s'."
                  (substring txt (match-beginning 1) (match-end 1))))
     ((= num 431)                       ; ERR_NONICKNAMEGIVEN
      (irc-insert "No nickname give to change to."))
     ((= num 432)                       ; ERR_ERRONEUSNICKNAME
      (irc-insert "Bad format for nickname change."))
     ((= num 433)                       ; ERR_NICKNAMEINUSE
      (string-match "^\\S + 433 \\(\\S *\\) \\(\\S +\\) " str)
      (irc-insert "Nickname '%s' is already being used; please choose another."
                  (substring str (match-beginning 2) (match-end 2)))
      ;; either we couldn't change the current nickname
      (setq irc-nick (if (/= (match-beginning 1) (match-end 1))
                         (get 'irc-nick 'o-nick)
                       ;; or we never even had one
                       "NO NAME YET (/NICK to set one)"))
      (set-buffer-modified-p (buffer-modified-p))
      (irc-insert (if (= (match-beginning 1) (match-end 1)) "%s"
                    "Hmmm ... looks like you're still \"%s\".") irc-nick))
     ((= num 441)                       ; ERR_USERNOTINCHANNEL
      (irc-insert "You're not on any channel."))
     ((= num 451)                       ; ERR_NOTREGISTERED
      (irc-insert "You haven't checked in yet.  Choose a nickname."))
     ((= num 461)                       ; ERR_NEEDMOREPARAMS
      (irc-insert "There weren't enough arguments for the last command."))
     ((= num 462)                       ; ERR_ALREADYREGISTRED
      (irc-insert "You've already registered."))
     ((= num 463)                       ; ERR_NOPERMFORHOST
      (irc-insert "Your host isn't permitted."))
     ((= num 464)                       ; ERR_PASSWDMISMATCH
      (irc-insert "That password is incorrect."))
     ((= num 465)                       ; ERR_YOUREBANNEDCREEP
      (irc-insert "You've been banned from IRC."))
     ((= num 471)                       ; ERR_CHANNELISFULL
      (string-match "^\\S +" txt)
      (irc-insert "Channel %s is full."
                  (substring txt (match-beginning 0) (match-end 0))))
     ((= num 481)                       ; ERR_NOPRIVILEGES
      (irc-insert "You must be an IRC Operator to do that."))
     ((= num 482)                       ; ERR_NOOPERHOST
      (irc-insert "You can't be that IRC Operator from your host."))
     (t                                 ; default
      (irc-insert "Unrecognized numeric ERR message follows; please tell tale@pawl.rpi.edu:")
      (irc-insert str))))
  nil) ; no need for a bell, I suppose.

(defun irc-parse-RPL (str)
  "Examine a numeric RPL_ message from the IRC server.
Numeric control messages are used by newer servers to aid in generalized
client design; while people are converting to the new servers the older
irc-parse-error, irc-parse-notice, et al, functions are redundant with
irc-parse-ERR and irc-parse-RPL.  Values used by this function are found
in the IRC source file numeric.h.

Note well that some things are still going to come out wrong because the
servers are currently still doing things inconsistently."
  (string-match "^\\S +\\s +\\(3[0-9][0-9]\\)\\s +\\S +\\s +\\(.*\\)$" str)
  ;; we assume that the server and message are consistent for us; just
  ;; worry about the numeric value and the rest of the line
  (let ((num (string-to-int (substring str (match-beginning 1) (match-end 1))))
        (txt                (substring str (match-beginning 2) (match-end 2)))
        tmp1 tmp2 tmp3 tmp4)
    (cond
     ((= num 301)                       ; RPL_AWAY
      (string-match "^\\(\\S +\\) :\\(.*\\)$" txt)
      (irc-insert "%s is away, \"%s\"."
                  (substring txt (match-beginning 1) (match-end 1))
                  (substring txt (match-beginning 2) (match-end 2))))
     ((= num 311)                       ; RPL_WHOISUSER
      (string-match
       "^\\(\\S +\\) \\(\\S +\\) \\(\\S +\\) \\(\\S +\\) :\\(.*\\)$" txt)
      (irc-insert "%s is %s <%s@%s> on %s."
                  (substring txt (match-beginning 1) (match-end 1))
                  (substring txt (match-beginning 5) (match-end 5))
                  (substring txt (match-beginning 2) (match-end 2))
                  (substring txt (match-beginning 3) (match-end 3))
                  (if (string= (setq tmp1 (substring txt (match-beginning 4)
                                                     (match-end 4))) "*")
                      "a private channel"
                    (concat "channel " tmp1))))
     ((= num 312)                       ; RPL_WHOISSERVER
      (string-match "^\\(\\S +\\) :?\\(.*\\)$" txt)
      (setq tmp1 (substring txt (match-beginning 1) (match-end 1))
            tmp2 (substring txt (match-beginning 1) (match-end 1)))
      (or (and (string= tmp1 "*") (string= tmp2 "*"))
          ;; (irc-insert "On via server %s (%s)." tmp1 tmp2)
          ;; bizarre.  tmp2 appears to always equal tmp1
          (irc-insert "On via server %s." tmp1)))
     ((= num 313)                       ; RPL_WHOISOPERATOR
      (string-match "^\\S +" txt)
      (irc-insert "%s is an IRC Operator."
                  (substring txt (match-beginning 0) (match-end 0))))
     ((= num 321)                       ; RPL_LISTSTART
      (irc-insert "Channel  Users Topic"))
     ((= num 322)                       ; RPL_LIST
      (string-match "^\\(\\S +\\) \\(\\S +\\) :\\(.*\\)$" txt)
      (setq tmp1 (substring txt (match-beginning 1) (match-end 1)))
      (irc-insert "%7s   %2s   %s"
                  (if (string= tmp1 "*") "Private" tmp1)
                  (substring txt (match-beginning 2) (match-end 2))
                  (substring txt (match-beginning 3) (match-end 3))))
     ((= num 323)                       ; RPL_LISTEND
      (or irc-conserve-space (irc-insert "\n")))
     ((= num 331)                       ; RPL_NOTOPIC
      (irc-insert "No topic is set."))
     ((= num 332)                       ; RPL_TOPIC
      (string-match "^:\\(.*\\)$" txt)
      (irc-insert "The topic is %s."
                  (substring txt (match-beginning 1) (match-end 1))))
     ((= num 341)                       ; RPL_INVITING
      (string-match "^:\\(\\S +\\) 341 \\S + \\(\\S +\\) \\(\\S +\\)" str)
      (irc-insert "Server %s inviting %s to channel %s"
                  (substring str (match-beginning 1) (match-end 1))
                  (substring str (match-beginning 2) (match-end 2))
                  (substring str (match-beginning 3) (match-end 3))))
     ((= num 351)                       ; RPL_VERSION
      (string-match "^\\(\\S +\\) \\(\\S +\\)" txt)
      (irc-insert "%s is running IRC daemon version %s"
                  (substring txt (match-beginning 2) (match-end 2))
                  (substring txt (match-beginning 1) (match-end 1))))
     ((= num 361)                       ; RPL_KILLDONE
      (string-match "^\\S +" txt)
      (irc-insert "%s has been removed from IRC."
                  (substring txt (match-beginning 0) (match-end 0))))
     ((= num 371)                       ; RPL_INFO
      (irc-insert (substring txt 1)))
     ((= num 372)                       ; RPL_MOTD
      (irc-insert (substring txt 1)))
     ((= num 381)                       ; RPL_YOUREOPER
      (setq irc-operator " Operator")
      (set-buffer-modified-p (buffer-modified-p))
      (irc-insert "Welcome to the Twilight Zone."))
     ((= num 382)                       ; RPL_REHASHING
      (irc-insert "Rereading local ircd configuration information."))
     ((= num 391)                       ; RPL_TIME
      (string-match "^\\(\\S +\\) :\\(.*\\)$" txt)
      (irc-insert "It is %s at %s."
                  (substring txt (match-beginning 2) (match-end 2))
                  (substring txt (match-beginning 1) (match-end 1))))
     (t                                 ; default
      (irc-insert "Unrecognized numeric RPL message follows; please tell tale@pawl.rpi.edu:")
      (irc-insert str))))
  nil) ; no bell rung

(defun irc-pong ()
  "Send a PONG message with the hostname as an argument.
This is usually used to answer a PING message."
  ;; it's interactive so it can be bound during testing.
  (interactive) (irc-send (concat "PONG " (system-name))) nil)

(defun irc-insert-message (msg &optional pure-first)
  "Format MSG by word-wrapping into 5 characters less than the window-width
or less. If a word is too long to be split this way then it is broken at
the last character which would fit on the line and continued on the next line
as if white space had been there.  Each line is prefixed with the string
\" - \" and passed to irc-insert for the actual insertion into the buffer."
  (let (line (lines 0))
    (while (> (length msg) (- (window-width) 5))
      (setq lines (1+ lines)
            line (substring msg 0 (- (window-width) 4))
            msg (substring msg (- (window-width) 4))
            line (irc-fix-wordwrap line msg)
            msg (cdr line)
            line (car line))
      (irc-insert (concat (if (not (and pure-first (= lines 1))) " - ") line)))
    (irc-insert (concat (if (not (and pure-first (zerop lines))) " - ") msg))))

(defun irc-insert (format &rest args)
  "Insert before irc-mark the string created by FORMAT with substituted ARGS.
Word-wrapping is done to make lines of length less than or equal to one
character less than the window-width.  If a word is too long to be wrapped it
is cut at the last column on the line as though white space were there."
  (let ((str (apply 'format format args)) line)
    (save-excursion
      (goto-char irc-mark)
      (while (> (length str) (1- (window-width)))
        (setq line (substring str 0 (1- (window-width)))
              str  (substring str (1- (window-width)))
              line (irc-fix-wordwrap line str)
              str  (concat "    " (cdr line))
              line (car line))
        (insert-before-markers (concat line "\n")))
      (insert-before-markers (concat str "\n")))))

(defun irc-fix-wordwrap (line1 line2)
  "With arguments LINE1 and LINE2 apply some simple heuristics to see if the
line which they originally formed was broken in an acceptable place.  Returns
a dotted pair with LINE1 as the car and LINE2 as the cdr."
  (cond ((string-match "^\\s +" line2)
         ;; broke at whitespace; strip leading space from next line
         (setq line2 (substring line2 1)))
        ((string-match "\\s +$" line1)
         ;; trailing whitespace on line.  might as well just nuke it all.
         (setq line1 (substring line1 0 (match-beginning 0))))
        ((string-match "\\(\\s +\\)\\S +$" line1)
         ;; broke in a word, but it's wrappable.  just eat one space.
         (setq line2 (concat (substring line1 (1+ (match-beginning 1))) line2)
               line1 (substring line1 0 (match-beginning 0)))))
  (cons line1 line2))

;; simple key functions -- self-insert, tab, destructive backspace
(defun irc-self-insert (arg)
  "Normally just inserts the typed character in the input region.
If point is in the output region, irc-spacebar-pages is non-nil and a space
is typed, scroll-up (aka window-forward) otherwise point moves to end of input
region and inserts the character.

If the character to be inserted is a colon or semi-colon and it is the first
non-white space charavter on the line then the input region is updated to
begin with the last explicit sendlist, irc-last-explicit.

Inserts the character ARG times if self-inserting.  An argument is not
passed to scroll-up if paging with the spacebar."
  (interactive "p")
  (let* ((in-region (>= (point) irc-mark))
         ;; it's times like this that i wish someone would tell me what
         ;; a good indentation style is for this expression
         (expand-colon
          (and
           (or (= last-input-char ?:) (= last-input-char ?\;))
           (string-match
            "^\\s *$"
            (buffer-substring irc-mark (if in-region (point) (point-max)))))))
    (if (not expand-colon)
        (if in-region (self-insert-command arg)
          (if (and irc-spacebar-pages (= last-input-char 32))
              ;; it's nice to be able to return to the input region just by
              ;; pounding on the spacebar repeatedly.
              (condition-case EOB (scroll-up nil)
                (end-of-buffer (goto-char (point-max))))
            (goto-char (point-max))
            (self-insert-command arg)))
      (or in-region (goto-char (point-max)))
      ;; kill white space.  This also takes out previous lines in input region.
      (delete-region irc-mark (point))
      (insert (if (= last-input-char ?:) irc-last-private irc-last-explicit))
      ;; put in the extra characters if need be.
      (if (> arg 1) (self-insert-command (1- arg))))))

(defun irc-del-backward-char (arg)
  "If in the input region, delete ARG characters before point, restricting
deletion to the input region.  If in the output region and irc-spacebar-pages
then scroll-down (aka window-back) otherwise do nothing."
  (interactive "p")
  (if (> (point) irc-mark)
      ;; only delete as far back as irc-mark at most
      (if (> arg (- (point) irc-mark)) (delete-region (point) irc-mark)
        (delete-backward-char arg))
    (if (and (< (point) irc-mark) irc-spacebar-pages) (scroll-down nil)
      (ding))))

(defun irc-tab ()
  "If point is in the input region then tab-to-tab-stop.  If it is in the
output region, go to the previous line if irc-spacebar-pages; do nothing
otherwise."
  (interactive)
  (if (>= (point) irc-mark) (tab-to-tab-stop)
    (if irc-spacebar-pages (previous-line 1)
      (ding))))

;; top-level -- entry, sentinel and mode
(defun irc (new-buffer)
  "Enter the Internet Relay Chat conferencing system.
If no connexion to an irc-server is open, then one is started.  If no buffer
*IRC* exists then it is created otherwise the existing buffer is used.  If
a connexion is already active then the most recently started IRC session
is switched to in the current window.  This makes binding 'irc' to a key
much more convenient.

With prefix argument NEW-BUFFER, another *IRC* buffer is created and a
new IRC session is started.  This is provided so that multiple IRC
sessions can co-exist in one Emacs, which is sometimes a useful thing."
  (interactive "P")
  (let ((buffer (if new-buffer (generate-new-buffer "*IRC*")
                  (get-buffer-create "*IRC*")))
        proc)
    (if (and (not new-buffer) irc-processes)
        ;; just head for the most recent session
        (switch-to-buffer (process-buffer (car irc-processes)))
      (switch-to-buffer buffer)
      (goto-char (point-max))
      (insert "IRC-mode for GNU Emacs -- comments to tale@pawl.rpi.edu."
              "  C-c n for news.\n\n")
      (irc-mode)
      (condition-case NOT-IRCED 
          (progn
            (setq proc (open-network-stream "irc" buffer irc-server irc-port))
            (set-process-filter proc 'irc-filter)
            (set-process-sentinel proc 'irc-sentinel)
            (irc-send (format "USER %s %s %s %s" (user-login-name)
                              (system-name) irc-server (user-full-name)))
            (irc-send (concat "NICK " irc-nick))
            ;; a new process, so initialize the variables.  they aren't set
            ;; in irc-mode so that irc-mode can be called at any time.
            (setq irc-away     nil    irc-channel 0     irc-history-index -1
                  irc-operator nil    irc-scratch ""    irc-last-command  ""
                  irc-last-explicit "*;" irc-last-private "*;"
                  irc-processes (cons proc irc-processes)
                  irc-last-time (irc-get-time)
                  irc-total-time (string-to-int (substring irc-last-time 3))
                  ;; this next bit of messiness just ups irc-last-stamp
                  ;; in an effort to make nice numbers out of the time
                  ;; stamps -- ie, if the time is now 13:53 with an interval
                  ;; of 15 minutes, this makes it 13:45
                  irc-last-stamp 0
                  irc-last-stamp (if (zerop irc-time-stamp) 0
                                   (while (< (+ irc-last-stamp irc-time-stamp)
                                             irc-total-time)
                                     (setq irc-last-stamp (+ irc-last-stamp
                                                             irc-time-stamp)))
                                   irc-last-stamp)))
        (error (irc-insert "Sorry ... couldn't connect to %s at %s.\n\n"
                           irc-port irc-server))))))

(defun irc-mode ()
  "To understand some documentation given with irc-mode variables and
functions, \"output region\" is defined as everything before the irc-mark.
irc-mark is a marker kept by irc-mode to know where to insert new text
from IRC.  Text in the output region cannot be modified by the most common
methods of typing a self-inserting character or pressing delete.

The input region is everything which follows irc-mark.  It is what
gets processed by irc-mode when you type LFD or RET.  If irc-spacebar-pages
is non-nil, the following keys are in effect when the cursor is in the
output region:

SPC             scroll-forward       DEL     scroll-backward
LFD or RET      next-line            TAB     previous-line

Local keys:
\\{irc-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (setq major-mode 'irc-mode mode-name "IRC" fill-column (- (window-width) 5))
  (make-local-variable 'irc-away)          ; for the mode-line 
  (make-local-variable 'irc-channel)       ; for sendlists and broken PRIVMSGs
  (make-local-variable 'irc-wholist)       ; for sendlists
  (make-local-variable 'irc-operator)      ; for special priviledges
  (make-local-variable 'irc-history-index) ; for the message history
  (make-local-variable 'irc-scratch)       ; for accumulating server messages
  (make-local-variable 'irc-last-command)  ; for the command history
  (make-local-variable 'irc-last-explicit) ; for sendlist ; auto-expansion
  (make-local-variable 'irc-last-private)  ; for sendlist : auto-expansion
  (make-local-variable 'irc-last-stamp)    ; for time-sentinel
  (make-local-variable 'irc-last-time)     ; ditto
  (make-local-variable 'irc-total-time)    ; here too
  ;; too many ways to get unbalanced parens (most notably ":-)")
  (set (make-local-variable 'blink-matching-paren) nil)
  ;; closest we can come to "natural" terminal scrolling
  (set (make-local-variable 'scroll-step) 1)
  (set (make-local-variable 'mode-line-format)
       (list (purecopy "--- %14b") 'global-mode-string
             (purecopy "  %[(") 'mode-name 'irc-operator ;  'minor-mode-alist
             (purecopy ")%]---") 'irc-nick 'irc-away (purecopy "-%-")))
  (set-marker (set (make-local-variable 'irc-mark) (make-marker)) (point-max))
  (buffer-enable-undo)
  (irc-wrap-display-time)
  (turn-on-auto-fill)
  ;; "invisible subwindows" or whatever you would like to call them would be
  ;; nice.  That way I could make the output-region read-only.  The two things
  ;; most likely to screw up the buffer are backward-kill-word and kill-region
  (use-local-map irc-mode-map)
  (run-hooks 'irc-mode-hook))

(defun irc-sentinel (proc stat)
  "The sentinel for the IRC connexion.
Takes the normal sentinel arguments PROCESS and STATUS."
  ;; ignore anything but finished; i don't know what to do with the others
  (cond ((string= stat "finished\n")
         (save-excursion
           (set-buffer (process-buffer proc))
           (goto-char (point-max))
           (irc-insert "\nIRC session finished at %s.\n" (irc-get-time)))
         ;; all that needs to be done is a little maintenance ...
         (setq irc-processes (delq proc irc-processes)))))

;; processing input
(defun irc-process-input ()
  "If in the input region, parse it for messages and commands.
In the output region, next-line if irc-spacebar-pages, otherwise do nothing.

All of the lines in the input region are rejoined during processing to be
handled as one.  A command is any line starting with a / after leading
whitespace is stripped away; commands can not exceed 250 characters.  Messages
can be longer but they will be split into 250 character segments for IRC.  The
buffer will reflect how the message was sent if it needed to be broken; the
split(s) will be indicated by \" >>\" to mean that the message is continued."
  (interactive)
  ;; do the simple stuff for the output region
  (if (< (point) irc-mark) (if irc-spacebar-pages (next-line 1) (ding))
    (irc-check-time)
    ;; the input region is more work ...
    ;; first, toast extraneous spaces, tabs and newlines at end of input region
    (delete-region (goto-char (point-max))
                   (if (re-search-backward "[^ \t\n]" irc-mark t)
                       (1+ (point)) (point)))
    ;; nuke the white space at the beginning of input region, too
    (delete-region (goto-char irc-mark)
                   (progn (re-search-forward "\\s *") (point)))
    (setq irc-history-index -1)            ; reset the history scroll location
    (let ((txt (buffer-substring irc-mark (point-max))) send ass)
      ;; check to see if the input region is empty
      (if (string= "" txt) (message "(nothing sent to the irc-server)")
        (while (string-match "\n" txt)
          (aset txt (match-beginning 0) 32))
        (if (string-match "^/" txt)  ; it's a command
            (if (< (length txt) 250)
                (progn
                  (goto-char (point-max)) (insert "\n")
                  (set-marker irc-mark (point))
                  (irc-execute-command (setq irc-last-command
                                             (substring txt 1))))
              ;; can't use error because that kills the function
              (ding) (message "IRC commands can't exceed 250 characters."))
          ;; "a specified sendlist" -- was there one?
          (setq ass (irc-find-to txt 'explicit))
          (if (and ass (string-match "^[^:;]" txt))
              ;; a real sendlist was specified -- update irc-last-explicit
              (setq irc-last-explicit (irc-find-to txt)))
          (irc-add-to-hist (concat (if (not ass) irc-default-to)
                                   (buffer-substring irc-mark (point-max))))
          (while (> (length txt) 250)
            (setq send (substring txt 0 250)
                  txt  (substring txt 250)
                  send (irc-fix-wordwrap send txt)
                  txt  (concat (if ass irc-last-explicit irc-default-to)
                               (cdr send))
                  send (concat (car send) " >>"))
            (goto-char (+ irc-mark (- (length send) 3)))
            (insert " >>\n" (if ass irc-last-explicit irc-default-to))
            (if (looking-at "[ \t]") (delete-char 1))
            (beginning-of-line)
            (set-marker irc-mark (point))
            (irc-execute-msg send))
          (goto-char (point-max)) (insert "\n")
          (set-marker irc-mark (point))
          (irc-execute-msg txt))))))

(defun irc-execute-command (str)
  "Execute the \"/\" command of STR.  STR should not begin with a slash.
Commands are first looked up in the irc-alias-alist; if it is found there
then the alias gets passed recursively with any arguments the original
had.  The irc-command-alist is checked next and finally the irc-operator-alist.
A command is considered \"found\" when it matches either exactly or
unambiguously starting at the first character.  That is, J would match JOIN,
but OIN would not."
  (let* ((case-fold-search t)
         (cmd (substring str 0 (string-match "\\(\\s +\\|$\\)" str)))
         (text (substring str (match-end 0)))
         (ambig (irc-check-list
                 (mapcar 'car (append irc-alias-alist irc-command-alist
                                      (if irc-operator irc-operator-alist)))
                 cmd 'start-only))
         matches)
    ;; if no matches are found the command might still be a valid command
    ;; name hiding behind non-operator status.  i don't like messages that
    ;; lie and say "Unknown command '/REHASH'" so this should make it not lie.
    (if (and (not irc-operator) (null ambig))
        (setq ambig (irc-check-list (mapcar 'car irc-operator-alist) cmd t)))
    ;; first determine any ambiguities among the lists
    (if (null ambig)
        ;; no matches at all were found
        (irc-insert "Unknown command '/%s'.  Type /HELP for help."
                    (upcase cmd))
      ;; this is here for when a regular command gets aliased.  it shows up as
      ;; being ambiguous but it really isn't later on.
      (if (member-general (car ambig) (cdr ambig) 'string=)
          (setq ambig (cdr ambig)))
      (if (> (length ambig) 1)
          (irc-insert "Ambiguous command '/%s'.  Could be %s." (upcase cmd)
                      (irc-subst-comma
                       (mapconcat (function (lambda (arg)
                                              (concat "/" arg))) ambig ", ")
                       "or"))
        ;; alias list has highest priority
        (setq matches (irc-check-list (mapcar 'car irc-alias-alist) cmd t))
        ;; make sure matches is what we set out to looking for ...
        (if (and matches (string= (car matches) (car ambig)))
            ;; call this function again with the text as argument
            (irc-execute-command
             (concat (cdr (assoc (car matches) irc-alias-alist))
                     ;; the servers won't grok trailing whitespace for some
                     ;; messages so only use it to separate an argument
                     (if (string< "" text) " ") text))
          ;; next try the command alist
          (setq matches (irc-check-list (mapcar 'car irc-command-alist) cmd t))
          (if matches
              ;; call the appropriate irc-execute-* function
              (funcall (intern-soft
                        (concat "irc-execute-"
                                (cdr (assoc (car matches)
                                            irc-command-alist)))) text)
            ;; no matches yet.  last resort is the operator alist
            (setq matches (irc-check-list (mapcar 'car irc-operator-alist)
                                          cmd t))
            (if matches
                (if irc-operator
                    (funcall (intern-soft
                              (concat "irc-execute-"
                                      (cdr (assoc (car matches)
                                                  irc-operator-alist)))) text)
                  (irc-insert "Only IRC Operators can use the /%s command."
                              (upcase (car matches)))))))))))

(defun irc-send (str)
  "Send STR to process in the current buffer.
A CR-LFD pair is appended automatically as per the 'official' IRC protocol,
but it seems unnecessary.  Returns its argument STR."
  (send-string (get-buffer-process (current-buffer)) (concat str "\r\n"))
  str)

;; sending messages to people
(defun irc-execute-privmsg (str)
  "Usage: /MSG recipient(s) message

This command is provided simply for compatability with the C client.  It is
preferable instead to just type the name of the user followed by a semi-colon
or colon and then the message. That is, \"tale;hi!\" will send the message
\"hi!\" to the user with the nickname which unambiguously matches \"tale\".
A semi-colon or colon at the beginning of the line means to send to the last
recipient explicity specified; typing a semi-colon at the beginning of a line
expands it to the last recipient(s) specified while typing a colon at the
beginning of the line automatically expands to the last person to have sent
you a private message.  The recipients for a message can be a comma separated
list of users and/or channels."
  (irc-add-to-hist
   (irc-execute-msg (concat
                     (setq irc-last-explicit
                           (concat (substring
                                    str 0 (string-match "\\s +\\|$" str)) ";"))
                     (substring str (match-end 0))))))

(defun irc-execute-msg (str)
  "Send a message to a channel or another user.  Returns its argument STR,
munged slightly to indicate where it was attempted to be sent."
  ;; this really is an indirect fucntion of the UI (ie, not through a /COMMAND)
  ;; so it isn't interactive
  (let (tolist (orig str) icw confirm)
    (if (string-match "^[;:]" str)
        ;; a little bit of fill-in-the-blank
        (setq str (concat irc-last-explicit (substring str 1)))
      (if (not (irc-find-to str 'explicit))
          ;; prepend an implicit sendlist if need be
          (if irc-default-to (setq str (concat irc-default-to str))
            (irc-insert "You have no default sendlist."))))
    (if (irc-find-to str 'explicit)
        (setq icw (irc-find-to str)
              tolist (irc-burst-comma (substring icw 0 (1- (length icw))))
              str (irc-find-message str)
              ;; kill on leading space if it exists.  ie, "tale: hi" will
              ;; send "hi" as a message not " hi".
              str (if (= (aref str 0) 32) (substring str 1) str)))
    (setq
     confirm
     (delq                              ; whee.  lisp indentation is fun.
      nil
      (mapcar (function
               (lambda (to)
                 (if (not (zerop (string-to-int to)))
                     (if (= (string-to-int to) irc-channel)
                         (progn (irc-send (concat "MSG :" str)) to)
                       ;; new in 1.2 -- you _can_ send to a channel you
                       ;; are not on
                       (irc-send (concat "PRIVMSG " to " :" str))
                       to)
                   (setq icw (irc-check-list irc-wholist to))
                   (cond
                    ((string= to "*")
                     (if (zerop irc-channel)
                         (progn (irc-insert "You are not on any channel.") nil)
                       (irc-send (concat "MSG :" str))
                       (int-to-string irc-channel)))
                    ((string= to "0")
                     (irc-insert "You can't send to channel 0.") nil)
                    ((= (length icw) 1)
                     (irc-send (concat "PRIVMSG " (car icw) " :" str))
                     (car icw))
                    ((not icw)
                     ;; wox.  no one found, but we'll do a nonomatch.  try
                     ;; sending it anyway and let the server bitch if necessary
                     (irc-maintain-list 'irc-wholist to 'add)
                     (irc-send (concat "PRIVMSG " to " :" str))
                     to)
                    (t
                     (irc-insert "Ambiguous recipient \"%s\"; could be %s."
                                 to (irc-subst-comma
                                     (mapconcat 'eval icw ", ") "or")) nil)))))
              tolist)))
    (if (and confirm irc-confirm)
        (irc-insert "(message sent to %s)"
                    (irc-subst-comma (mapconcat 'eval confirm ", ") "and"))
      (if (not confirm) (irc-insert "(message not sent)")))
    orig))

(defun irc-execute-oops (newto)	; one of my favourites. 
  "Usage: /OOPS intended-recipient

Send irc-oops to recipient(s) of last message and resend message to
'intended-recipient'.  This command is handy when you've just sent a message
to the wrong place and you want the person/people who saw it to know that they
should just disregard it.  The message which was originally sent then gets
forwarded to its proper destination."
  (interactive)
  ;; first do the oops message
  (irc-execute-msg (concat (irc-find-to (car irc-history)) irc-oops))
  ;; then resend the original
  (irc-execute-redirect newto))

(defun irc-execute-redirect (newto)
  "Usage: /REDIRECT additional-recipient

Send to 'additional-recipient' the last message which you sent.  This 
command can be fairly easily duplicated using the history mechanism by hand
but it is provided to make it even easier."
  (interactive (list
                (read-string
                 (format "New recipient(s)? %s"
                         (if irc-default-to
                             (concat "[RET for "
                                     (substring irc-default-to 0
                                                (1- (length irc-default-to)))
                                     "] ")
                           "")))))
  (if (not (string-match "^[a-zA-Z0-9-_,|{*]*$" newto))
      ;; perhaps crapping out here is too harsh
      (irc-insert "%s is not a valid sendlist.  Message not redirected." newto)
    (if (and (not (interactive-p)) (string= "" newto))
        (call-interactively 'irc-execute-redirect)
      (setq newto (if (string= "" newto) irc-default-to (concat newto ";"))
            irc-last-explicit newto)
      (irc-add-to-hist
       (irc-execute-msg (concat newto
                                (irc-find-message (car irc-history))))))))

;; /commands for the server
(defun irc-execute-quote (msg)
  "Usage: /QUOTE string

This command is used to send 'string' directly to the IRC server without
any local processing.  Warning: this has the potential to screw up some
things in irc-mode, particularly if it is used to change your nickname or
to switch channels."
  (interactive "sString to send to server: ")
  (if (string-match "^\\s *$" msg)
      (irc-insert "(nothing was sent to the IRC server)")
    (irc-send msg)))

(defun irc-execute-who (channel)
  "Usage: /WHO [ channel | user ]

Get a list of the users on IRC.  Optional argument 'channel' means to show
just the users on that channel, with * representing the current channel.

Each user is indicated on a separate line with their nickname, channel, login
name, host and real name.  The first column indicates their status --
' ' for here, '-' for away, '*' for an operator, '=' for an away operator
and '#' for someone being ignored.  Servers don't propogate the information
about who is away so you will probably only see people on your server
correctly marked regarding their presence.

Users who are either on a channel greater than 1000 or who are on no channel
have nothing listed in the Chan column.  Users who are on channels less than
zero do not appear in the list.

If a non-numeric argument 'user' is given then it is taken to be the nickname
of a user on IRC and more information, if available, is given about the person.

If this function is called interactively then the prefix argument is used
as the channel to query.  No argument means all of them and an argument of -
means the current channel."
  (interactive (if current-prefix-arg
                   (if (eq current-prefix-arg '-) '("*")
                     (list (int-to-string
                            (prefix-numeric-value current-prefix-arg))))
                 '("0")))
  ;; make * be the current channel, even though the server groks it.
  (if (string-match "^\\s *\\*\\(\\s .*\\)?$" channel)
      (setq channel (int-to-string irc-channel)))
  (if (string-match "^\\s *$" channel)
      (setq channel "0"))
  ;; A simple choice to make; if channel isn't a number or nothing, try
  ;; doing a whois with the argument.
  (if (not (numberp (car (read-from-string channel))))
      (irc-execute-whois channel)
    ;; if channel converts to 0 then we will get fresh information about
    ;; who is present.
    (if (zerop (string-to-int channel)) (setq irc-wholist nil))
    (irc-send (concat "WHO " channel))
    ;; a gratuitous blank line only if called interactively.
    (if (and (not irc-conserve-space) (interactive-p)) (irc-insert ""))))

(defun irc-execute-whois (user)
  "Usage: /WHOIS user

Get a two line description of who and where 'user' is.  If user is not
provided it is read from the minibuffer with a completing-read."
  (interactive '(""))
  (setq user (irc-read-user "Who is who? " user))
  (if (string< "" user) (irc-send (concat "WHOIS " user)))
  (if (and (not irc-conserve-space) (interactive-p)) (irc-insert "")))

(defun irc-execute-list (&optional channel)
  "Usage: /LIST [ channel ]

Get a list of the discussions that are on IRC.  The optional channel argument
is supposed to show just that channel but this is not currently supported
by most servers."
  ;; according to Comms LIST can take an optional channel number.
  ;; don't believe it -- it doesn't.  I send one anyway just in case it
  ;; gets fixed; in the meantime servers seem to ignore any extra stuff
  (interactive)
  (irc-send (concat "LIST " channel))
  ;; put a blank line before the list if interactive
  (if (and (not irc-conserve-space) (interactive-p)) (irc-insert "")))

(defun irc-execute-links (&optional cruft)
  "Usage: /LINKS

Show the names of all the servers which can communicate with your server.
The links can go down isolating different parts of the IRC-net, so this
is a good way to find out how extensive it is at the moment.  Any arguments
to this command are ignored."
  (interactive)
  (irc-send "LINKS")
  (if (and (not irc-conserve-space) (interactive-p)) (irc-insert "")))

(defun irc-execute-lusers (&optional cruft) ; I still don't like the name
  "Usage: /LUSERS

Get the number of users and servers on your IRC network.  Arguments to this
command are ignored."
  (interactive)
  (irc-send "LUSERS")
  (if (and (not irc-conserve-space) (interactive-p)) (irc-insert "")))

(defun irc-execute-admin (server)       ; what an evil thought
  "Usage: /ADMIN [ server ]

Get information about the IRC administrator for 'server'; if server is not
supplied just query for the server to which you are connected."
  (interactive "sAdministrative info about which server? ")
  (irc-send (concat "ADMIN " server))
  (if (and (not irc-conserve-space) (interactive-p)) (irc-insert "")))

(defun irc-execute-time (&optional server)
  "Usage: /TIME [ server ]

Get the current time on 'server'; is no server is provided use the one to which
you are connected.  When called with a interactively with a prefix-argument
the server name is read using the minibuffer.

Querying other servers can be handy given that people on IRC are spread out
 from the west coast of the United States to Finland.  The question \"What
time is it in Finland?\" comes up so frequently that an alias -- /TF -- has
been provided by default to get the answer.  This alias should work as long
as tut.fi is connected to your IRC-net."
  (interactive (if current-prefix-arg
                   (list (read-string "Get time at which server? "))
                 '("")))
  (irc-send (concat "TIME " server)))

(defun irc-execute-join (channel)
  "Usage: /JOIN channel

Join 'channel' on IRC.  If channel is not provided it is requested in the
minibuffer; when called interactively channel is set to the prefix argument if
one is present.  Use /LEAVE to exit the channel."
  (interactive (if current-prefix-arg
                   (list (int-to-string
                          (prefix-numeric-value current-prefix-arg)))
                 '("")))
  (if (string= channel "")
      (setq channel (read-string "Channel to join? ")))
  (or (string-match "^\\s *$" channel)  ; well, so much for that
    ;; the seeming no-op is nice for turning random garbage into "0"
      (irc-send (concat "CHANNEL " (int-to-string (string-to-int channel))))))

(defun irc-execute-leave (&optional cruft)
  "Usage: /LEAVE

Leave your current channel and join no other.  Any arguments to this command
are ignored."
  (interactive)
  (irc-send "CHANNEL 0"))

(defun irc-execute-nick (name)
  "Usage: /NICKNAME name

Change your nickname in IRC.  A nickname can contain alphanumeric characters,
underscores (_), hyphens (-) or the special characters vertical bar (|) and
left brace ({), which are alphabetic characters in Finnish.  The name cannot
start with a hyphen or number and only the first nine characters are used.

Unfortunately, due to the way confirmation from the servers work, it might be
falsely reported that your nickname was successfully changed when it was not.
The server will come back and say so and finally irc-mode will wise-up and
note that your nickname was not changed."
  (interactive "sNew nickname? ")
  (while (not (string-match "^\\([a-zA-Z|{_][a-zA-Z0-9-_|{]*\\)$" name))
    (setq name (read-string
                (format "\"%s\" is not valid.  New nickname? " name))))
  (if (> (length name) 9) (setq name (substring name 0 9)))
  (if (string= "" name)
      (if (interactive-p)
          (irc-insert "Nickname not changed.")
        (call-interactively 'irc-execute-nick))
    (irc-insert "You will now be known as \"%s\"." name)
    (put 'irc-nick 'o-nick irc-nick)
    (setq irc-nick name)
    (set-buffer-modified-p (buffer-modified-p))
    (irc-send (concat "NICK " name))))

(defun irc-execute-quit (&optional text)
  "Usage: /QUIT

Exit IRC.  The connexion is closed but the buffer is left behind.
Arguments to this command are not ignored; if any are present then
the session is not exited as a safety precaution against seemingly
unintentional use of the command."
  (interactive)
  (if (and text (string< "" text))
      (irc-insert "/QUIT takes no arguments.")
    (irc-send "QUIT")))

(defun irc-execute-away (&optional text)
  "Usage: /AWAY message

Mark yourself as away, giving TEXT to people who send you private messages.
Without any arguments it will just insert a message about your current status."
  (interactive "sReason for being away: ")
  (if (string= "" text)
      (if irc-away
          (irc-insert "You are marked as away: '%s'." irc-away)
        (irc-insert "You are not currently marked as being away."))
    (irc-send (concat "AWAY " text))
    (setq irc-away (concat " [" text "]")))
  (set-buffer-modified-p (buffer-modified-p)))

(defun irc-execute-here (&optional cruft)
  "Usage: /HERE

Mark yourself as present (ie, not \"away\") on IRC.  Any arguments to this
command are ignored."
  (interactive)
  (irc-send "AWAY")
  (setq irc-away nil)
  (set-buffer-modified-p (buffer-modified-p)))

(defun irc-execute-topic (topic)
  "Usage: /TOPIC topic

Make 'topic' the description of the current channel; this description is
shown to people looking at the channel listing."
  (interactive (list (if (zerop irc-channel) ""
                       (read-string (format "Topic for channel %d? "
                                            irc-channel)))))
  (if (zerop irc-channel)
      (irc-insert "You aren't on any channel.")
    (if (and (not (interactive-p)) (string-match "^\\s *$" topic))
        (call-interactively 'irc-execute-topic)
      (irc-send (concat "TOPIC :" topic)))))

(defun irc-execute-oper (oper)          ; for crimes against humanity
  "Usage: /OPER [ name [ password ]]

Attempt to become an IRC Operator.  Can take the name of the operator
and the password as arguments.  If name is missing then it will be read
from the minibuffer; if password is missing it will be read and hidden
in the minibuffer.

If you become an operator then the word \"Operator\" will appear in the
minibuffer along with the mode name."
  (interactive "sOperator name? ")
  (string-match "^\\s *\\(\\S *\\)\\s *\\(\\S *\\).*$" oper)
  (let* ((name   (substring oper (match-beginning 1) (match-end 1)))
         (passwd (substring oper (match-beginning 2) (match-end 2)))
         (noname (string= "" name)))
    (if (and (interactive-p) noname) () ; just drop right through
      (if noname (call-interactively 'irc-execute-oper)
        (if (string= "" passwd)
            (setq passwd
                  (irc-read-passwd (format "Password for operator %s? "
                                           name))))
        (irc-send (concat "OPER " name " " passwd))))))

(defun irc-execute-summon (user)
  "Usage: /SUMMON user

Summon a user not on IRC to join IRC.  The argument provided may either be
a user name on the local machine or user@server, where server is another
machine on the IRC-net.  The user must be signed on to the specified server."
  (interactive "sUser to summon to IRC? ")
  (let ((nouser (string-match "^\\s *$" user)))
    (if (and (interactive-p) nouser) ()  ; guess s/he didn't really mean it ...
      (if nouser (call-interactively 'irc-execute-summon)
        (irc-send (concat "SUMMON " user))))))

(defun irc-execute-users (host)         ; this is entirely too violent
  "Usage: /USERS [ server ]

Get a list of the users signed on to 'server'.  If no server name is provided
then the server to which you are connected is used.  When called interactively
a prefix argument means to prompt for the server to query."
  (interactive (if current-prefix-arg
                   (list (read-string "List users on which host? "))
                 '("")))
  (if (and (not irc-conserve-space) (interactive-p)) (irc-insert ""))
  (irc-send (concat "USERS " host)))

(defun irc-execute-info (&optional cruft)
  "Usage: /INFO

Show some information about the programmer of IRC.  Arguments to this command
are ignored."
  (interactive) (irc-send "INFO"))

(defun irc-execute-kill (user)
  "Usage: /KILL user

Forcibly remove a user from IRC.  This command is reserved for IRC Operators."
  (interactive '(""))
  (setq user (irc-read-user "Nuke which user? " user))
  (if (string< "" user) (irc-send (concat "KILL " user))))

(defun irc-execute-invite (user)
  "Usage: /INVITE user [ channel ]

Ask 'user' on IRC to join 'channel'.  If channel is 0, * or not provided then
the invitation defaults to your current channel.  If you are not on any channel
and channel is 0 or not provided then no invitation is sent -- you can't
invite someone to go private.  When called interactively channel is set to
the prefix argument; with no argument or - the current channel is assumed."
  (interactive (list
                (int-to-string
                 (if (and current-prefix-arg (not (eq current-prefix-arg '-)))
                     (prefix-numeric-value current-prefix-arg)
                   irc-channel))))
  (if (interactive-p)
      (progn
        (if (and (zerop irc-channel) (string= "0" user))
            (setq user (read-string "Invite to which channel? ")))
        ;; this is so irc-read-user will force a completing-read
        ;; something needs to come up as "name" so that "channel" comes up in
        ;; the right place.  a tiny kludge but the results are the same
        (setq user (concat ". " user))))
  (string-match "^\\s *\\(\\S *\\)\\s *\\(\\S *\\).*$" user)
  (let* ((name    (substring user (match-beginning 1) (match-end 1)))
         (channel (substring user (match-beginning 2) (match-end 2)))
         (noname  (string= "" name)))
    (cond
     (noname (call-interactively 'irc-execute-invite)) ; no arguments ...
     ((and (zerop irc-channel) (zerop (string-to-int channel)))
      (irc-insert "You are not on any channel.  No invitation sent."))
     (t (setq name (irc-read-user (format "Invite whom to channel %d? "
                                          (string-to-int channel))
                                  (if (string= "." name) "" name)))
        (if (string< "" name)
            (irc-send (concat "INVITE " name " " channel)))))))

(defun irc-execute-names (channel)
  "Usage: /NAMES [ channel ]

Show which channels everyone is on.  Optional argument 'channel' (which can
be provided as a prefix argument if called interactively) means to show
just the users on that channel.  * or an interactive prefix argument of -
means to show people on the current channel.

Each line starts with a column for the channel number and is followed by the
nicknames of the people on that channel.  Users who are on private channels
or who are not on any channel are listed as \"Private\".  Users who are
on secret channels (channels less than 0) are not shown at all."
  (interactive (if current-prefix-arg
                   (if (eq current-prefix-arg '-) '("*")
                     (list (int-to-string
                            (prefix-numeric-value current-prefix-arg))))
                 '("0")))
  ;; server doesn't understand * for current channel.  but we want to be
  ;; nice and consistent in the client so we take it.
  (if (string-match "^\\s *\\*\\(\\s .*\\)?$" channel)
      (setq channel (int-to-string irc-channel)))
  ;; have to do some weird things here.  servers don't grok a NAMES 0
  ;; at all so have to make anything that appears to be 0 really disappear.
  ;; names also provides us with fresh information on who is around.
  (if (zerop (string-to-int channel))
      (setq irc-wholist nil channel ""))
  (irc-send (concat "NAMES " channel))
  (if (and (not irc-conserve-space) (interactive-p)) (irc-insert ""))
  ;; need a header here.  server is not gratuitous as in WHOREPLY.
  (irc-insert "Channel  Users"))

(defun irc-execute-wall (msg)
  "Usage: /WALL message

Send 'message' to everyone on IRC.  This can only be done by IRC Operators."
  (interactive "sMessage for everyone: ")
  (if (and (not (interactive-p)) (string= "" msg))
      (call-interactively irc-execute-wall)
    (if (string< "" msg)
        (irc-send (concat "WALL " msg)))))

(defun irc-execute-rehash (&optional cruft)
  "Usage: /REHASH

Force the server to which you are connected to reread it's irc.conf file.
Arguments are ignored.  This command is only available to IRC Operators."
  ;; what a joy this was to write
  (interactive) (irc-send "REHASH"))

(defun irc-execute-trace (server)
  "Usage: /TRACE [ server ]

Find the route from the server to which you are attached to 'server'; if the
server argument is not provided then the servers to which the current server
is directly connected are listed.  This command is only available to IRC
Operators."
  (interactive (list (if current-prefix-arg
                         (read-string "Trace route to which server? ")
                       "")))
  (string-match "^\\s *\\(\\S *\\).*$" server)
  (irc-send (concat "TRACE "
                    (substring server (match-beginning 1) (match-end 1))))
  (if (interactive-p) (irc-insert "")))

;; /command just for the client  (need /stamp /alias /unalias)
(defun irc-execute-send (slist)
  "Usage: /SEND [ sendlist | - ]

Set the default sendlist for IRC messages.  This is a comma separated list
of the intended recipient(s) of messages which do not have an explicit
sendlist.  '-' as an argument means to disable the default sendlist; every
message sent then must have an explicit recipient provided with the message.
Without any arguments this command just displays the current default sendlist.

Each item specified is checked to see whether you can send there; ambiguous
references to users are not allowed nor are channels which you are not on.
\"*\" is always allowed and means to send to the current channel.
If no item in the new list can be set then the sendlist is not changed."
  (interactive "sDefault recipient(s) for messages? ")
  ;; blast some whitespace
  (setq slist (irc-nuke-whitespace slist))
  (let (matches)
    ;; first the easiest case
    (if (string= "-" slist) (setq irc-default-to nil)
      (setq matches
            (delq nil                   ; more indentation fun.  can someone
                  (mapcar               ; recommend a good style manual?
                   (function
                    (lambda (arg)
                      (setq matches (irc-check-list irc-wholist arg))
                      (cond
                       ((string= arg "*") arg)
                       ((string= arg "0")
                        (irc-insert "You can't send to channel 0.") nil)
                       ((not (zerop (string-to-int arg)))
                        (if (= (string-to-int arg) irc-channel) arg
                          (irc-insert "You are not on channel %s." arg) nil))
                       ((= (length matches) 1) (car matches))
                       ((eq matches nil)
                        (irc-insert "No names found to match \"%s\"." arg) nil)
                       (t
                        (irc-insert "Ambiguous recipient \"%s\"; could be %s."
                                    to (irc-subst-comma
                                        (mapconcat 'eval matches ", ") "or"))
                        nil)))) (irc-burst-comma slist))))
      (if matches
          (setq irc-default-to (concat (mapconcat 'eval matches ",") ";"))
        (or (string= "" slist)  ; only print the error if tried to set it.
            (irc-insert "(no matches -- sendlist not changed)"))))
    (if (not irc-default-to) (irc-insert "Your default sendlist is disabled.")
      (irc-insert
       "You are sending to %s."
       (irc-subst-comma
        (mapconcat 'eval
                   (irc-burst-comma
                    (substring irc-default-to 0
                               (1- (length irc-default-to)))) ", ") "and")))))

(defun irc-execute-notify (notify)
  "Usage: /NOTIFY [ [+]event | -event ] [...]

Set the list of events to notify you about with a message.  Notification
is a one-line message inserted when someone causes that event to occur.
Events are added with +event or simply event; they are removed with -event.
+ adds all supported events and - removes all supported events.  More than
one event can be specified in the arguments.  In case of conflict, the argument
which appears later overrides the argument with which it conflicts.

Currently supported by /NOTIFY are the 'join' and 'nick' events.  Join happens
whenever someone enters or leaves a channel which you are on.  Nick occurs
when someone changes nicknames; recognition of this event is currently limited
to when the person making the change is on the same channel as you."
  (interactive "sNotify for events: ")
  ;; die scurvy whitespace
  (setq notify (irc-nuke-whitespace notify))
  (let ((recog '(join nick topic)) (str notify) sym off event)
    (while (string< "" notify)
      ;; multiple args are okay.  we'll do one at a time.
      (setq str (substring notify 0 (string-match "\\s +\\|$" notify))
            notify (substring notify (match-end 0)))
      (string-match "^\\([-+]?\\)\\(.*\\)$" str)
      (setq off (string= "-" (substring str (match-beginning 1) (match-end 1)))
            event (substring str (match-beginning 2) (match-end 2))
            sym (if (string= "" event) nil
                  (car (delq nil              ; do some minor pattern matching
                             (mapcar          ; to find the intended event
                              (function
                               (lambda (arg)
                                 (if (string-match
                                      (concat "^" (regexp-quote event))
                                      (prin1-to-string arg))
                                     arg))) recog)))))
      (cond
       ((and (string= "" event) off) (setq irc-notifies nil))
       ;; the only way for this to happen and not the above is str == "+"
       ((string= "" event) (setq irc-notifies recog))
       ((null sym) (irc-insert "Notify: Unknown argument '%s'." event))
       (t (setq irc-notifies (if off (delq sym irc-notifies)
                               (if (not (memq sym irc-notifies))  ; avoid
                                   (cons sym irc-notifies)        ; redundancy
                                 irc-notifies))))))
    (if irc-notifies
        (irc-insert "Notification is currently enabled for %s."
                    (irc-subst-comma (mapconcat 'prin1-to-string irc-notifies
                                                ", ") "and"))
      (irc-insert "Notification is currently disabled."))))

(defun irc-execute-confirm (str)
  "Usage: /CONFIRM [ + | - ]

Turn on message confirmation with + or off with -.  Any other arguments or no
arguments just gives a message about the current setting.

Message confirmation is a line indicating to whom a message was sent.
Occasionally this will say that a message has been sent to someone who
was not present but another message soon after will set the record straight."
  (interactive "sSet confimation on (+) or off (-)? ")
  ;; grab the first arg
  (string-match "^\\s *\\(\\S *\\).*$" str)
  (setq str (substring str (match-beginning 1) (match-end 1)))
  (cond ((string= str "+") (setq irc-confirm t))
        ((string= str "-") (setq irc-confirm nil)))
  (irc-insert "Message confirmation is %s." (if irc-confirm "on" "off")))

(defun irc-execute-ignore (user)
  "Usage: /IGNORE user

Ignore another user on IRC.  Any events by this person (except for WALL)
are not displayed.  With no arguments a list of all currently ignored people.

IRC-mode will track the ignored user across nickname changes if it notices the
change.  If the user sends either a private message or an invitation to you
while being ignored a message will be sent to that person saying \"You are
being ignored.\"  To undo this command, use /UNIGNORE."
  (interactive '(""))
  (if (or (interactive-p) (not (string= "" user)))
      (setq user (irc-read-user "Ignore which user? " user)))
  (if (string= "" user)
      (if irc-ignores
          (irc-insert "You are currently ignoring %s."
                      (irc-subst-comma (mapconcat 'eval irc-ignores ", ")
                                       "and"))
        (irc-insert "You are not ignoring anyone."))
    (irc-insert "You are now ignoring %s." user)
    (irc-maintain-list 'irc-ignores user 'add)))

(defun irc-execute-unignore (user)
  "Usage: /UNIGNORE user | + | -

Stop ignoring a user who has been /IGNOREd.  The special arguments + or -
mean to stop ignoring everyone who is being ignored."
  ;; how long a doc string could I write for this?
  (interactive '(""))
  (if (null irc-ignores)
      (irc-insert "You are not ignoring anyone.")
    (if (string-match "^\\s *\\([-+]\\)\\(\\s |$\\)" user)
        (progn (setq irc-ignores nil)
               (irc-insert "You are no longer ignoring anyone."))
      (setq user (irc-read-user "Stop ignoring whom? " user irc-ignores))
      (if (string= "" user) ()
        (irc-insert "You are no longer ignoring %s." user)
        (irc-maintain-list 'irc-ignores user 'remove)))))

(defun irc-execute-signal (sigs)
  "Usage: /SIGNAL [ + | - | [+]event | -event ] [...]

Set the events which will get signals (aks bells or dings) when they
occur.  Events supported are:

  private -- private messages      join   -- channel changes
  public  -- public messages       topic  -- channel topic changes
  wall    -- broadcast messages    nick   -- nickname changes
  invite  -- invitations

Without any arguments /SIGNAL simply prints a message about what signals
are currently enabled.  With event or +event turn on all signalling for that
event.  Remove all signals for an event with -event.  /SIGNAL + or /SIGNAL -
adds or removes all signals respectively.  Multiple arguments are accepted;
later ones take precedence over the ones which came before them.  For example,
'/SIGNAL - +w +i' would turn off all signals and then turn on signalling only
for wall messages and invitations."
  (interactive "sSet signal: ")
  ;; blow some whitespace away.  curiously this doesn't work correctly in debug
  (setq sigs (irc-nuke-whitespace sigs))
  (let ((recog '(private public wall invite join nick topic)) str sym
        on off event)
    (while (string< "" sigs)
      ;; take one argument at a time
      (setq str  (substring sigs 0 (string-match "\\s +\\|$" sigs))
            sigs (substring sigs (match-end 0)))
      (string-match "^\\([-+]?\\)\\(.*\\)$" str)
      (setq off (string= "-" (substring str (match-beginning 1) (match-end 1)))
            event (substring str (match-beginning 2) (match-end 2))
            sym (if (string= "" event) nil
                  (car (delq nil
                             (mapcar
                              (function
                               (lambda (arg)
                                 (if (string-match
                                      (concat "^" (regexp-quote event))
                                      (prin1-to-string arg))
                                     arg))) recog)))))
      (cond
       ((and (string= "" event) off)
        (setq irc-signals (mapcar 'list recog)))
       ((string= "" event)
        (setq irc-signals (mapcar
                           (function (lambda (arg) (list arg t))) recog)))
       ((null sym) (irc-insert "Signal: Unknown argument '%s'." event))
       (t (if off (setcdr (assoc sym irc-signals) nil)
            (setcdr (assoc sym irc-signals) '(t))))))
    (setq on (delq nil
                   (mapcar        ; test against t because I have plans
                    (function     ; to couple users and events
                     (lambda (arg)
                       (if (eq (nth 1 (assoc arg irc-signals)) t)
                           arg))) recog)))
    (if on
      (irc-insert (concat "Signalling is enabled for "
                          (irc-subst-comma
                           (mapconcat 'prin1-to-string on ", ") "and") "."))
      (irc-insert "All signalling is currently disabled."))))

(defun irc-execute-stamp (stamp)
  "Usage: /STAMP [ + | - | [+]event | -event | interval ] [...]

Set time-stamping for IRC.  + means to turn it on for all messages from users
and - means to turn it off for them.  +event or just event will turn it on for
that class of message and -event means to disable it for those messages.  An
integer interval means to insert a message indicating the time every N minutes,
where N is the interval.  With no arguments simply insert a message indicating
the current time-stamps.

The current time in HH:MM format can appear two different ways in IRC.  One is
to have it associate with 'event'; two events, 'private' and 'public' messages,
are supported this way.  The other is to have it as a stand-alone message
indicating the current time.  Both can be very useful in noting when someone
actually sent you a message or when another event happened if you happen to be
away for a while.  The accuracy of the interval timer is currently limited to
0-2 minutes beyond the interval if display-time is not running; accuracy is
greatly improved if it is.  It can be turned off by setting the interval
to 0."
  (interactive "sSet time-stamp: ")
  ;; whee.  napalm would feel particularly good here.
  (setq stamp (irc-nuke-whitespace stamp))
  (let (str sym event off)
    (while (string< "" stamp)
      ;; as the args go marching one by one the last one stopped ... <ahem>
      (setq str   (substring stamp 0 (string-match "\\s +\\|$" stamp))
            stamp (substring stamp (match-end 0)))
      (string-match "^\\([-+]?\\)\\(.*\\)$" str)
      (setq off (string= "-" (substring str (match-beginning 1) (match-end 1)))
            event (substring str (match-beginning 2) (match-end 2))
            sym (cond ((string= "" event) nil)
                      ((string-match (concat "^" (regexp-quote event))
                                     "private") 'private)
                      ((string-match (concat "^" (regexp-quote event))
                                     "public")  'public)
                      ((natnump (car (read-from-string event)))
                       (car (read-from-string event)))))
      ;; the following cond is really what sets eveything
      (cond ((and (string= "" event) off) (setq irc-message-stamp nil))
            ((string= "" event) (setq irc-message-stamp t))
            ((null sym) (irc-insert "Stamp: Unknown argument '%s'." event))
            ((natnump sym) (setq irc-time-stamp sym))
            (off (setq irc-message-stamp
                       (car (delq sym (if (eq irc-message-stamp t)
                                          '(private public)
                                        (list irc-message-stamp))))))
            (t (setq irc-message-stamp
                     (cond ((null irc-message-stamp) sym)
                           ((or (eq irc-message-stamp t)
                                (eq irc-message-stamp sym)) irc-message-stamp)
                           (t t)))))))
  (irc-insert "%s messages get time-stamps.%s"
              (cond ((eq irc-message-stamp t) "Private and public")
                    ((null irc-message-stamp) "No")
                    (t (capitalize (prin1-to-string irc-message-stamp))))
              (if (zerop irc-time-stamp) ""
                (format "  The time interval is %d minutes."
                        irc-time-stamp))))

(defun irc-execute-alias (alias)
  "Usage: /ALIAS [ alias [ command [ args for command ]]]

Allow 'alias' to be equivalent to 'command'.
For example, \"/ALIAS tf time tut.fi\" will make typing \"/tf\" be equivalent
to having issued the command \"/time tut.fi\".  Aliases can only be made
to existing commands not other aliases.  They are also only recognized when
in the command name position of a line.  If given with no arguments then
all aliases are displayed; if given with just an alias name then the alias
with that name will be shown.  Aliases can be removed with /UNALIAS."
  (interactive "sName for alias? ")
  (if (interactive-p)
      (setq alias (concat alias " "
                          (read-string (format "Alias '%s' to which command? "
                                               alias)))))
  (setq alias (irc-nuke-whitespace alias))
  (string-match "^/?\\(\\S *\\)\\s */?\\(\\S *\\)\\s *\\(.*\\)$" alias)
  (let ((new (upcase (substring alias (match-beginning 1) (match-end 1))))
        (cmd (upcase (substring alias (match-beginning 2) (match-end 2))))
        (arg         (substring alias (match-beginning 3) (match-end 3)))
        match)
    (cond
     ((string= "" new)
      (let ((aliases irc-alias-alist))
        (while aliases
          (irc-insert "\"/%s\" is aliased to \"/%s\"."
                      (car (car aliases)) (cdr (car aliases)))
          (setq aliases (cdr aliases)))))
     ((string= "" cmd)
      (let ((alias (assoc new irc-alias-alist)))
        (if alias
            (irc-insert "\"/%s\" is aliased to \"/%s\"."
                        (car alias) (cdr alias))
          ;; this could possibly have done some matching to see whether
          ;; just an abbrev was being given, but we'll just take it as given
          (irc-insert "\"/%s\" is not aliased." new))))
     (t  ; okay, we've got at least a command.  let's try and make this as
         ; painless as possible. 
      (setq match
            (irc-check-list (mapcar 'car (append irc-command-alist
                                                 (if irc-operator
                                                     irc-operator-alist)))
                            cmd 'start-only))
      ;; try not to confuse a regular user with commands he couldn't use
      ;; anyway, but let him at it if that's what he really wants.  it'll
      ;; just come through as an error from the server in the long run ...
      (if (and (not match) (not irc-operator))
          (setq match (irc-check-list (mapcar 'car irc-operator) cmd t)))
      (if (/= (length match) 1)
          (if match
              (irc-insert "'/%s' is an ambiguous command.  Could be %s." cmd
                          (irc-subst-comma (mapconcat 'eval match ", ") "or"))
            (irc-insert "Command not found: '/%s'." cmd))
        (irc-change-alias new (concat (downcase (car match))
                                        ; no trailing space if no arg
                                      (if (string= "" arg) "" " ") arg) 'add)
        (irc-insert "\"/%s\" has been aliased to \"/%s\"." new 
                    (cdr (assoc new irc-alias-alist))))))))

(defun irc-execute-unalias (alias)
  "Usage: /UNALIAS alias

Remove the 'alias' for a command."
  ;; well, that's a pretty dull doc string.
  (interactive (list (completing-read "Unalias which command? "
                                      (cons '("" . "") irc-alias-alist)
                                      nil t)))
  (string-match "^\\s *\\(\\S *\\)\\s *$" alias)
  (setq alias (substring alias (match-beginning 1) (match-end 1)))
  (if (string= "" alias)
      (if (not (interactive-p))
          (call-interactively 'irc-execute-unalias))
    (let ((match (irc-check-list (mapcar 'car irc-alias-alist) alias t)))
      (if (/= (length match) 1)
          (if match
              (irc-insert "'%s' is an ambiguous alias.  Could be %s."
                          (upcase alias)
                          (irc-subst-comma (mapconcat 'eval match ", ") "or"))
            (irc-insert "No alias found to match '%s'." (upcase alias)))
        (irc-change-alias (car match) nil 'remove)
        (irc-insert "'%s' is no longer aliased." (car match))))))
  
(defun irc-execute-help (topic)
  "Usage: /HELP topic

Get the documentation for 'command'.  If no command is given then a list
of the possible topics is shown.  Note that commands for IRC Operators will
not appear in the help topics when not an IRC Operator."
  (interactive "sHelp for which command? ")
  (string-match "^\\s *\\(\\S *\\)\\s *$" topic)
  (setq topic (substring topic (match-beginning 1) (match-end 1)))
  (if (string= topic "")
      (let ((str "Help is available for the following IRC-mode commands:\n")
            (topics (mapcar 'car
                            (append irc-command-alist
                                    (if irc-operator irc-operator-alist)))))
        (while topics
          (setq str
                (concat str
                        (format "\n%14s%14s%14s%14s%14s"
                                (nth 0 topics)
                                (or (nth 1 topics) "") (or (nth 2 topics) "")
                                (or (nth 3 topics) "") (or (nth 4 topics) "")))
                topics (nthcdr 5 topics)))
        (with-output-to-temp-buffer "*Help*" (princ str)))
    (let ((match (irc-check-list (mapcar 'car (append irc-command-alist
                                                      (if irc-operator
                                                          irc-operator-alist)))
                                 topic 'start-only)))
      (if (and (not match) (not irc-operator))
          (setq match  
                (irc-check-list (mapcar 'car irc-operator-alist) topic t)))
      (if (/= (length match) 1)
          (if match
              (irc-insert "Ambiguous help topic '%s'; could be %s."
                          (upcase topic)
                          (irc-subst-comma (mapconcat 'eval match ", ") "or"))
            (irc-insert "No help is available for '%s'." (upcase topic)))
        (setq match (car match))
        (with-output-to-temp-buffer "*Help*"
          (princ (documentation
                  (intern-soft
                   (concat "irc-execute-"
                           (cdr (assoc match
                                       (if (assoc match irc-command-alist)
                                           irc-command-alist
                                         irc-operator-alist))))))))))))

;; miscellaneous irc-* commands
(defun irc-truncate-buffer (size)
  "Remove as many lines from the beginning of the buffer as is necessary
to get it under SIZE characters.  This function is used by irc-mode
to prevent an irc-session from consuming gross amounts of space."
  (if (< (buffer-size) size) ()
    (save-excursion
      ;; first go to the lowest point posssible that would do it
      (goto-char (- (point-max) size))
      ;; get to the end of this line
      (end-of-line)
      (if (< (point) irc-mark)
          ;; just to make sure we don't toast pending input
          (delete-region 1 (1+ (point)))
        (message "Warning: %s exceeding %s characters.  Couldn't truncate."
                 (buffer-name (current-buffer)) size)))))

(defun irc-read-passwd (&optional prompt)
  "Allow user to type a string without it showing.  Returns string.
If optional PROMPT non-nil, use it as the prompt string in the minibuffer."
  ;; this is based on a similar function in telnet.el
  ;; the major drawback is that while being prompted for a password
  ;; it stays in this routine until C-g, RET or LFD is typed.
  (let ((passwd "") (echo-keystrokes 0) char)
    (if prompt (message prompt))
    (while (not (or (= (setq char (read-char)) 13) (= char 10)))
      ;; naughty bit.  take C-h to mean DEL.
      (if (or (= char 8) (= char 127))
          (if (> (length passwd) 0)
              (setq passwd (substring passwd 0 (1- (length passwd)))))
        (setq passwd (concat passwd (char-to-string char))))
      (if prompt (message (concat prompt (make-string (length passwd) ?*)))))
    (if prompt (message ""))
    passwd))

(defun irc-read-user (prompt user &optional list)
  "Prompting with PROMPT, read an IRC nickname from the minibuffer.
Second argument USER is a string which is checked for a non-ambiguous match
before the minibuffer read is done.  Optional third argument LIST is a
list to use for checking rather than the irc-wholist.

It returns either the name of a user or an empty string (\"\")."
  (string-match "^\\s *\\(\\S *\\)" user) ; just want one name
  (setq user (substring user (match-beginning 1) (match-end 1)))
  (let ((completion-ignore-case t) (list (if list list irc-wholist)) match)
    (if (or (string= "" user)
            (/= (length (setq match (irc-check-list list user))) 1))
        (completing-read (format "%s%s"
                                 (if (string= "" user) ""
                                   (format (if (zerop (length match))
                                               "No names match '%s'.  "
                                             "'%s' is ambiguous.  ")
                                           user))
                                 prompt)
                         ;; build the list for completing-read.  a
                         ;; null string is there so that it can exit
                         ;; without anything, since we require matches
                         (mapcar 'list (cons "" list))
                         nil t user)
      (car match))))

(defun irc-nuke-whitespace (str)
  "One string argument.  Returns it with surrounding whitespace removed."
  ;; i hate stupid extra spaces when parsing
  (string-match "^\\s *" str)
  (substring str (match-end 0) (string-match "\\s *$" str)))

(defun irc-subst-comma (str newsep)
  "Return the string formed by substituting for the last \", \" in STR
the string NEWSEP followed by a space.  For example:
  (irc-subst-comma \"1, 2, 3\" \"or\") => \"1, 2 or 3\"

This function is especially designed for making message from irc-mode
more grammatically correct and the strings which it operates on should
be carefully chosen so as to avoid possibly blowing away a comma that
really wasn't separating elements in a list."
  ;; did you know that example up there can't appear starting in column 0
  ;; without screwing up lisp-indent-line?
  (if (string-match ", [^,]*$" str)
      (concat (substring str 0 (match-beginning 0)) " " newsep
              (substring str (1+ (match-beginning 0))))
    str))

(defun irc-get-time ()
  "Return the hour and minutes of the current time in the form \"HH:MM\"."
  (let ((time (current-time-string)))
    (substring time (string-match "..:.." time) (match-end 0))))

(defun irc-check-time ()
  "Check to see whether it is time to insert a current-time message into
the *IRC* buffer."
  (let* ((time (irc-get-time))
         (old-minute (string-to-int (substring irc-last-time 3)))
         (new-minute (string-to-int (substring time 3))))
  (if (zerop irc-time-stamp) ()
    ;; check the time sentinel
    (if (string= irc-last-time time) ()
      ;; time has gone stomping on by ...
      (setq new-minute (+ new-minute (if (< new-minute old-minute) 60 0))
            irc-last-time time
            irc-total-time (+ irc-total-time (- new-minute old-minute)))
      (if (< (- irc-total-time irc-last-stamp) irc-time-stamp) ()
        ;; it's time for a new message
        (irc-insert "*** It is now %s ***" time)
        ;; might as well check to see if display-time is running
        (irc-wrap-display-time)
        (setq irc-last-stamp irc-total-time))))))

(defun irc-wrap-display-time ()
  "Set up a wrapper around the display-time-filter to hopefully provide a
little better accuracy for the time stamps."
  (if (and (fboundp 'display-time-filter)
           (not (fboundp 'original-display-time-filter)))
      (progn
        (fset 'original-display-time-filter
              (symbol-function 'display-time-filter))
        ;; a nested defun seems to do funny things to the byte-compiler, so
        ;; instead we find a way around it.
        (fset 'display-time-filter
              (function
               (lambda (proc str)
                 "
The filter for the display-time-process.  This function has been modified
for IRC-mode to call irc-check-time before calling the original
display-time-filter."
                 (save-excursion
                   (let (buf (procs irc-processes))
                     (while procs
                       (if (setq buf
                                 (buffer-name (process-buffer (car procs))))
                           (progn
                             (set-buffer buf)
                             (irc-check-time)))
                       (setq procs (cdr procs)))))
                 (original-display-time-filter proc str)))))))

(defun irc-change-alias (alias cmd add)
  "Modify ALIAS for CMD in the irc-alias-alist.  ADD non-nil means to put the
alias in the list, nil (or the symbol `remove') means to clear it.  This
function does no hand-holding like /ALIAS; its intended use is in
irc-mode-hook."
  (let ((entry (assoc (upcase alias) irc-alias-alist)))
    (if (or (null add) (eq add 'remove))
        (setq irc-alias-alist (delq entry irc-alias-alist))
      (if entry (setcdr entry cmd)
        (setq irc-alias-alist
              (cons (cons (upcase alias) cmd) irc-alias-alist))))))

(defun irc-signal (user event)
  "Return t if a ding should be issued for a USER/EVENT pair.
Currently only the event part of things is supported by /SIGNAL."
  (let ((signal (cdr (assoc event irc-signals))))
    (or (memq t signal) (member-general user signal 'string=)
        (member-general user (cdr (assoc 'user irc-signals)) 'string=))))

(defun irc-check-list (list item &optional start-only)
  "See if LIST has string ITEM.  Returns a list of possible matches.  The list
returned is based on the following precedence rules:  if there is an exact
match, it is returned.  If there are any strings in the list whose beginning
match the item, they are returned.  If that fails and optional argument
START-ONLY is missing or nil, strings which have the item match anywhere are
returned.  As a last resort, nil is returned.
This function is not case-sensitive."
  (let (return (case-fold-search t) (item (regexp-quote item)))
    (if (setq return
              (delq nil                         ; whole words
                    (mapcar (function   
                             (lambda (arg)
                               (if (string-match (concat "^" item "$") arg)
                                   arg))) list)))
        return
      (if (setq return
                (delq nil                       ; beginnings
                      (mapcar (function
                               (lambda (arg)
                                 (if (string-match (concat "^" item) arg)
                                     arg))) list)))
          return
        (if start-only
            nil
          (delq nil
                (mapcar (function               ; anywhere
                         (lambda (arg)        
                           (if (string-match (concat "." item) arg) arg)))
                        list)))))))

(defun irc-maintain-list (list item func)
  "Maintain a LIST of strings by adding or removing string ITEM.
Third argument FUNC should be 'add or t or to make sure the item is in
the list or 'remove or nil to make sure item is out of the list."
  (cond
   ((memq func '(add t))
    (if (member-general item (eval list) 'string=) ()
      ;; sigh.  with random adding of names via sending messages to people
      ;; that irc-mode doesn't know about a name can be here in the wrong
      ;; case.  this has the potential to screw things up big so we'll ditch
      ;; the old one in favour of whatever is being added.
      (let* ((case-fold-search t)
             (old (delq nil
                        (mapcar
                         (function
                          (lambda (arg)
                            (if (string-match (concat "^" (regexp-quote item)
                                                      "$") arg)
                                arg))) (eval list)))))
        (while old
          (irc-maintain-list list (car old) 'remove)
          (setq old (cdr old)))
        (set list (cons item (eval list))))))
   ((memq func '(remove nil))
    (set list  
         (delq nil (mapcar (function (lambda (arg)
                                       (if (string= item arg) nil arg)))
                           (eval list)))))))

(defun irc-burst-comma (str)
  "Take a comma or space separated STR and turn it into a list of its elements.
Example: \"1, 2,3,4,  6  7\" becomes the list (\"7\" \"6\" \"4\" \"3\" \"2\" \"1\")."
  (let (list sub (beg 0))
    (string-match "" str)
    (while (string-match ",+\\|\\s +\\|,+\\s +" str beg)
      (if (not (string= (setq sub (substring str beg (match-beginning 0))) ""))
          (setq list (cons sub list)))
      (setq beg (match-end 0)))
    (if (/= (length str) beg) (cons (substring str beg) list) list)))

;; miscellaneous other commands (usually from other sources)

;; this makes up for not being able to provide a :test to memq.
;; member-general by Bard Bloom <bard@theory.lcs.mit.com>
(defun member-general (x l comparison)
  "Is X a member of L under COMPARISON?"
  (let ((not-found t))
    (while (and l not-found)
      (setq not-found (not (funcall comparison x (car l)))
            l         (cdr-safe l)))
    (not not-found)))

;; wish i could remember who I got this from; I had to patch it to work
;; with the minibuffer correctly but it is mostly untouched.
(defun walk-windows (proc &optional no-mini)
  "Applies PROC to each visible window (after selecting it, for convenience).
Optional arg NO-MINI non-nil means don't apply PROC to the minibuffer
even if it is active."
  (let* ((real-start (selected-window))
          (start (next-window real-start no-mini))
          (current start) done)
     (while (not done)
       (select-window current)
       (funcall proc)
       (setq current (next-window current no-mini))
       (setq done (eq current start)))
     (select-window real-start)))

(defun count-windows (&optional no-mini)
   "Returns the number of visible windows.
Optional arg NO-MINI non-nil means don't count the minibuffer
even if it is active."
   (let ((count 0))
     (walk-windows (function (lambda () (setq count (1+ count)))) no-mini)
     count))

;; swiped from minibuf.el, but made exclusive to * Minibuf-n*.
(defun minibuffer-message (format &rest args)
  "Print a temporary message at the end of the Minibuffer.
After 2 seconds or when a key is typed, erase it."
  (if (zerop (minibuffer-depth)) (apply 'message format args)
    (let (p)
      (save-excursion
        (set-buffer (concat " *Minibuf-" (1- (minibuffer-depth)) "*"))
        (unwind-protect
            (progn
              (setq p (goto-char (point-max)))
              (insert (apply 'format format args))
              (sit-for 2))
          (delete-region p (point-max)))))))

(defun irc-find-to (str &optional explicit)
  "Find the part of STRING that IRC-mode will interpret as the sendlist.
If no explicit list is found, irc-default-to is returned.  The string returned
is either : or ; terminated.

If optional EXPLICIT is non-nil, then return t if a sendlist was explicitly
specified, nil if the sendlist was implicit."
  (let ((matched (string-match "^[A-Za-z0-9-_|{,*]*[;:]" str)))
    (if matched (if explicit t (substring str 0 (match-end 0)))
      (if explicit nil irc-default-to))))

(defun irc-find-message (string)
  "Find the message that IRC will see if STR were sent.  For messages
sent with explicit lists, this is everything following the colon or
semi-colon.  For everything else, it is just the string."
  (substring string (length (irc-find-to string))))

;; functions for the irc-history list
(defun irc-add-to-hist (str)
  "Put STRing at the head of the irc-history list."
  (if (string-match "^[:;]" str)
      (setq str
            (concat irc-last-explicit (substring str 1 (length str)))))
  (setq irc-history (append (list str) irc-history))
  (and (> (length irc-history) irc-max-history)
       (setq irc-history (reverse (cdr (reverse irc-history))))))

(defun irc-yank-prev-command ()
  "Put the last IRC /command in the input-region."
  (interactive)
  (delete-region irc-mark (goto-char (point-max)))
  (insert "/" irc-last-command)
  (goto-char (1+ irc-mark)))

(defun irc-history-prev (arg)
  "Select the previous message in the IRC history list.  ARG means
select that message out of the list (0 is the first)."
  (interactive "P")
  (let ((str (nth (or arg (1+ irc-history-index)) irc-history)))
    (if (not str)
        (message "No message %d in history." (or arg (1+ irc-history-index)))
      (delete-region irc-mark (goto-char (point-max)))
      (insert str)
      (goto-char irc-mark)
      (setq irc-history-index (or arg (1+ irc-history-index))))))

(defun irc-history-next (arg)
  "Select the next message in the IRC history list.  With prefix ARG
select that message out of the list (same as irc-history-prev if
called with a prefix arg)."
  (interactive "P")
  (if arg (irc-history-prev arg)
    (if (= irc-history-index -1)
        (message "No next message in history.")
      (delete-region irc-mark (goto-char (point-max)))
      (insert (if (zerop irc-history-index) ""
                (nth (1- irc-history-index) irc-history)))
      (setq irc-history-index (1- irc-history-index)))))

(defun irc-kill-input ()
  "Delete the input region and start out fresh.  This function is recommended
over any other way of killing the input-region interactively because it
also resets the index for the history list."
  (interactive)
  (delete-region irc-mark (goto-char (point-max)))
  (setq irc-history-index -1))

(defun irc-history-menu ()
  "List the history of messages kept by irc-mode in another buffer."
  (interactive)
  (let ((pop-up-windows t) (hist irc-history) (line 0))
    (save-excursion
      (set-buffer (get-buffer-create "*IRC History*"))
      (fundamental-mode)
      (erase-buffer)
      (while hist
        (insert (format "%2d: %s\n" line (car hist)))
        (setq hist (cdr hist))
        (setq line (1+ line)))
      (if (zerop line)
          (insert "No messages have been sent to IRC yet."))
      (set-buffer-modified-p nil)
      (goto-char (point-min)))
    (display-buffer "*IRC History*")))

;; stuff about irc-mode
(defun irc-version (&optional arg)
  "Print the current version of irc.el in the minibuffer.  With optional
ARG, insert it in the current buffer."
  (interactive "P")
  (if arg (insert irc-version) (princ irc-version)))

(defun irc-news ()
  "Shows news about latest changes to irc.el.  Even shows news about
old changes to irc.el -- what a wonderous function indeed."
  (interactive)
  (save-excursion
    (set-buffer (get-buffer-create "*IRC-mode News*"))
    (erase-buffer)
    (insert "Latest changes to IRC mode:

For functions, use C-h f to describe the function.  Variables use C-h v.

27 Sep 89 -- This release is a mixture of changes in aspects of the interface
   and added support for the latest IRC servers, which use a different reply
   format for some messages.  The changes are listed here.  I have to admit
   that the version has not had as much testing as I would have liked to have
   given it, but I have been very busy lately.  I expect that any bugs found
   will be primarily due to typographical errors or some minor oversight.  As
   usual, bug reports and other questions can be sent to me <tale@pawl.rpi.edu>
   and I'll take a look at it for the next release.

   The client used to be very messy about inserting a whole glob of text
   when it didn't understand the first line.  It should be a little cleaner
   now by just inserting the line it didn't understand and a message to report
   it to me.

   The 'topic' event is now handled by /SIGNAL and /NOTIFY; it occurs when
   someone changes the topic of a channel you have joined.

   I fixed a bug in the channel joining routines that made the client think
   you always made it to a channel you /JOINed, which isn't true if the
   channel is full.

   The default for irc-message-stamp has been changed from nil to private and
   the default for irc-time-stamp has been made 0 rather than 10.  This means
   no *** It is now HH:MM *** messages.

   C-c C-c (the binding for /NAMES) works now with a prefix argument.  There
   was also a bug in formatting the output of /NAMES where a name would not be
   shown on a very full channel; it has been fixed.

   /LUSERS has been added; it shows how many people are on IRC.

   KILL messages are now shown to the client; they indicate who was being
   removed from IRC and who did the removing.

   One space will be stripped from after the : or ; that ends a sendlist.
   That is, \"tale: hi\" and \"tale:hi\" will both send \"hi\" as the message.

   Word-wrapping used to be done with an 80 column screen in mind; it now uses
   the width of the window within Emacs instead.

   Had to fix a pattern to match the message when a nickname change failed;
   it changed in newer servers.

   If display-time is running then it is piggy-backed to give better accuracy
   to interval time stamps.  Additionally, the time is included in the
   \"IRC Session finished\" message.

   irc-conserve-space is a variable that was added for users who want messages
   which somewhat mimick the style of the C client.  It is not completelyy
   true to that style (for example, it still does word-wrapping) but I'm
   pretty sure it will appease them.  It is useful for short screen, slow
   terminal speeds and chatty channels.

   The IRCNICK and IRCSERVER environment variables are supported when irc.el
   loads.  They set the default nickname and server, respectively.

   If /WHO is given a non-numeric argument it will call /WHOIS instead.

   There used to be a bug that an alias would sometimes get executed rather
   than an exactly matching regular command; it has been fixed.

   The buffer is by default in auto-fill-mode with a fill-column of 75.  This
   means that when you are typing a long message and want to move around in it,
   C-p and C-n are much more useful.  Along with this the handling of the
   input-region when RET is pressed has changed.  It is now handled as though
   it is all one line, rather than on a line-at-a-time basis.

   Aliases now have a no-frills function, irc-change-alias, for the setting/
   removing of aliases in non-interactive elisp, as in the irc-mode-hook.
   See the documentation of that function regarding how to use it.

   Last but certainly not least, the numeric messages from the newer servers
   are parsed correctly.  You should be getting a lot fewer \"This might be
   a bug ...\" message.

Older news available on request (it includes a description of what to expect
out of this client, but it takes up way too much space).")
    (goto-char (point-min)))
  (display-buffer "*IRC-mode News*"))
