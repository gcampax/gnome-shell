// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const St = imports.gi.St;

const Batch = imports.gdm.batch;
const Fprint = imports.gdm.fingerprint;
const Realmd = imports.gdm.realmd;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

const PASSWORD_SERVICE_NAME = 'gdm-password';
const FINGERPRINT_SERVICE_NAME = 'gdm-fingerprint';
const FADE_ANIMATION_TIME = 0.16;
const CLONE_FADE_ANIMATION_TIME = 0.25;

const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
const FINGERPRINT_AUTHENTICATION_KEY = 'enable-fingerprint-authentication';
const BANNER_MESSAGE_KEY = 'banner-message-enable';
const BANNER_MESSAGE_TEXT_KEY = 'banner-message-text';
const ALLOWED_FAILURES_KEY = 'allowed-failures';

const LOGO_KEY = 'logo';
const DISABLE_USER_LIST_KEY = 'disable-user-list';

function fadeInActor(actor) {
    if (actor.opacity == 255 && actor.visible)
        return null;

    let hold = new Batch.Hold();
    actor.show();
    let [minHeight, naturalHeight] = actor.get_preferred_height(-1);

    actor.opacity = 0;
    actor.set_height(0);
    Tweener.addTween(actor,
                     { opacity: 255,
                       height: naturalHeight,
                       time: FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           this.set_height(-1);
                           hold.release();
                       },
                     });

    return hold;
}

function fadeOutActor(actor) {
    if (!actor.visible || actor.opacity == 0) {
        actor.opacity = 0;
        actor.hide();
        return null;
    }

    let hold = new Batch.Hold();
    Tweener.addTween(actor,
                     { opacity: 0,
                       height: 0,
                       time: FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           this.hide();
                           this.set_height(-1);
                           hold.release();
                       },
                     });
    return hold;
}

function cloneAndFadeOutActor(actor) {
    // Immediately hide actor so its sibling can have its space
    // and position, but leave a non-reactive clone on-screen,
    // so from the user's point of view it smoothly fades away
    // and reveals its sibling.
    actor.hide();

    let clone = new Clutter.Clone({ source: actor,
                                    reactive: false });

    Main.uiGroup.add_child(clone);

    let [x, y] = actor.get_transformed_position();
    clone.set_position(x, y);

    let hold = new Batch.Hold();
    Tweener.addTween(clone,
                     { opacity: 0,
                       time: CLONE_FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           clone.destroy();
                           hold.release();
                       }
                     });
    return hold;
}

