;;; $Id: irchat.el,v 1.25beta
;;; Internet Relay Chat interface for GNU Emacs
;;; Copyright (C) 1989 Tor Lillqvist
;;;
;;; This program is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 1, or (at your option)
;;; any later version.
;;;
;;; This program is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with GNU Emacs; if not, write to the Free Software
;;; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

;;; Author's email address: tml@hemuli.atk.vtt.fi
;;; You can ftp the latest version from hemuli.atk.vtt.fi
;;; [130.188.52.2]

;;; You may complain about bugs to Kaizzu <kk102087@tut.fi>.
;;; Newest version in funic.funet.fi [128.214.6.100].

;; $Log:	irchat.el,v $

;; Revision 1.25beta 90/06/21 13:30:00 Kaizzu (Kai Kein{nen)
;; Added new numeric reply codes. Sorted out the part of code
;; with the numeric replys. To be done: better parsing of
;; MODE. One-buffer-mode. What else? Tell me!

;; Revision 1.24alpha 90/06/07 14:20:00 Kaizzu (Kai Kein{nen) Now,
;; this tries to follow WiZ' ideas....  Changed irchat-Command-join to
;; understand strings.  Added irchat-channel-alist and
;; irchat-scan-channels to keep a list of channels. Removed unused
;; variables. Changed irchat-Command-list, irchat-Command-who,
;; irchat-Command-names to understand strings. C-u works, but no way
;; to say /who +foo with C-c w. Fixed regexps to like non-numerical
;; channels.

;; Revision 1.23gamma 90/05/13 16:15:00 nam (Kim Nyberg)
;; 2.4 newline-bug fixed with an elegant solution by mta.
;; Fixed broadcast-bugs (private messages wont be broadcasted
;; any more) and minor /away-bug.
;; Empty lines bug fixed (not for privates, though, major argh!)
;; Datestamps for priv messages when away added by cletus.
;; Revision 1.23beta 90/05/13 00:30:00 Kaizzu (Kai Kein{nen)
;; Fixed irchat to work with buggy 2.4. (:-))
;; Added some kind of handlers for new numeric messages.
;;
;; Revision 1.22 90/04/16  13:30:00  Kaizzu (Kai Kein{nen)
;; Changes by Kai Kein{nen based on long-time irritations and Easter.
;; Added handle for 314 (WHOWAS). Fixed some of the bugs in scrolling
;; back and forth. Automagic reconnect added. (No, it didn't exist
;; before.) Added a kludge to add a '_' in the nick in reconnects,
;; irchat is too fast otherwise. Fixed notice-handle so that is removes
;; extra away-notices. Added checking for IRCNAME.
;; Changes by Kim Nyberg, cleaning mta-topic, adding C-u to LIST and
;; TOPIC. 
;;
;; Revision 1.21  90/03/30  06:00:00  nam (Kim Nyberg)
;; Changes by Kim Nyberg with suggestions/code by Kai Kein{nen.
;; Yet another major release with bug fixes and new 'nice' features.
;; Cleaned up some kludges (channel-number for example), added a
;; broadcast-mode (suggested by Kari Syd{nmaanlakka, kts), fixed
;; annoying nickname-bug etc.
;;
;; Revision 1.20  90/03/19  19:30:00  nam (Kim Nyberg)
;; Changes by Jukka Partanen, Markku J{rvinen (mta) & Kim Nyberg.
;; Major release with bug fixes, customization-options. Handles more
;; numeric reply codes, keybindings & functions added. Tries to reconnect
;; automatically (still a major kludge).
;;
;; Revision 1.19  89/11/24  14:38:28  tml (Tor Lillqvist)
;; Changes by Jukka Partanen.
;;
;; Revision 1.18  89/09/18  10:59:53  tml (Tor Lillqvist)
;; Intermediate revision.  Works, but needs cleaning up.  Handles the new
;; numeric reply codes (at least those I have come across).
;;
;; Revision 1.17  89/07/12  15:49:43  tml (Tor Lillqvist)
;; Added handling of NOTICE messages.
;;
;; Revision 1.16  89/07/11  12:39:42  tml (Tor Lillqvist)
;; Fixed killing:  We used to get empty lines for each message from a killed
;; user.  Use irchat-author-nickname also when sending the greeting.
;;
;; Revision 1.15  89/07/10  11:14:45  tml (Tor Lillqvist)
;; Added automatic sending of an greeting to the author, telling him
;; what version of irchat this is.
;;
;; Revision 1.14  89/07/10  10:50:13  tml (Tor Lillqvist)
;; Fixed irchat-Command-redisplay to work like I want (finally...).
;;
;; Revision 1.10  89/07/10  00:08:21  tml (Tor Lillqvist)
;; Added irchat-Command-redisplay (C-c C-l) to get dialogue window back
;; without having to do C-x 4 b C-x o.  Added help for the generic
;; command.  Fixes a bug that caused server messages handled in the same
;; chunk to get a wrong prefix (the old value stayed around).
;;
;; Revision 1.9  89/07/05  15:25:02  tml (Tor Lillqvist)
;; Oops.  Must have (goto-char (point-max)) in irchat-own-message.
;;
;; Revision 1.8  89/07/05  14:57:51  tml (Tor Lillqvist)
;; Freezing the dialogue window now works better: if frozen, we use
;; save-excursion.  Space and Delete scrolls in the dialogue window.
;; Don't print "private message from nil".
;; 
;; Revision 1.7  89/06/20  10:10:52  tml (Tor Lillqvist)
;; Cosmetic changes; removed the (unused) hash table functions; added 
;; my email address.
;; 
;; Revision 1.6  89/06/15  17:25:20  tml (Tor Lillqvist)
;; Now byte-compiles.  (The byte-compiler didn't like lambda expressions.)
;; 
;; Revision 1.5  89/06/15  14:11:08  tml (Tor Lillqvist)
;; Added the freeze command to stop automatic scrolling of the dialogue
;; window.  Added an automatic greeting to the wait command.
;; 
;; Revision 1.4  89/06/14  20:41:36  tml (Tor Lillqvist)
;; More suggestions from mta@tut.fi.
;; Fixed a bug: if a message had nothing after the command word, eg QUIT,
;; it was handled incorrectly.
;; 
;; Revision 1.3  89/06/08  17:59:20  tml (Tor Lillqvist)
;; Suggestions from Markku Jarvinen <mta@tut.fi>:  The Command buffer
;; isn't erased after each message you enter.  You can enter a numeric
;; argument to WHO.  C-c c send the current line to the server (for 
;; compatibility with another Emacs IRC client).  The mode line tells
;; which channel you are on.  The environment variable IRCNICK is checked.
;; 
;; Revision 1.2  89/06/08  14:26:48  tml (Tor Lillqvist)
;; Many enhancements:  We now poll the server periodically with WHO
;; messages, and keep a list of the nicknames on IRC (used for completion
;; when sending private messages or fingering (WHOIS).  Added a WAIT 
;; command, which marks a nickname as waited for, and when we notice that
;; she is present, we inform the loser.  Etc.
;; 
;; Revision 1.1  89/06/05  19:11:41  tml (Tor Lillqvist)
;; Initial revision
;; 

;; This is a set of Emacs-Lisp commands and support functions for
;; the Internet Relay Chat protocol, IRC.
;;
;; The entry point to IRChat is the command irchat.
;;
;; If autoloading then the line:
;;   (autoload 'irchat "irchat" nil t)
;; should appear in a user's .emacs or in default.el in the lisp
;; directory of the Emacs distribution.
;;
;; Much code borrowed from GNUS, thanks to Masanobu Umeda.

(provide 'irchat)

(defconst irchat-RCS-revision "$Revision: 1.25 $")

(defconst irchat-version
  (substring irchat-RCS-revision 11 (- (length irchat-RCS-revision) 2))
  "The version of irchat you are using.")

(defvar irchat-server (getenv "IRCSERVER")
  "*Name of the host running the IRC server.
Initialized from the IRCSERVER environment variable.")

(defvar irchat-service 6667
  "*IRC service name or (port) number.")

(defvar irchat-system-fqdname (system-name)
  "*The fully qualified domain name of the system.
Default is what (system-name) returns.")

(defvar irchat-nickname (or (getenv "IRCNICK") (user-real-login-name))
  "*The nickname you want to use in IRC.
Default is the environment variable IRCNICK, or your login name.")

(defvar irchat-startup-channel nil
  "*The channel to join automagically at start.
If nil, don't join any channel.")

(defvar irchat-command-window-on-top t
  "*If non-nil, the Command window will be put at the top of the screen.
Else it is put at the bottom.")

;; some more or less useful defvars by nam

(defvar irchat-use-full-window t
  "*If non-nil, IRCHAT will use whole emacs window. Annoying for GNUS-
users, therefore added by nam.")

(defvar irchat-change-prefix ""
  "*String to add before any change msg, used for customisation of
IRCHAT to suit old users of the irc-loser-client.")

(defvar irchat-format-string1 ">%s<"
  "*Format string for private messages being sent. Thanks to jvh
for the help with format.")

(defvar irchat-format-string2 "=%s="
  "*Format string for arriving private messages.")

(defvar irchat-format-string3 "(%s)"
  "*Format string for arriving messages to a channel.")

(defvar nam-suffix ""
  "*String to add to end of line ... like n{{s.")

(defvar irchat-ignore-changes nil
  "*Ignore changes? Good in topic-wars/link troubles.")

(defvar irchat-reconnect-automagic nil
  "*Automatic reconnection, default is disabled (huge kludge).
This was implemented by Kai 'Kaizzu' Kein{nen and others.")

(defvar irchat-ask-for-nickname nil
  "*Ask for nickname if irchat was entered with C-u. Suggested
by Kaizzu.")

(setq irchat-channel-filter "")
;;  *Enables use of C-u argument with NAMES and TOPIC."

;; end of nam additions
;; Kaizzu additions:

(defvar irchat-grow-tail "_"
  "*Add a _ to nick when reconnecting. Otherwise you might get killed
again if automagic reconnect is too fast.")

(defvar irchat-ignore-extra-notices t
  "*Don't show NOTICEs with \"as being away\" unless they come from
the local server.")

;; end of Kaizzu additions

;; mta's goodies

(defvar mta-own-topic "Default topic"
  "*Default topic for automagic topic-setter.")

(defvar mta-topic-active nil
  "*Do I want to keep my topic...")

(defvar mta-snap-topic t
  "*Snap mta-own-topic from users /topic command?")

(if mta-topic-active
    (setq irchat-mta-topic-indicator "T")
  (setq irchat-mta-topic-indicator "-"))

;;; Completition --- experimental ---
;;; mta@tut.fi 21-08-89 19:30

(defun get-word-left ()
  "Return word left from point."
  (save-excursion
    (let (point-now)
      (setq point-now (point))
      (backward-word 1)
      (buffer-substring (point) point-now))))

(defun irchat-Command-complete ()
  "Complete word before point from userlist."
  (interactive)
  (insert
   (save-excursion
     (let ((completion-ignore-case t) point-now word result)
       (setq point-now (point))
       (setq word (get-word-left))
       (setq result (try-completion word irchat-nick-alist))
       (backward-word 1)
       (delete-region (point) point-now)
       (if (or (eq result t) (eq result nil))
           word
         result)))))

;; that's it (added by nam ... sure, I know it's a kludge ;)

(defvar irchat-Command-mode-hook nil
  "*A hook for IRCHAT Command mode.")

(defvar irchat-Dialogue-mode-hook nil
  "*A hook for IRCHAT Dialogue mode.")

(defvar irchat-Exit-hook nil
  "*A hook executed when signing off IRC.")

(defvar irchat-kill-nickname nil
  "*A list of nicknames, as symbols, to ignore.  Messages from these people
won't be displayed.")

(defvar irchat-kill-realname nil
  "*A list of real names of people to ignore. Messages from them
won't be displayed.")

(defvar irchat-kill-logon nil
  "*A list of logon names (user@host.dom.ain). Messages from them
won't be displayed.")

(defvar irchat-beep-on-bells nil
  "*If non-nil, and the IRC Dialogue buffer is not selected in a window,
an IRC message arriving containing a bell character, will cause you
 to be notified.
If value is 'always, an arriving bell will always cause a beep (or flash).")

;; Define hooks for each IRC message the server might send us.
;; The newer IRC servers use numeric reply codes instead of words.

(defvar irchat-msg-list
  '(channel error invite linreply msg namreply nick ping pong
	    privmsg quit topic wall whoreply kill wallops mode kick
	    401 402 403 404 411 412 421 431 432 433 441 442 451 461
	    462 463 464 465 466 471 472 473 481 491 301 311 312 313
	    316 314 315 321 322 323 324 331 332 341 351 361 365 366
	    371 372 381 382 391)
  "A list of the IRC messages and numeric reply codes we are prepared to handle.")

(mapcar (function
	 (lambda (sym)
	   (eval (list 'defvar (intern (concat "irchat-"
					       (prin1-to-string sym)
					       "-hook"))
		       nil
		       (concat "*A hook that is executed when the IRC "
				 "message \"" (upcase (prin1-to-string sym))
				 "\" is received.
The hook function is called with two arguments, PREFIX and REST-OF-LINE.
It should return t if no further processing of the message is to be
carried out.")))))
	 irchat-msg-list)

(defvar irchat-current-channel nil
  "The channel you currently have joined.")

(defvar irchat-channel-indicator "No channel"
  "The current channel, \"pretty-printed.\"")

(defvar irchat-private-indicator nil
  "A string displayed in the mode line indicating that you are
currently engaged in a one-to-one conversation.")

(defvar irchat-polling nil
  "T when we are automatically polling the server.")

(defvar irchat-last-poll-minute ""
  "The minute when we last polled the server.")

(defvar irchat-freeze nil
  "If non-nil the Dialogue window will not be scrolled automatically to bring
new entries into view.")

(defvar irchat-privmsg-partner nil
  "The person who got your last private message.")

(defvar irchat-chat-partner nil
  "The person you are in a private conversation with.")

(defvar irchat-nick-alist nil
  "An alist containing the nicknames of users known to currently be on IRC.
Each element in the list is a list containing a nickname.")

(defvar irchat-channel-alist nil 
  "An alist containing the channels on IRC.  Each element in the list is 
a list containing a channel name.")

(defvar irchat-greet-author t
  "T until we notice that the author of IRCHAT is present, and send him
a greeting telling what version of IRCHAT this is.")

(defconst irchat-author-nickname "tml"
  "The nickname used by the author of IRCHAT.")

(defvar irchat-debug-buffer nil)

(defvar irchat-Command-buffer "*IRC Commands*")
(defvar irchat-Dialogue-buffer "*IRC Dialogue*")

(defvar irchat-Command-mode-map nil)
(defvar irchat-Dialogue-mode-map nil)

(defvar irchat-server-process nil)
(defvar irchat-status-message-string nil)

(put 'irchat-Command-mode 'mode-class 'special)
(put 'irchat-Dialogue-mode 'mode-class 'special)

(if irchat-Command-mode-map
    nil
  (setq irchat-Command-mode-map (make-sparse-keymap))
  (define-key irchat-Command-mode-map "\C-m" 'irchat-Command-enter-message)
  (define-key irchat-Command-mode-map "\C-j" 'irchat-Command-enter-message)
  (define-key irchat-Command-mode-map "\C-c\C-d" 'irchat-Command-debug)
  (define-key irchat-Command-mode-map "\C-c\C-l" 'irchat-Command-redisplay)
  (define-key irchat-Command-mode-map "\C-c\C-n" 'irchat-Command-names)
  (define-key irchat-Command-mode-map "\C-c\C-?" 'irchat-Command-scroll-down)
  (define-key irchat-Command-mode-map "\C-c " 'irchat-Command-scroll-up)
  (define-key irchat-Command-mode-map "\C-c2" 'irchat-Command-private-conversation)
  (define-key irchat-Command-mode-map "\C-ca" 'irchat-Command-away)
  (define-key irchat-Command-mode-map "\C-cb" 'irchat-Command-private-broadcast)
  (define-key irchat-Command-mode-map "\C-cc" 'irchat-Command-inline)
  (define-key irchat-Command-mode-map "\C-c\C-c" 'irchat-Command-reconnect)
  (define-key irchat-Command-mode-map "\C-cf" 'irchat-Command-finger)
  (define-key irchat-Command-mode-map "\C-c\C-f" 'irchat-Command-freeze)
  (define-key irchat-Command-mode-map "\C-ci" 'irchat-Command-invite)
  (define-key irchat-Command-mode-map "\C-cj" 'irchat-Command-join)
  (define-key irchat-Command-mode-map "\C-ck" 'irchat-Command-kill)
  (define-key irchat-Command-mode-map "\C-c\C-k" 'irchat-Command-kick)
  (define-key irchat-Command-mode-map "\C-cl" 'irchat-Command-list)
  (define-key irchat-Command-mode-map "\C-cm" 'irchat-Command-message)
  (define-key irchat-Command-mode-map "\C-c\C-m" 'irchat-Command-modec)
  (define-key irchat-Command-mode-map "\C-cn" 'irchat-Command-nickname)
  (define-key irchat-Command-mode-map "\C-cp" 'irchat-Command-mta-private)
  (define-key irchat-Command-mode-map "\C-cq" 'irchat-Command-quit)
  (define-key irchat-Command-mode-map "\C-ct" 'irchat-Command-topic)
  (define-key irchat-Command-mode-map "\C-c\C-t" 'irchat-Command-mta-topic)
  (define-key irchat-Command-mode-map "\C-cu" 'irchat-Command-lusers)
  (define-key irchat-Command-mode-map "\C-cw" 'irchat-Command-who)
  (define-key irchat-Command-mode-map "\C-cW" 'irchat-Command-wait)
  (define-key irchat-Command-mode-map "\C-c/" 'irchat-Command-generic)
  (define-key irchat-Command-mode-map "\C-[\C-i" 'irchat-Command-complete)
  (define-key irchat-Command-mode-map "/" 'irchat-Command-irc-compatible)
  )

(if irchat-Dialogue-mode-map
    nil
  (setq irchat-Dialogue-mode-map (make-keymap))
  (suppress-keymap irchat-Dialogue-mode-map)
  ;; mta@tut.fi wants these
  (define-key irchat-Dialogue-mode-map "\C-?" 'scroll-down)
  (define-key irchat-Dialogue-mode-map " " 'scroll-up)
  (define-key irchat-Dialogue-mode-map "$" 'end-of-buffer)
  (define-key irchat-Dialogue-mode-map "f" 'irchat-Dialogue-freeze)
  (define-key irchat-Dialogue-mode-map "o" 'other-window)
  (define-key irchat-Dialogue-mode-map "t" 'irchat-Dialogue-tag-line)
  )

(defun matching-substring (string arg)
  (substring string (match-beginning arg) (match-end arg)))

(defun irchat (&optional confirm)
  "Connect to the IRC server and start chatting.
If optional argument CONFIRM is non-nil, ask which IRC server to contact.
If already connected, just pop up the windows."
  (interactive "P")
  (if (irchat-server-opened)
      (irchat-configure-windows)
    (unwind-protect
	(progn
	  (switch-to-buffer (get-buffer-create irchat-Command-buffer))
	  (irchat-Command-mode)
	  (irchat-start-server confirm))
      (if (not (irchat-server-opened))
	  (irchat-Command-quit)
	;; IRC server is successfully open. 
	(setq mode-line-process (format " {%s}" irchat-server))
	(let ((buffer-read-only nil))
	  (erase-buffer)
	  (sit-for 0))
	(irchat-Dialogue-setup-buffer)
	(irchat-configure-windows)
	(run-hooks 'irchat-Startup-hook)
	(irchat-Command-describe-briefly)
	))))

(defun irchat-Command-mode ()
  "Major mode for IRCHAT.  Normal edit function are available.
Typing Return or Linefeed enters the current line in the dialogue.
The following special commands are available:
For a list of the generic commands type \\[irchat-Command-generic] ? RET.
\\{irchat-Command-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (setq irchat-nick-alist (list (list irchat-nickname)))
  (setq mode-line-modified "--- ")
  (setq major-mode 'irchat-Command-mode)
  (setq mode-name "IRCHAT Commands")
  (setq irchat-privmsg-partner nil)
  (setq irchat-private-indicator nil)
;; Nice to know if one's present or not, some people always leave the /away on
  (setq irchat-away-indicator "-")
  (setq irchat-freeze-indicator "-")
  (setq irchat-broadcast-indicator "")
;; The person(s) you are broadcasting to.
  (setq irchat-broadcast-partner nil)
  
  (setq mode-line-format
	'("--- IRCHAT: Commands " irchat-private-indicator
	  "{"irchat-channel-indicator "} "irchat-away-indicator irchat-freeze-indicator irchat-mta-topic-indicator "- " irchat-server " %-"))
  (use-local-map irchat-Command-mode-map)
  (run-hooks 'irchat-Command-mode-hook))

(defun irchat-Dialogue-mode ()
  "Major mode for displaying the IRC dialogue.
All normal editing commands are turned off.
Instead, these commands are available:
\\{irchat-Dialogue-mode-map}"
  (kill-all-local-variables)
  (setq mode-line-modified "--- ")
  (setq major-mode 'irchat-Dialogue-mode)
  (setq mode-name "IRCHAT Dialogue")
  (setq mode-line-format
	'("--- IRCHAT: Dialogue " irchat-broadcast-indicator
	  "{"irchat-channel-indicator "} "irchat-away-indicator irchat-freeze-indicator irchat-mta-topic-indicator "-" (-3 . "%p") "-%-"))
  (use-local-map irchat-Dialogue-mode-map)
  (buffer-flush-undo (current-buffer))
  (erase-buffer)
  (set-buffer irchat-Dialogue-buffer)
  (setq buffer-read-only t)
  (run-hooks 'irchat-Dialogue-mode-hook))

(defun irchat-startup-message ()
  (insert "\n\n\n\n
			   IRCHAT
        $Id: irchat.el,v 1.25 90/06/21 13:30:00 kmk Exp $


		 Internet Relay Chat interface for GNU Emacs

	If you have any trouble with this software, please let me
	know. I will fix your problems in the next release.

	Comments, suggestions, and bug fixes are welcome.

        This is a beta release. Report bugs to Kaizzu at 
        kmk@assari.tut.fi.

	Tor Lillqvist
	tml@hemuli.atk.vtt.fi"))

(defun irchat-Command-describe-briefly ()
  (message (substitute-command-keys "Type \\[describe-mode] for help")))

(defun irchat-Command-redisplay ()
  "Un-freezes and re-selects the Dialogue buffer in another window."
  (interactive)
  (setq irchat-freeze nil)
  (setq irchat-freeze-indicator "-")
  (if (get-buffer-window irchat-Dialogue-buffer)
     nil
    (if (one-window-p)
	(irchat-configure-windows)
      (display-buffer irchat-Dialogue-buffer)))
  (set-buffer irchat-Dialogue-buffer)
  (set-window-point (get-buffer-window irchat-Dialogue-buffer) (point-max)))

(defun irchat-Command-enter-message ()
  "Enter the current line as an entry in the IRC dialogue on the
current channel."
  (interactive)
  (let (message start stop)
    (beginning-of-line)
    (setq start (point))
    (end-of-line)
    (setq stop (point))
    (setq message (concat (buffer-substring start stop) nam-suffix))
    (next-line 1)
    (if irchat-chat-partner
	(progn
	  (irchat-send "PRIVMSG %s :%s" irchat-chat-partner message)
	  (irchat-own-message (format (format "%s %%s" irchat-format-string1) irchat-chat-partner message))))
    (if (not irchat-chat-partner)
	(progn
	  (irchat-send "MSG :%s" message)
	  (irchat-own-message (concat "> " message))))))

(defun irchat-Command-debug ()
  "Start debugging irchat."
  (interactive)
  (if irchat-debug-buffer
      (progn
	(setq irchat-debug-buffer nil)
	(other-window 1)
	(delete-window)
	(other-window -1))
    (if irchat-use-full-window
	(delete-other-windows))
    (irchat-configure-windows)
    (split-window-horizontally)
    (other-window 1)
    (setq irchat-debug-buffer (get-buffer-create "*IRC Debugging*"))
    (switch-to-buffer irchat-debug-buffer)
    (other-window -1)))

(defun irchat-Command-inline ()
  "Send current line as a message to the IRC server."
  (interactive)
  (let (message start stop)
    (beginning-of-line)
    (setq start (point))
    (end-of-line)
    (setq stop (point))
    (setq message (buffer-substring start stop))
    (newline)
    (irchat-send message)))

(defun irchat-Command-join (channel)
  "Join a channel."
  (interactive (let (channel (completion-ignore-case t))
		 (setq channel (completing-read "Join channel: " irchat-channel-alist
						'(lambda (s) t) nil nil))
		 (list channel)))
  (irchat-send "CHANNEL %s" channel))

(defun irchat-Command-kill (nickname)
  "Ignore messages from this user.  If already ignoring him/her, toggle."
  (interactive (let (nickname (completion-ignore-case t))
		 (setq nickname (completing-read "Ignore nickname: " irchat-nick-alist
						 '(lambda (s) t) nil nil))
		 (list nickname)))
  (if (memq (intern nickname) irchat-kill-nickname)
      (setq irchat-kill-nickname (delq (intern nickname) irchat-kill-nickname))
    (setq irchat-kill-nickname (cons (intern nickname) irchat-kill-nickname))))

(defun irchat-Command-kick (nickname)
  "Kick this user out."
  (interactive (let (nickname (completion-ignore-case t))
		 (setq nickname (completing-read "Kick out nickname: " irchat-nick-alist
						 '(lambda (s) t) nil nil))
		 (list nickname)))
	       (irchat-send "KICK %s" nickname))

(defun irchat-Command-list (&optional channel)
  "List the channels and their topics, or if numeric ARG, topic of current
   channel. If you enter only Control-U as argument, list the current channel."
  (interactive "P")
  (if (not channel)
      (irchat-send "LIST")
    (irchat-send "LIST %s" (if (listp current-prefix-arg)
			       irchat-current-channel
			     (int-to-string channel)))))

(defun irchat-Command-lusers ()
  "List the number of users and servers"
  (interactive)
  (irchat-send "LUSERS"))

(defun irchat-Command-modec ()
  "Send a MODE command"
  (interactive)
  (irchat-send "MODE %s %s" irchat-current-channel (read-string "Mode for this channel: ")))


(defun irchat-Command-message (nick message)
  "Send a private message to another user."
  (interactive (let (nick (completion-ignore-case t))
		 (setq nick (completing-read "Private message to: " irchat-nick-alist
					     '(lambda (s) t) nil irchat-privmsg-partner))
		 (list nick (read-string (format "Private message to %s: " nick)))))
  (setq irchat-privmsg-partner nick)
  (irchat-send "PRIVMSG %s :%s" nick message)
  (irchat-own-message (format (format "%s %%s" irchat-format-string1) nick message)))

;; Added at mta@tut.fi's request...

(defun irchat-Command-mta-private ()
  "Send a private message (current line) to another user."
  (interactive)
  (let ((completion-ignore-case t) message start stop)
    (setq irchat-privmsg-partner
	  (completing-read "To whom: " irchat-nick-alist
			   '(lambda (s) t) 
			   nil irchat-privmsg-partner))
    (beginning-of-line)
    (setq start (point))
    (end-of-line)
    (setq stop (point))
    (setq message (irchat-clean-string
		   (buffer-substring start stop)))
    (next-line 1)
    (irchat-send "PRIVMSG %s :%s" irchat-privmsg-partner message)
    (irchat-own-message (format (format "%s %%s" irchat-format-string1) irchat-privmsg-partner message))))

(defun irchat-Command-private-conversation (nick)
  "Enter private conversation mode.  Each line you type in the Command
buffer gets sent to your partner only."
  (interactive (let (nick (completion-ignore-case t))
		 (setq nick (completing-read "With whom do you wish to chat privately: " irchat-nick-alist
					     '(lambda (s) t) nil irchat-privmsg-partner))
		 (list nick)))
  (if (string= nick "")
      (progn
	(message "No longer in private conversation mode")
	(setq irchat-chat-partner nil)
	(setq irchat-private-indicator ""))
    (setq irchat-chat-partner nick)
    (setq irchat-private-indicator (format "(Chatting with %s) " nick))))

(defun irchat-Command-names (&optional channel)
  "List the nicknames of the current IRC users per channel.
With an Control-U as argument, only the current channel is listed."
  (interactive "P")
  (if (not channel)
      (irchat-send "NAMES")
    (irchat-send "NAMES %s" (if (listp current-prefix-arg)
				irchat-current-channel
			      (int-to-string channel)))))

(defun irchat-Command-nickname (nick)
  "Set your nickname."
  (interactive "sEnter your nickname: ")
  (setq irchat-nickname nick)
  (irchat-send "NICK %s" nick))

(defun irchat-Command-who (&optional channel)
  "List the users currently on each channel, or if numeric ARG, on that channel.
If you enter only Control-U as argument, list the current channel."
  (interactive "P")
  (if (not channel)
      (irchat-send "WHO")
    (irchat-send "WHO %s" (if (listp current-prefix-arg)
			      irchat-current-channel
			    (int-to-string channel)))))

(defun irchat-Command-wait (nick &optional greeting)
  "Wait for NICK to enter IRC.  When this person appears, you will
be informed.
If the optional argument GREETING is non-nil, it should be a string to send
NICK upon entering."
  (interactive (progn (setq nick (read-string "Wait for: "))
		      (setq greeting (read-string (format "Message to send %s upon entering: " nick)))
		      (if (string= greeting "")
			  (setq greeting nil))
		      (list nick greeting)))
  (put (intern nick) 'irchat-waited-for t)
  (if greeting (put (intern nick) 'irchat-greeting greeting)))

(defun irchat-Command-finger (nick)
  "Get information about a specific user."
  (interactive (let (nick (completion-ignore-case t))
		 (setq nick (completing-read "Finger whom: " irchat-nick-alist
					     '(lambda (s) t) nil nil))
		 (list nick)))
  (irchat-send "WHOIS %s" nick))

;; Added by nam

(defun irchat-Command-topic (topic)
  "Change topic of channel."
  (interactive "sTopic: ")
  (irchat-send "TOPIC %s" topic))

(defun irchat-Command-invite (nick)
  "Invite user to channel."
  (interactive (let (nick (completion-ignore-case t))
		 (setq nick (completing-read "Invite whom: " irchat-nick-alist
					     '(lambda (s) t) nil nil))
		 (list nick)))
  (irchat-send "INVITE %s" nick))

(defun irchat-Command-away (awaymsg)
  "Mark/unmark yourself as being away."
  (interactive "sAway message: ")
  (irchat-send "AWAY %s" awaymsg))

(defun irchat-Command-private-broadcast (nick)
  "Enter private broadcast mode.  Each line you type in the Command
buffer and each line recieved from your channel gets sent to your partner(s)."
  (interactive (let (nick (completion-ignore-case t))
		 (setq nick (completing-read "Who do you wish to broadcast to: " irchat-nick-alist
					     '(lambda (s) t) nil irchat-broadcast-partner))
		 (list nick)))
  (if (string= nick "")
      (progn
	(message "No longer in broadcast mode")
	(setq irchat-broadcast-partner nil)
	(setq irchat-broadcast-indicator ""))
    (setq irchat-broadcast-partner nick)
    (setq irchat-broadcast-indicator (format "(BCasting to %s) " nick))))

(defun super-insert (text-to-insert)
  "Kludge needed for broadcast mode (and irchat-ignore-changes)."
  (if (not irchat-ignore-changes)
      (progn
	(insert text-to-insert)
	(if irchat-broadcast-partner
	    (irchat-send "PRIVMSG %s :%s" irchat-broadcast-partner text-to-insert)))))

(defun irchat-Command-mta-topic ()
  "Set/unset mta-topic-active."
  (interactive)
  (if mta-topic-active
      (setq irchat-mta-topic-indicator "-")
    (setq irchat-mta-topic-indicator "T"))
  (switch-to-buffer (current-buffer))
  (setq mta-topic-active (not mta-topic-active)))

;; this needs work!! (HUGE kludge)
;; worked on by Kaizzu. May still die in beginning and end of buffer.

(defun irchat-Command-scroll-down ()
  "Scroll Dialogue-buffer down from Command-buffer."
  (interactive)
  (pop-to-buffer irchat-Dialogue-buffer)
  (scroll-down)
  (pop-to-buffer irchat-Command-buffer))

(defun irchat-Command-scroll-up ()
  "Scroll Dialogue-buffer up from Command-buffer."
  (interactive)
  (pop-to-buffer irchat-Dialogue-buffer)
  (scroll-up)
  (pop-to-buffer irchat-Command-buffer))

;; end of nam additions

(defun irchat-Command-freeze ()
  "Toggle the automatic scrolling of the Dialogue window."
  (interactive)
  (if irchat-freeze
      (setq irchat-freeze-indicator "-")
    (setq irchat-freeze-indicator "F"))
  (switch-to-buffer (current-buffer))
  (setq irchat-freeze (not irchat-freeze)))

(defun irchat-Command-quit ()
  "Quit IRCHAT."
  (interactive)
  (if (or (not (irchat-server-opened))
	  (y-or-n-p "Quit IRCHAT? "))
      (progn
	(message "")
	(if (get-buffer-process irchat-server-buffer)
	    (irchat-send "QUIT"))
	(irchat-clear-system)
	(if irchat-use-full-window
	    (delete-other-windows))
	(irchat-close-server)
	(run-hooks 'irchat-Exit-hook))))

(defun irchat-Command-generic (message)
  "Enter a generic IRC message, which is sent to the server.
 A ? lists the useful generic messages."
  (interactive "sIRC Command: ")
  (if (string= message "?")
      (with-output-to-temp-buffer "*IRC Help*"
	(princ "The following generic IRC messages may be of interest to you:
TOPIC <new topic>		set the topic of your channel
INVITE <nickname>		invite another user to join your channel
LINKS				lists the currently reachable IRC servers
SUMMON <user@host>		invites an user not currently in IRC
USERS <host>			lists the users on a host
AWAY <reason>			marks you as not really actively using IRC
				(an empty reason clears it)
WALL <message>			send to everyone on IRC
NAMES <channel>			lists users per channel
")
	(message (substitute-command-keys "Type \\[irchat-Command-redisplay] to continue")))
  (irchat-send "%s" message)))

(defun irchat-Command-irc-compatible ()
  "If entered at column 0, allows you to enter a generic IRC message to
be sent to the server.  For a list of messages, see irchat-Command-generic."
  (interactive)
  (if (eq (current-column) 0)
      (call-interactively (function irchat-Command-generic))
    (self-insert-command 1)))

(defun irchat-configure-windows ()
  "Configure Command mode and Dialogue mode windows.
One is for entering commands and text, the other displays the IRC dialogue."
  (if (or (one-window-p t)
	  (null (get-buffer-window irchat-Command-buffer))
	  (null (get-buffer-window irchat-Dialogue-buffer)))
      (progn
	(if irchat-command-window-on-top
	    (progn
	      (switch-to-buffer irchat-Command-buffer)
	      (if irchat-use-full-window
		  (delete-other-windows))
	      (split-window-vertically
	       (max window-min-height 2))
	      (other-window 1)
	      (switch-to-buffer irchat-Dialogue-buffer)
	      (other-window 1))
	  ;; mta@tut.fi wants it like this
	  (switch-to-buffer irchat-Dialogue-buffer)
	  (if irchat-use-full-window
	      (delete-other-windows))
	  (split-window-vertically
	   (- (window-height) (max window-min-height 4)))
	  (other-window 1)
	  (switch-to-buffer irchat-Command-buffer))
	)))

(fset 'irchat-Dialogue-freeze 'irchat-Command-freeze)

(defun irchat-Dialogue-tag-line ()
  "Move current line to kill-ring."
  (interactive)
  (save-excursion
    (let (start)
      (beginning-of-line)
      (setq start (point))
      (end-of-line)
      (kill-ring-save start (point)))))

(defun irchat-Dialogue-setup-buffer ()
  "Initialize Dialogue mode buffer."
  (or (get-buffer irchat-Dialogue-buffer)
      (save-excursion
	(set-buffer (get-buffer-create irchat-Dialogue-buffer))
	(irchat-Dialogue-mode))))

(defun irchat-clear-system ()
  "Clear all IRCHAT variables and buffers."
  (interactive)
  (if (get-buffer irchat-Command-buffer)
      (kill-buffer irchat-Command-buffer))
  (if (get-buffer irchat-Dialogue-buffer)
      (kill-buffer irchat-Dialogue-buffer))
  (setq irchat-channel-indicator "No channel"))

(defun irchat-start-server (&optional confirm)
  "Open network stream to remote chat server.
If optional argument CONFIRM is non-nil, ask you the host that the server
is running on even if it is defined."
  (if (irchat-server-opened)
      ;; Stream is already opened.
      nil
    ;; Open IRC server.
    (if (or
	 (and confirm
	      (not (eq confirm 42)))
	 (null irchat-server))
	(setq irchat-server
	      (read-string "IRC server: " irchat-server)))
    (if (and confirm
	     (not (eq confirm 42))
	     irchat-ask-for-nickname)
	(setq irchat-nickname
	      (read-string "Enter your nickname: " irchat-nickname)))
    (if (eq confirm 42)
	(setq irchat-nickname (concat irchat-nickname "_")))
    ;; If no server name is given, local host is assumed.
    (if (string-equal irchat-server "")
	(setq irchat-server (system-name)))
    (message "Connecting to IRC server on %s..." irchat-server)
    (cond ((irchat-open-server irchat-server irchat-service))
	  ((and (stringp irchat-status-message-string)
		(> (length irchat-status-message-string) 0))
	   ;; Show valuable message if available.
	   (error irchat-status-message-string))
	  (t (error "Cannot open IRC server on %s" irchat-server)))
    ))

(defun irchat-open-server (host &optional service)
  "Open chat server on HOST.
If HOST is nil, use value of environment variable \"IRCSERVER\".
If optional argument SERVICE is non-nil, open by the service name."
  (let ((host (or host (getenv "IRCSERVER")))
	(status nil))
    (setq irchat-status-message-string "")
    (cond ((and host (irchat-open-server-internal host service))
	   (irchat-send "PING %s" host)
	   (if (setq status (irchat-wait-for-response "^PONG.*"))
	       (progn
		 (set-process-sentinel irchat-server-process
				       'irchat-sentinel)
		 (set-process-filter irchat-server-process
				     'irchat-filter)
		 (irchat-send "USER %s %s %s %s" 
			      (user-real-login-name)
			      irchat-system-fqdname 
			      irchat-server
			      (or (getenv "IRCNAME") 
				  (getenv "NAME") 
				  (user-full-name)))
		 (irchat-send "NICK %s" irchat-nickname))
	     ;; We have to close connection here, since the function
	     ;;  `irchat-server-opened' may return incorrect status.
	     (irchat-close-server-internal)
	     ))
	  ((null host)
	   (setq irchat-status-message-string "IRC server is not specified."))
	  )
    status
    ))

(defun irchat-close-server ()
  "Close chat server."
  (unwind-protect
      (progn
	;; Un-set default sentinel function before closing connection.
	(and irchat-server-process
	     (eq 'irchat-sentinel
		 (process-sentinel irchat-server-process))
	     (set-process-sentinel irchat-server-process nil))
	;; We cannot send QUIT command unless the process is running.
	(if (irchat-server-opened)
	    (irchat-send "QUIT"))
	)
    (irchat-close-server-internal)
    ))

(defun irchat-server-opened ()
  "Return server process status, T or NIL.
If the stream is opened, return T, otherwise return NIL."
  (and irchat-server-process
       (memq (process-status irchat-server-process) '(open run))))

(defun irchat-open-server-internal (host &optional service)
  "Open connection to chat server on HOST by SERVICE (default is irc)."
  (condition-case err 
      (save-excursion
	;; Initialize communication buffer.
	(setq irchat-server-buffer (get-buffer-create " *IRC*"))
	(set-buffer irchat-server-buffer)
	(kill-all-local-variables)
	(buffer-flush-undo (current-buffer))
	(erase-buffer)
	(setq irchat-server-process
	      (open-network-stream "IRC" (current-buffer)
				   host (or service "irc")))
	(setq irchat-server-name host)
	(run-hooks 'irchat-server-hook)
	;; Return the server process.
	irchat-server-process)
    (error (message (car (cdr err)))
	   nil)))

(defun irchat-close-server-internal ()
  "Close connection to chat server."
  (if irchat-server-process
      (delete-process irchat-server-process))
  (if irchat-server-buffer
      (kill-buffer irchat-server-buffer))
  (setq irchat-server-buffer nil)
  (setq irchat-server-process nil))

(defun irchat-wait-for-response (regexp)
  "Wait for server response which matches REGEXP."
  (save-excursion
    (let ((status t)
	  (wait t))
      (set-buffer irchat-server-buffer)
      (irchat-accept-response)
      (while wait
	(goto-char (point-min))
	(cond ((looking-at "ERROR")
	       (setq status nil)
	       (setq wait nil))
	      ((looking-at ".")
	       (setq wait nil))
	      (t (irchat-accept-response))
	      ))
      ;; Save status message.
      (end-of-line)
      (setq irchat-status-message-string
	    (buffer-substring (point-min) (point)))
      (if status
	  (progn
	    (setq wait t)
	    (while wait
	      (goto-char (point-max))
	      (forward-line -1)		;(beginning-of-line)
	      (if (looking-at regexp)
		  (setq wait nil)
		(message "IRCHAT: Reading...")
		(irchat-accept-response)
		(message "")
		))
	    ;; Successfully received server response.
	    t
	    ))
      )))

(defun irchat-accept-response ()
  "Read response of server.
It is well-known that the communication speed will be much improved by
defining this function as macro."
  ;; To deal with server process exiting before
  ;;  accept-process-output is called.
  ;; Suggested by Jason Venner <jason@violet.berkeley.edu>.
  ;; This is a copy of `nntp-default-sentinel'.
  (or (memq (process-status irchat-server-process) '(open run))
      (if (not irchat-reconnect-automagic)
	  (error "IRCHAT: Connection closed.")
	(if irchat-grow-tail
	    (irchat irchat-current-channel)
	  (irchat))))
  (condition-case errorcode
      (accept-process-output irchat-server-process)
    (error
     (cond ((string-equal "select error: Invalid argument" (nth 1 errorcode))
	    ;; Ignore select error.
	    nil
	    )
	   (t
	    (signal (car errorcode) (cdr errorcode))))
     )
    ))

(defun irchat-sentinel (proc status)
  "Sentinel function for IRC server process."
  (if (and irchat-server-process
	   (not (irchat-server-opened)))
      (if (not irchat-reconnect-automagic)
	  (error "IRCHAT: Connection closed.")
	(if irchat-grow-tail
	    (irchat 42) ;;; This is ugly and I know it. (Kaizzu)
	  (irchat)))))

(defun irchat-filter (process output)
  "Filter function for IRC server process."
  (let ((obuf (current-buffer))
	(data (match-data))
	bol)
    (if irchat-debug-buffer
	(progn
	  (set-buffer irchat-debug-buffer)
	  (goto-char (point-max))
	  (insert output)
	  (irchat-scroll-if-visible (get-buffer-window irchat-debug-buffer))))
    (set-buffer (process-buffer process))
    (goto-char (point-max))
    (insert output)
    (goto-char (point-min))
    (while (re-search-forward "\n\n" (point-max) t)
      (delete-char -1)) ;;; This hack (from mta) for 2.4
    (goto-char (point-min))

    (if (string-match "\n" output)
	(irchat-handle-message))
    (set-buffer obuf)
    (store-match-data data)))

(defun irchat-handle-message ()
  "Called when we have at least one line of output from the IRC server."
  (let ((obuf (current-buffer)) beg end prefix message rest-of-line)
    (while (looking-at "\\(:[^ ]*\\)? *\\([^ \n]+\\) *:?\\(.*\\)\n")
      (setq beg (match-beginning 0)
	    end (match-end 0))
      (setq prefix (if (match-beginning 1)
		       (buffer-substring (1+ (match-beginning 1))
					 (match-end 1))))
      (setq rest-of-line
	    (buffer-substring (match-beginning 3) (match-end 3)))
      (setq message (downcase (buffer-substring
			       (match-beginning 2)
			       (match-end 2))))
      (set-buffer irchat-Dialogue-buffer)
      (if irchat-freeze
	  (save-excursion
	    (irchat-handle-message-2 prefix message rest-of-line))
	(irchat-handle-message-2 prefix message rest-of-line))
      (set-buffer obuf)
      (delete-region beg end))))

(defun irchat-handle-message-2 (prefix message rest-of-line)
  "Helper function for irchat-handle-message."
  (let (buffer-read-only hook fun)
    (goto-char (point-max))
    (if (and (boundp (setq hook
			   (intern (concat "irchat-" message "-hook"))))
	     (eval hook)
	     (eq (eval (list hook prefix rest-of-line)) t))
	;; If we have a hook, and it returns T, do nothing more
	nil
      ;; else call the handler
      (if (fboundp (setq fun (intern
			      (concat "irchat-handle-" message "-msg"))))
	  (progn
	    (eval (list fun prefix rest-of-line))
	    (if (not irchat-freeze)
		(irchat-scroll-if-visible
		 (get-buffer-window (current-buffer)))))
	(message "IRCHAT: Unknown IRC message \":%s %s %s\"" prefix (upcase message) rest-of-line)
	(insert (format "MESSAGE: %s, %s, %s" prefix message rest-of-line))
	(newline)))))

;; As the server PINGs us regularly, we can be sure that we will 
;; have the opportunity to poll it with a NAMES as often.
;; We do this so that we can keep the irchat-nick-alist up-to-date.
;; We send a PING after the NAMES so that we notice when the final
;; NAMREPLY has come.

(defun irchat-maybe-poll ()
  "At regular intervals, we poll the IRC server, 
checking which users are present."
  (let ((now (substring (current-time-string) 14 16)))
    (if (string= now irchat-last-poll-minute)
	nil
      (setq irchat-last-poll-minute now)
      (setq irchat-polling t)
      (setq irchat-nick-alist nil)
      (setq irchat-channel-alist nil)
      (irchat-send "NAMES")
      (irchat-send "PING %s" (system-name)))))

(defun irchat-handle-error-msg (prefix rest)
  (message "IRC error: %s" rest))

(defun irchat-handle-channel-msg (prefix rest)
;;; changed by Kaizzu to understand strings in channel names
  (if (string= prefix irchat-nickname)
      (progn
	(setq irchat-current-channel rest)
	(setq irchat-channel-indicator
	      (if (string= rest "0")
		  "No channel"
		(format "Channel %s" rest)))))
  (if (string= rest "0")
      (super-insert (format "%s%s has left this channel\n" irchat-change-prefix prefix))
    (super-insert (format "%s%s has joined this channel\n" irchat-change-prefix prefix))
    (irchat-change-nick-of prefix prefix)))

(defun irchat-handle-linreply-msg (prefix rest)
  (if (string-match "\\([^ ]+\\) +\\(.*\\)" rest)
      (progn
	(insert (matching-substring rest 1))
	(indent-to-column 30)
	(insert (matching-substring rest 2))
	(newline))
    (message "IRCHAT: Strange LINREPLY")))

(defun irchat-handle-msg-msg (prefix rest)
  (if (or (and prefix
	       (or (memq (intern prefix) irchat-kill-nickname)))
	  (and (not prefix)
	       (string= "> " rest)))
      nil
    (if prefix (insert (format "<%s> " prefix)))
    (insert rest)
    (setq tmpprefix prefix)
    (if (and irchat-broadcast-partner (not irchat-chat-partner))
	(progn
	  (if (eq prefix nil)
	      (setq prefix "")
	    (setq prefix (concat "<" prefix "> ")))
	  (if (and (string= prefix "")
		   (string-match (format "^%s.*" (format irchat-format-string1)) rest))
	      nil
	    (irchat-send "PRIVMSG %s :%s%s" irchat-broadcast-partner prefix rest))))
    (setq prefix tmpprefix)
    (if (and (string-match "\007" rest) irchat-beep-on-bells)
	(progn
	  (if (not (get-buffer-window irchat-Dialogue-buffer))
	      (progn
		(beep t)
		(message "IRCHAT: %s is trying to get attention" prefix)))
	  (if (eq irchat-beep-on-bells 'always)
	      (beep t))))
    (newline)))

;; This needs work

(defun irchat-handle-namreply-msg (prefix rest)
  "Handle the NAMREPLY message.   If we are just polling the server,
don't display anything.  Check if someone we are waiting for has entered."
  (if (string-match "[=*] \\([*0-9+][^ ]*\\) \\(.*\\)" rest)
      (let ((chnl (matching-substring rest 1)))
	(if irchat-polling
	    nil
	  (insert (format "%9s: %s" (if (string= chnl "*") "Priv" chnl)
			  (matching-substring rest 2)))
	  (newline))
	(irchat-scan-channels chnl)
	(irchat-scan-nicklist (matching-substring rest 2)))
    (message "IRCHAT: Strange NAMREPLY")))

(defun irchat-handle-nick-msg (prefix rest)
  (irchat-change-nick-of prefix rest)
  (if (string= prefix irchat-nickname)
      (setq irchat-nickname rest))
  (super-insert (format "%s%s is now known as %s" irchat-change-prefix prefix rest))
  (newline))

;;; Kaizzu added removing of extraneous away-NOTICES 90-04-16 14:30:00.
(defun irchat-handle-notice-msg (prefix rest)
  (if (or (not irchat-ignore-extra-notices)
	  (not prefix)
	  (not (string-match "as being away" rest)))
      (progn
	(if prefix (insert (format "<%s> " prefix)))
	(if (string-match "[^ ]+ :\\(.*\\)" rest)
	    (insert (matching-substring rest 1))
	  (insert rest))
	(newline))))

(defun irchat-handle-ping-msg (prefix rest)
  (irchat-send "PONG yourself")
  (irchat-maybe-poll))

(defun irchat-handle-pong-msg (prefix rest)
  (if irchat-polling
      (setq irchat-polling nil)))

(defun irchat-handle-privmsg-msg (prefix rest)
  (if (and prefix
	   (memq (intern prefix) irchat-kill-nickname))
      nil
    (if (and (string-match "\007" rest) irchat-beep-on-bells)
	(beep t))
    (string-match "\\([^ ]+\\) :\\(.*\\)" rest)
    (setq temp (matching-substring rest 2))
    (if prefix
	(if (string-match "^[\-0-9]" (matching-substring rest 1))
	    (insert (format (format "%s " irchat-format-string3)
			    prefix))
	  (insert (format (format "%s " irchat-format-string2) prefix))))
    (insert temp)
    (if (and prefix (not (get-buffer-window (current-buffer))))
	(message "IRCHAT: A private message has arrived from %s" prefix))
    (if (string= irchat-away-indicator "A")
	(insert (format " (%s)" (current-time-string))))
    (newline)))

(defun irchat-handle-wall-msg (prefix rest)
  "Handle the WALL message."
  (insert (format "*broadcast%s* %s" (if prefix (concat " from " prefix) "")
		  rest))
  (newline))

(defun irchat-handle-wallops-msg (prefix rest)
  "Handle the WALLOPS message."
  (insert (format "*wallops%s* %s" (if prefix (concat " from " prefix) "")
		  rest))
  (newline))

(defun irchat-handle-whoreply-msg (prefix rest)
  "Handle the WHOREPLY message.
Check if someone we are waiting for has entered."
  (if (string-match "\\([*+0-9][^ ]*\\) \\([^ ]+\\) \\([^ ]+\\) \\([^ ]+\\) \\([^ ]+\\) \\([^ ]+\\) +:\\(.*\\)" rest)
      (let ((chnl (matching-substring rest 1))
	    (nick (matching-substring rest 5))
	    (oper (matching-substring rest 6)))
	(insert (format "%s %5s %8s <%s@%s>"
			oper ;; Kaizzu 06/03/90
			(if (string= chnl "*") "Chnl"
			  (if (string= chnl "0")
			      "Priv"
			    chnl))
			nick
			(matching-substring rest 2)
			(irchat-clean-hostname (matching-substring rest 3))))
	(indent-to-column 50 1)
	(insert (matching-substring rest 7))
	(newline))
    (message "IRCHAT: Strange WHOREPLY")))

(defun irchat-handle-quit-msg (prefix rest)
  "Handle the QUIT message."
  (super-insert (format "%s%s has left IRC" irchat-change-prefix prefix))
  (newline)
  (irchat-change-nick-of prefix nil))

(defun irchat-handle-topic-msg (prefix rest)
  "Handle the TOPIC message."
  (super-insert (format "%sNew topic of this channel set by %s: %s" irchat-change-prefix prefix rest))
  (newline)
  (cond
   ((equal rest mta-own-topic) nil)
   ((and (equal prefix irchat-nickname)
	 mta-snap-topic)
    (setq mta-own-topic rest))
   (mta-topic-active (irchat-send "TOPIC %s" mta-own-topic))))

(defun irchat-handle-mode-msg (prefix rest)
  "Handle the MODE message."
  (insert (format "%sNew mode set by %s: %s" irchat-change-prefix prefix rest))
  (newline))

(defun irchat-handle-kick-msg (prefix rest)
  "Handle the KICK message."
  (insert (format "%s%s has kicked %s out" irchat-change-prefix prefix rest))
  (newline))

(defun irchat-handle-invite-msg (prefix rest)
  (string-match "\\([-*0-9]+\\)" rest)
  (insert (format "*** %s invites you to channel %s" prefix (matching-substring rest 1)))
  (newline))

(defun irchat-handle-kill-msg (prefix rest)
  (string-match "[^ ]+ +:\\(.*\\)" rest)
  (let ((path (matching-substring rest 1)))
    (insert (format "*** IRCHAT: You were killed by %s. Path: %s. RIP" prefix path))
    (newline)
    (setq irchat-channel-indicator "No channel")))

;; The newer versions of ircd partly use numeric reply codes a'la SMTP
;; These functions provided by mta@tut.fi.  Some cosmetic changes necessary.
;; kmk@assari.tut.fi sorted these 90/06/21 while adding 2.5 ones. The
;; order is now the one in numeric.h.

(defun irchat-handle-401-msg (prefix rest) ;;; ERR_NOSUCHNICK
  (if (string-match "[^ ]+ \\([^ ]+\\) +:\\(.*\\)" rest)
      (let ((name (matching-substring rest 1))
	    (error (matching-substring rest 2)))
	(message "IRCHAT: %s: %s" name error)
	(beep))
    (message "IRCHAT: Strange 401 reply")))

(defun irchat-handle-402-msg (prefix rest) ;;; ERR_NOSUCHSERVER
  (if (string-match "\\([^ ]+\\) +\\([^ ]+\\) +:\\(.*\\)" rest)
      (let ((host (matching-substring rest 2))
	    (msg (matching-substring rest 3)))
	(message (format "IRCHAT: %s: %s" host msg)))
    (message "OPER: Strange 402 message")))

(defun irchat-handle-403-msg (prefix rest) ;;; ERR_NOSUCHCHANNEL
  (message "IRCHAT: Erroneous channel name."))

(defun irchat-handle-404-msg (prefix rest) ;;; ERR_CANNOTSENDTOCHAN
  (message "IRCHAT: cannot write to a moderated or privmsg restricted channel"))

(defun irchat-handle-411-msg (prefix rest) ;;; ERR_NORECIPIENT
  (insert "*** IRCHAT: No recipient given")
  (newline))

(defun irchat-handle-412-msg (prefix rest) ;;; ERR_NOTEXTTOSEND
  (message "IRCHAT: no text to send"))

(defun irchat-handle-421-msg (prefix rest) ;;; ERR_UNKNOWNCOMMAND
  (if (string-match "\\([^ ]+\\) +\\([^ ]+\\)" rest)
      (let ((comm (matching-substring rest 2)))
	(insert (format "*** IRCHAT: Unknown command: %s" comm))
	(newline))
    (message "Strange 421 message")))

(defun irchat-handle-431-msg (prefix rest) ;;; ERR_NONICKNAMEGIVEN
  (insert "*** IRCHAT: No nickname given")
  (newline))

(defun irchat-handle-432-msg (prefix rest) ;;; ERR_ERRONEUSNICKNAME
  (insert "*** IRCHAT: Erroneus nickname")
  (newline))

(defun irchat-handle-433-msg (prefix rest) ;;; ERR_NICKNAMEINUSE
  "Handle the 433 reply (nickname already in use)"
  (save-excursion
    (set-buffer irchat-Command-buffer)
    (beep)
    (string-match "\\([^ ]+\\) +\\([^ ]+\\) +:\\(.*\\)" rest)
    (let ((nick (matching-substring rest 2)))
      (message "IRCHAT: Nickname %s already in use.  Choose a new one with %s."
	       nick
	       (substitute-command-keys "\\[irchat-Command-nickname]")))))

(defun irchat-handle-441-msg (prefix rest) ;;; ERR_USERNOTINCHANNEL
  (insert "*** IRCHAT: You have not joined any channel")
  (newline))

(defun irchat-handle-442-msg (prefix rest) ;;; ERR_NOTONCHANNEL
  (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
  (let ((msg (matching-substring rest 2)))
    (insert (format "*** %s" msg))
    (newline)))

(defun irchat-handle-451-msg (prefix rest) ;;; ERR_NOTREGISTERED
  (interactive)
  (message "IRCHAT: You have not registered yet."))

(defun irchat-handle-461-msg (prefix rest) ;;; ERR_NEEDMOREPARAMS
  (if (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
      (let ((message (matching-substring rest 2)))
	(message (format "IRCHAT: %s" message)))
    (message "IRCHAT: Strange 461 reply")))

(defun irchat-handle-462-msg (prefix rest) ;;; ERR_ALREADYREGISTRED
  (message "IRCHAT: Already registered"))

(defun irchat-handle-463-msg (prefix rest) ;;; ERR_NOPERMFORHOST
  (message "IRCHAT: This IRC server can't talk to you. Goodbye."))

(defun irchat-handle-464-msg (prefix rest) ;;; ERR_PASSWDMISMATCH
  (message "IRCHAT: Bad password"))

(defun irchat-handle-465-msg (prefix rest) ;;; ERR_YOUREBANNEDCREEP
  (message "IRCHAT: You're banned from IRC. Sorry."))

(defun irchat-handle-466-msg (prefix rest) ;;; ERR_YOUWILLBEBANNED
  (insert (format "*** You will be banned from IRC within %s minutes" rest)))

(defun irchat-handle-471-msg (prefix rest) ;;; ERR_CHANNELISFULL
  (insert "*** IRCHAT: Sorry, channel is full")
  (newline))

(defun irchat-handle-472-msg (prefix rest) ;;; ERR_UNKNOWNMODE
  (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
  (let ((msg (matching-substring rest 2)))
    (insert (format "*** %s" msg))
    (newline)))

(defun irchat-handle-473-msg (prefix rest) ;;; ERR_INVITEONLYCHAN
  (insert "That channel is invite only and you have not been invited")
  (newline))

(defun irchat-handle-481-msg (prefix rest) ;;; ERR_NOPRIVILEGES
  (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
  (let ((msg (matching-substring rest 2)))
    (insert (format "*** IRCHAT: %s" msg))
    (newline)))

(defun irchat-handle-491-msg (prefix rest) ;;; ERR_NOOPERHOST
  (message "IRCHAT: You can't be OPER here. Sorry."))

(defun irchat-handle-301-msg (prefix rest) ;;; RPL_AWAY
  (if (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
      (let ((who (matching-substring rest 1))
	    (iswhat (matching-substring rest 2)))
	(insert 
	 (format "*** %s is marked as being AWAY, but left the message:"
		 who))
	(newline)
	(insert iswhat)
	(newline))
    (insert "IRCHAT: Strange 301 reply")))

(defun irchat-handle-311-msg (prefix rest) ;;; RPL_WHOISUSER
  "Handle the 311 reply (from WHOIS)."
  (if (string-match "[^ ]+ \\([^ ]+\\) \\([^ ]+\\) \\([^ ]+\\) \\([^ ]+\\) :\\(.*\\)" rest)
      (let ((nick (matching-substring rest 1))
	    (username (matching-substring rest 2))
	    (machine (matching-substring rest 3))
	    (channel (matching-substring rest 4))
	    (realname (matching-substring rest 5)))
	(insert 
	 (format "%s [%s] is %s (%s) at %s"
		 nick
		 (if (string= channel "*") "Priv" channel)
		 username
		 realname
		 machine))
	(newline))
    (insert "IRCHAT: Strange 311 reply")))

(defun irchat-handle-312-msg (prefix rest) ;;; RPL_WHOISSERVER
  "Handle the 312 reply (from WHOIS)."
  (if (string-match "\\([^ ]+\\) :\\(.*\\)" rest)
      (let ((server (matching-substring rest 1))
	    (real (matching-substring rest 2)))
	(insert 
	 (format "on via server %s (%s)"
		 server
		 real))
	(newline))
    (insert "IRCHAT: Strange 312 reply")))

(defun irchat-handle-313-msg (prefix rest) ;;; RPL_WHOISOPERATOR
  (if (string-match "\\([^ ]+\\) :\\(.*\\)" rest)
      (let ((who (matching-substring rest 1))
	    (iswhat (matching-substring rest 2)))
	(insert 
	 (format "STATUS: %s"
		 iswhat))
	(newline))
    (insert "IRCHAT: Strange 313 reply")))

(defun irchat-handle-316-msg (prefix rest) ;;; RPL_WHOISCHANOP
  (if (string-match "\\([^ ]+\\) :\\(.*\\)" rest)
      (let ((who (matching-substring rest 1))
	    (iswhat (matching-substring rest 2)))
	(insert 
	 (format "STATUS: %s"
		 iswhat))
	(newline))
    (insert "IRCHAT: Strange 316 reply")))

(defun irchat-handle-314-msg (prefix rest) ;;; RPL_WHOWASUSER
  "Handle the 314 reply (msa's WHOWAS)."
  (if (string-match "[^ ]+ \\([^ ]+\\) \\([^ ]+\\) \\([^ ]+\\) \\([^ ]+\\) :\\(.*\\)" rest)
      (let ((nick (matching-substring rest 1))
	    (username (matching-substring rest 2))
	    (machine (matching-substring rest 3))
	    (channel (matching-substring rest 4))
	    (realname (matching-substring rest 5)))
	(insert 
	 (format "%s [%s] was %s (%s) at %s"
		 nick
		 (if (string= channel "*") "Priv" channel)
		 username
		 realname
		 machine))
	(newline))
    (insert "IRCHAT: Strange 314 reply")))

(defun irchat-handle-315-msg (prefix rest) ;;; RPL_ENDOFWHO
  nil)

(defun irchat-handle-321-msg (prefix rest) ;;; RPL_LISTSTART
  "Handle the 321 reply (first line from NAMES)."
  (insert
   (format "%7s%8s   %s"
	   "Channel"
	   "Users"
	   "Topic"))
  (newline))

(defun irchat-handle-322-msg (prefix rest) ;;; RPL_LIST
  "Handle the 322 reply (from NAMES)."
  (if (string-match "\\([^ ]+\\) \\([^ ]+\\) \\([^ ]+\\) :\\(.*\\)" rest)
      (let ((chnl (matching-substring rest 2))
	    (users (matching-substring rest 3))
	    (topic (matching-substring rest 4)))
	(if (or (string= irchat-channel-filter chnl)
		(string= irchat-channel-filter "")
		(and (string= irchat-channel-filter "0")
		     (string= chnl "*")))
	    (progn
	      (insert
	       (format "%7s%8s   %s"
		       (if (string= chnl "*") "Priv"
			 chnl)
		       users
		       topic))
	      (newline))))
    (message "IRCHAT: Strange 322 reply")))

(defun irchat-handle-323-msg (prefix rest) ;;; RPL_LISTEND
  nil)

(defun irchat-handle-324-msg (prefix rest) ;;; RPL_CHANNELMODEIS
  (insert (format "The mode is %s." rest))
  (newline)) ;;; needs work, I presume

(defun irchat-handle-331-msg (prefix rest) ;;; RPL_NOTOPIC
  (insert "*** IRCHAT: No topic is set")
  (newline))

(defun irchat-handle-332-msg (prefix rest) ;;; RPL_TOPIC
  (if (string-match "[^ ] +:\\(.*\\)" rest)
      (let ((topic (matching-substring rest 1)))
	(insert (format "*** Topic: %s" topic))
	(newline))
    (message "IRCHAT: Strange 332 message")))

(defun irchat-handle-341-msg (prefix rest) ;;; RPL_INVITING
  (if (string-match "\\([^ ]+\\) +\\([^ ]+\\) +\\([-0-9+][^ ]*\\)" rest)
      (let ((who (matching-substring rest 1))
	    (nick (matching-substring rest 2))
	    (chnl (matching-substring rest 3)))
	(insert (format "*** %s: Inviting user %s to channel %s" who nick chnl))
	(newline))
    (message "Strange 341 message")))

(defun irchat-handle-351-msg (prefix rest) ;; RPL_VERSION
  (if (string-match "[^ ]+ \\([^ ]+\\) \\([^ ]+\\)" rest)
      (let ((version (matching-substring rest 1))
	    (machine (matching-substring rest 2)))
	(insert 
	 (format "Machine %s is running IRC version %s"
		 machine
		 version))
	(newline))
    (message "IRCHAT: Strange 351 reply")))

(defun irchat-handle-361-msg (prefix rest) ;;; RPL_KILLDONE
  (if (string-match "[^ ]+ \\([^ ]+\\) +:\\(.*\\)" rest)
      (let ((who (matching-substring rest 1))
	    (message (matching-substring rest 2)))
	(insert 
	 (format "You just KILLED %s. %s"
		 who
		 message))
	(newline))
    (message "IRCHAT: Strange 361 reply")))

(defun irchat-handle-365-msg (prefix rest) ;;; RPL_ENDOFLINKS
  nil)

(defun irchat-handle-366-msg (prefix rest) ;;; RPL_ENDOFNAMES
  nil)

(defun irchat-handle-371-msg (prefix rest) ;;; RPL_INFO
  (if (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
      (let ((msg (matching-substring rest 2)))
	(insert (format "*** %s" msg))
	(newline))
    (message "IRCHAT: Strange 371 message")))

(defun irchat-handle-372-msg (prefix rest) ;;; RPL_MOTD
  (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
  (let ((msg (matching-substring rest 2)))
    (insert (format "*** %s" msg))
    (newline)))

(defun irchat-handle-381-msg (prefix rest) ;;; RPL_YOUREOPER
  (if (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
      (let ((message (matching-substring rest 2)))
	(insert "OPER: " message)
	(newline))
    (message "IRCHAT: Strange 381 reply")))

(defun irchat-handle-382-msg (prefix rest) ;;; RPL_REHASHING
  (string-match "\\([^ ]+\\) +:\\(.*\\)" rest)
  (let ((name (matching-substring rest 1))
	(msg (matching-substring rest 2)))
    (insert (format "*** %s: %s" name msg))
    (newline)))

(defun irchat-handle-391-msg (prefix rest) ;;; RPL_TIME
  (if (string-match "\\([^ ]+\\) +\\(.*\\)" rest)
      (let ((time (matching-substring rest 2)))
	(insert (format "Time: %s" time))
	(newline))
    (message "IRCHAT: Strange 391 message")))

;;; End of numeric reply codes.

(defun irchat-own-message (message)
  (let ((obuf (current-buffer)))
    (set-buffer irchat-Dialogue-buffer)
    (let (buffer-read-only)
      (goto-char (point-max))
      (irchat-handle-msg-msg nil (irchat-clean-string message)))
    (set-window-point (get-buffer-window irchat-Dialogue-buffer) (point-max))))

(defun irchat-send (&rest args)
  (let ((tem (eval (cons 'format args))))
    (process-send-string
     irchat-server-process
     (concat (irchat-clean-string tem) "\r"))
    (if (string-match "^away" (downcase tem))
	(if (string-match "^away *$" (downcase tem))
	    (setq irchat-away-indicator "-")
	  (setq irchat-away-indicator "A")))
    (setq foo (downcase tem))
    (if (string-match "^list" (downcase tem))
	(if (string-match "\\(^list\\) \\(.+\\)" foo)
	    (setq irchat-channel-filter (matching-substring foo 2))
	  (setq irchat-channel-filter "")))))    

(defun irchat-change-nick-of (old new)
  (let ((pair (assoc old irchat-nick-alist)))
    (if pair
	(rplaca pair new)
      (setq irchat-nick-alist (cons (cons new nil) irchat-nick-alist)))))

(defun irchat-clean-string (s)
  (while (string-match "[\C-@-\C-F\C-H-\C-_\C-?]" s)
    (aset s (match-beginning 0) ?.))
  s)

(defun irchat-clean-hostname (hostname)
  "Return the arg HOSTNAME, but if is a dotted-quad, put brackets around it."
  (let ((data (match-data)))
    (unwind-protect
	(if (string-match "[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+" hostname)
	    (concat "[" hostname "]")
	  hostname)
      (store-match-data data))))

(defun irchat-scroll-if-visible (window)
  (if window (set-window-point window (point-max))))

(defun irchat-scan-nicklist (str)
  (if (string-match "\\([^ ]+\\) \\(.*\\)" str)
      (let* ((nick (matching-substring str 1)) (n (intern nick)))
	(if (get n 'irchat-waited-for)
	    (progn
	      (beep t)
	      (message "IRCHAT: %s has entered! (%s)" nick
		       (if (string= chnl "0") "on no channel yet"
			 (concat "on channel " chnl)))
	      (if (get n 'irchat-greeting)
		  (irchat-send "PRIVMSG %s :%s" nick (get n 'irchat-greeting)))
	      (put n 'irchat-waited-for nil)
	      (put n 'irchat-greeting nil)))
	(if (and irchat-greet-author (string= nick irchat-author-nickname))
	    (progn
	      (setq irchat-greet-author nil)
	      (irchat-send "PRIVMSG %s :%s <%s@%s> is using irchat version %s"
			   irchat-author-nickname (user-full-name)
			   (user-login-name) irchat-system-fqdname
			   irchat-version)))	
	(setq irchat-nick-alist (cons (list nick) irchat-nick-alist))
	(irchat-scan-nicklist (matching-substring str 2)))
    nil))

(defun irchat-scan-channels (str)
  (setq irchat-channel-alist (cons (list chnl) irchat-channel-alist)))

