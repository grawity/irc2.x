;;; own-irc.el
;;; this is the file you should load when you start irc
;;; place the line
;;; (autoload 'irchat "own-irc" "IRC mode" t)
;;; in your .emacs and place this file in your e-lisp directory. If
;;; you don't have one, include full path of own-irc.el. The values
;;; given in this file try to make irchat.el as much similar to the
;;; traditional clients as possible. Feel free to modify them.

(defvar irchat-use-full-window t) 

;;; nil means that you want to spare some of the window for GNUS, for
;;; example. If you want irchat to use all of your emacs screen by
;;; default, set this to t.

(defvar irchat-command-window-on-top nil)

;;; whether you want the command or dialogue buffer be above. nil is
;;; more like the traditional clients, t is more like GNUS.

(defvar irchat-change-prefix "***")

;;; prefix to add to all change messages. "" if you don't need this
;;; (why would you? ;-))

(defvar irchat-format-string1 "-> *%s*")

;;; format string for your private messages to someone. The person's
;;; name comes to %s. ">%s<" is a good choise. 

(defvar irchat-format-string2 "*%s*")

;;; format string for private messages from someone to you. Name comes
;;; in %s, again.

(defvar irchat-format-string3 "(%s)")

;;; format string for messages to the channel you're on.


(defvar irchat-beep-on-bells 'always)

;;; nil if we want to bells at all, t if we want bells in private
;;; mesages only, 'always if we want to hear all the bells.

(defvar nam-suffix "")

;;; a string to be added to the end of all messages to the channel. 
;;; ", n{{s. :-)" may be used on Finnish channels. ;)

(defvar irchat-reconnect-automagic t)

;;; Should irchat try to reconnect if it can? nil if not.

(defvar irchat-ask-for-nickname t)

;;; Should C-u M-x irchat ask for nickname as well as server? nil if
;;; not.

(defvar mta-own-topic "Wanhat IRC-patut ... (Finnish)")
(defvar mta-topic-active nil "Do I want to keep my topic...")

;;; If you want to keep a topic set on a channel, set the topic in
;;; mta-own-topic and t to mta-topic-active. DO NOT DO THIS unless you
;;; know how to take them off while in IRC.

(load "irchat")

;;; Now, load irchat. If your irchat is not load-path -accessible, add
;;; path here.

;;; Add nice hooks here, like this one that gets you on the channel
;;; you were on before, and does a NAMES for you.

(setq irchat-Startup-hook 
      '(lambda ()
	 (progn
	   (if irchat-current-channel 
	       (irchat-Command-join irchat-current-channel))
	   ; if we were on a channel, join back.
;	   (if (string= irchat-server "assari") 
;	       (irchat-enter-oper)) ;;; Sends a "OPER nick pass".
				    ;;; I define it elsewhere.
	   (irchat-Command-names)))) ; do a /NAMES

(setq irchat-Command-mode-hook
      '(lambda ()
         (define-key irchat-Command-mode-map
           "\C-[\C-i" 'lisp-complete-symbol)
         (define-key irchat-Command-mode-map
           "\C-i" 'irchat-Command-complete)
         ))

;;; Use this to have completion in TAB and normal lisp thingy in ESC
;;; TAB. Add your own keybindings there too.