const ShellUserVerifier = new Lang.Class({
    Name: 'ShellUserVerifier',

    _init: function(client, params) {
        params = Params.parse(params, { reauthenticationOnly: false });
        this._reauthOnly = params.reauthenticationOnly;

        this._client = client;

        this._settings = new Gio.Settings({ schema: LOGIN_SCREEN_SCHEMA });

        this._hasFingerprintHint = false;
        this._fprintManager = new Fprint.FprintManager();
        this._realmManager = new Realmd.Manager();

        this._failCounter = 0;
    },

    begin: function(userName, hold) {
        this._cancellable = new Gio.Cancellable();
        this._hold = hold;
        this._userName = userName;

        this._checkForFingerprintReader();

        if (userName) {
            // If possible, reauthenticate an already running session,
            // so any session specific credentials get updated appropriately
            this._client.open_reauthentication_channel(userName, this._cancellable,
                                                       Lang.bind(this, this._reauthenticationChannelOpened));
        } else {
            this._client.get_user_verifier(this._cancellable, Lang.bind(this, this._userVerifierGot));
        }
    },

    cancel: function() {
        if (this._cancellable)
            this._cancellable.cancel();

        if (this._userVerifier)
            this._userVerifier.call_cancel_sync(null);
    },

    clear: function() {
        if (this._cancellable) {
            this._cancellable.cancel();
            this._cancellable = null;
        }

        if (this._userVerifier) {
            this._userVerifier.run_dispose();
            this._userVerifier = null;
        }
    },

    answerQuery: function(serviceName, answer) {
        // Clear any previous message
        this.emit('show-message', null, null);

        this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
    },

    _checkForFingerprintReader: function() {
        this._haveFingerprintReader = false;

        if (!this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY))
            return;

        this._fprintManager.GetDefaultDeviceRemote(Gio.DBusCallFlags.NONE, this._cancellable, Lang.bind(this,
            function(device, error) {
                if (!error && device)
                    this._haveFingerprintReader = true;
            }));
    },

    _reportInitError: function(where, error) {
        logError(error, where);
        this._hold.release();

        this.emit('show-message', _("Authentication error"), 'login-dialog-message-warning');
        this._verificationFailed(false);
    },

    _reauthenticationChannelOpened: function(client, result) {
        try {
            this._userVerifier = client.open_reauthentication_channel_finish(result);
        } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
            return;
        } catch(e if e.matches(Gio.DBusError, Gio.DBusError.ACCESS_DENIED) &&
                !this._reauthOnly) {
            // Gdm emits org.freedesktop.DBus.Error.AccessDenied when there is
            // no session to reauthenticate. Fall back to performing verification
            // from this login session
            client.get_user_verifier(this._cancellable, Lang.bind(this, this._userVerifierGot));
            return;
        } catch(e) {
            this._reportInitError('Failed to open reauthentication channel', e);
            return;
        }

        this._connectSignals();
        this._beginVerification();
        this._hold.release();
    },

    _userVerifierGot: function(client, result) {
        try {
            this._userVerifier = client.get_user_verifier_finish(result);
        } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
            return;
        } catch(e) {
            this._reportInitError('Failed to obtain user verifier', e);
            return;
        }

        this._connectSignals();
        this._beginVerification();
        this._hold.release();
    },

    _connectSignals: function() {
        this._userVerifier.connect('info', Lang.bind(this, this._onInfo));
        this._userVerifier.connect('problem', Lang.bind(this, this._onProblem));
        this._userVerifier.connect('info-query', Lang.bind(this, this._onInfoQuery));
        this._userVerifier.connect('secret-info-query', Lang.bind(this, this._onSecretInfoQuery));
        this._userVerifier.connect('conversation-stopped', Lang.bind(this, this._onConversationStopped));
        this._userVerifier.connect('reset', Lang.bind(this, this._onReset));
        this._userVerifier.connect('verification-complete', Lang.bind(this, this._onVerificationComplete));
        this._userVerifier.connect('service-unavailable', Lang.bind(this, this._onServiceUnavailable));
    },

    _beginVerification: function() {
        this._hold.acquire();

        if (this._userName) {
            this._userVerifier.call_begin_verification_for_user(PASSWORD_SERVICE_NAME,
                                                                this._userName,
                                                                this._cancellable,
                                                                Lang.bind(this, function(obj, result) {
                try {
                    obj.call_begin_verification_for_user_finish(result);
                } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                    return;
                } catch(e) {
                    this._reportInitError('Failed to start verification for user', e);
                    return;
                }

                this._hold.release();
            }));

            if (this._haveFingerprintReader) {
                this._hold.acquire();

                this._userVerifier.call_begin_verification_for_user(FINGERPRINT_SERVICE_NAME,
                                                                    this._userName,
                                                                    this._cancellable,
                                                                    Lang.bind(this, function(obj, result) {
                    try {
                        obj.call_begin_verification_for_user_finish(result);
                    } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                        return;
                    } catch(e) {
                        this._reportInitError('Failed to start fingerprint verification for user', e);
                        return;
                    }

                    this._hold.release();
                }));
            }
        } else {
            this._userVerifier.call_begin_verification(PASSWORD_SERVICE_NAME,
                                                       this._cancellable,
                                                       Lang.bind(this, function(obj, result) {
                try {
                    obj.call_begin_verification_finish(result);
                } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                    return;
                } catch(e) {
                    this._reportInitError('Failed to start verification', e);
                    return;
                }

                this._hold.release();
            }));
        }
    },

    _onInfo: function(client, serviceName, info) {
        // We don't display fingerprint messages, because they
        // have words like UPEK in them. Instead we use the messages
        // as a cue to display our own message.
        if (serviceName == FINGERPRINT_SERVICE_NAME &&
            this._haveFingerprintReader) {

            this._hasFingerprintHint = true;
            // Translators: this message is shown below the password entry field
            // to indicate the user can swipe their finger instead
            this.emit('show-login-hint', _("(or swipe finger)"));
        } else if (serviceName == PASSWORD_SERVICE_NAME) {
            this.emit('show-message', info, 'login-dialog-message-info');
        }
    },

    _onProblem: function(client, serviceName, problem) {
        this.emit('show-message', problem, 'login-dialog-message-warning');
    },

    _showRealmLoginHint: function() {
        if (this._realmManager.loginFormat) {
            let hint = this._realmManager.loginFormat;

            hint = hint.replace(/%U/g, 'user');
            hint = hint.replace(/%D/g, 'DOMAIN');
            hint = hint.replace(/%[^UD]/g, '');

            this._hasFingerprintHint = false;
            // Translators: this message is shown below the username entry field
            // to clue the user in on how to login to the local network realm
            this.emit('show-login-hint',
                      _("(e.g., user or %s)").format(hint));
        }
    },

    _onInfoQuery: function(client, serviceName, question) {
        // We only expect questions to come from the main auth service
        if (serviceName != PASSWORD_SERVICE_NAME)
            return;

        this._showRealmLoginHint();
        this._realmLoginHintSignalId = this._realmManager.connect('login-format-changed',
                                                                  Lang.bind(this, this._showRealmLoginHint));

        this.emit('ask-question', serviceName, question, '');
    },

    _onSecretInfoQuery: function(client, serviceName, secretQuestion) {
        // We only expect secret requests to come from the main auth service
        if (serviceName != PASSWORD_SERVICE_NAME)
            return;

        // Special untranslated marker from pam_pin.so
        if (secretQuestion == 'PIN') {
            this.emit('show-pin-pad');
            secretQuestion = _("PIN:");
        } else {
            this.emit('hide-pin-pad');
        }

        this.emit('ask-question', serviceName, secretQuestion, '\u25cf');
    },

    _onReset: function() {
        this.clear();

        // Clear previous attempts to authenticate
        this._failCounter = 0;
        this._hasFingerprintHint = false;

        this.emit('reset');
    },

    _onVerificationComplete: function() {
        this.emit('verification-complete');
    },

    _verificationFailed: function(retry) {
        // For Not Listed / enterprise logins, immediately reset
        // the dialog
        // Otherwise, we allow ALLOWED_FAILURES attempts. After that, we
        // go back to the welcome screen.

        this._failCounter++;
        let canRetry = retry && this._userName &&
            this._failCounter < this._settings.get_int(ALLOWED_FAILURES_KEY);

        if (canRetry) {
            this.clear();
            this.begin(this._userName, new Batch.Hold());
        } else {
            // Allow some time to see the message, then reset everything
            Mainloop.timeout_add(3000, Lang.bind(this, function() {
                this.cancel();

                this._onReset();
            }));
        }

        this.emit('verification-failed');
    },

    _onConversationStopped: function(client, serviceName) {
        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (serviceName == PASSWORD_SERVICE_NAME) {
            this._verificationFailed(true);
        }

        if (serviceName == FINGERPRINT_SERVICE_NAME &&
            this._hasFingerprintHint) {
            this._hasFingerprintHint = false;
            this.emit('hide-login-hint');
        }

        if (this._realmLoginHintSignalId) {
            this._realmManager.disconnect(this._realmLoginHintSignalId);
            this._realmLoginHintSignalId = 0;
        }
    },

    _onServiceUnavailable: function(client, serviceName) {
        if (serviceName == FINGERPRINT_SERVICE_NAME &&
            this._hasFingerprintHint) {
            this._hasFingerprintHint = false;
            this.emit('hide-login-hint');
        }
    },
});
Signals.addSignalMethods(ShellUserVerifier.prototype);

const PinPadWidget = new Lang.Class({
    Name: 'PinPadWidget',

    _init: function(entry) {
        this._entry = entry;

        this.actor = new St.Table({ style_class: 'pin-pad' });

        this.actor.add(this._makeButton(0), { col: 1, row: 3 });

        for (let i = 1; i <= 9; i++) {
            this.actor.add(this._makeButton(i),
                            { col: ((i-1) % 3),
                              row: Math.floor((i-1) / 3)
                            });
        }
    },

    _makeButton: function(num) {
        let numStr = String(num);
        let button = new St.Button({ label: numStr,
                                     style_class: 'pin-pad-key' });

        button.connect('clicked', Lang.bind(this, function() {
            this._entry.clutter_text.insert_unichar(numStr);
        }));

        return button;
    }
});
