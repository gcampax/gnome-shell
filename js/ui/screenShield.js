// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const St = imports.gi.St;

const GnomeSession = imports.misc.gnomeSession;
const Lightbox = imports.ui.lightbox;
const UnlockDialog = imports.ui.unlockDialog;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';
const LOCK_ENABLED_KEY = 'lock-enabled';

const CURTAIN_SLIDE_TIME = 1.2;
// fraction of screen height the arrow must reach before completing
// the slide up automatically
const ARROW_DRAG_TRESHOLD = 0.4;

/**
 * To test screen shield, make sure to kill gnome-screensaver.
 *
 * If you are setting org.gnome.desktop.session.idle-delay directly in dconf,
 * rather than through System Settings, you also need to set
 * org.gnome.settings-daemon.plugins.power.sleep-display-ac and
 * org.gnome.settings-daemon.plugins.power.sleep-display-battery to the same value.
 * This will ensure that the screen blanks at the right time when it fades out.
 * https://bugzilla.gnome.org/show_bug.cgi?id=668703 explains the dependance.
 */
const ScreenShield = new Lang.Class({
    Name: 'ScreenShield',

    _init: function() {
        this.actor = Main.layoutManager.screenShieldGroup;

        this._lockScreenGroup = new St.Widget({ x_expand: true,
                                                y_expand: true,
                                                reactive: true,
                                                can_focus: true,
                                                layout_manager: new Clutter.BinLayout
                                              });
        this._lockScreenGroup.connect('key-release-event',
                                      Lang.bind(this, this._onLockScreenKeyRelease));

        this._background = Meta.BackgroundActor.new_for_screen(global.screen);
        this._lockScreenGroup.add_actor(this._background);

        // FIXME: build the rest of the lock screen here

        this._arrow = new St.DrawingArea({ style_class: 'arrow',
                                           reactive: true,
                                           x_align: Clutter.ActorAlign.CENTER,
                                           y_align: Clutter.ActorAlign.END,
                                           // HACK: without these, ClutterBinLayout
                                           // ignores alignment properties on the actor
                                           x_expand: true,
                                           y_expand: true
                                         });
        this._arrow.connect('repaint', Lang.bind(this, this._drawArrow));

        this._arrowGrabbed = false;
        // offset of the mouse from the bottom of the stage when starting the drag
        this._arrowMouseOffset = 0;
        this._arrow.connect('button-press-event', Lang.bind(this, this._onArrowButtonPress));
        this._arrow.connect('button-release-event', Lang.bind(this, this._onArrowButtonRelease));
        this._arrow.connect('motion-event', Lang.bind(this, this._onArrowMotion));

        this._lockScreenGroup.add_actor(this._arrow);

        this._lockDialogGroup = new St.Widget({ x_expand: true,
                                                y_expand: true });

        this.actor.add_actor(this._lockDialogGroup);
        this.actor.add_actor(this._lockScreenGroup);

        this._presence = new GnomeSession.Presence(Lang.bind(this, function(proxy, error) {
            if (error) {
                logError(error, 'Error while reading gnome-session presence');
                return;
            }

            this._onStatusChanged(proxy.status);
        }));
        this._presence.connectSignal('StatusChanged', Lang.bind(this, function(proxy, senderName, [status]) {
            this._onStatusChanged(status);
        }));

        this._settings = new Gio.Settings({ schema: SCREENSAVER_SCHEMA });

        this._isModal = false;
        this._isLocked = false;

        this._lightbox = new Lightbox.Lightbox(Main.uiGroup,
                                               { inhibitEvents: true, fadeInTime: 10, fadeFactor: 1 });
    },

    _onLockScreenKeyRelease: function(actor, event) {
        if (event.get_key_symbol() == Clutter.KEY_Escape) {
            if (this._arrowGrabbed) {
                Clutter.ungrab_pointer();
                this._arrowGrabbed = false;
            }

            this._showUnlockDialog();
        }
    },

    _drawArrow: function() {
        let cr = this._arrow.get_context();
        let [w, h] = this._arrow.get_surface_size();
        let node = this._arrow.get_theme_node();

        Clutter.cairo_set_source_color(cr, node.get_foreground_color());

        cr.moveTo(0, h);
        cr.lineTo(w/2, 0);
        cr.lineTo(w, h);
        cr.fill();

        // FIXME: it could be useful to generalize this code
        // to handle borders too
        // or maybe even better, make it into a
        // shell_util_render_arrow(StWidget, cairo_t)
        // in the style of gtk_render_arrow
    },

    _onArrowButtonPress: function(actor, event) {
        if (event.get_button() != 1)
            return false;

        if (!this._arrowGrabbed) {
            let [x,y] = event.get_coords();
            let [x0,y0] = actor.get_transformed_position();
            this._arrowMouseOffset = y0 + actor.height - y;

            Clutter.grab_pointer(this._arrow);
            this._arrowGrabbed = true;
            Tweener.removeTweens(this._lockScreenGroup);
        }
        return true;
    },

    _onArrowButtonRelease: function(actor, event) {
        if (this._arrowGrabbed) {
            Clutter.ungrab_pointer();
            this._arrowGrabbed = false;

            // restore the lock screen to its original place
            // try to use the same speed as the normal animation
            let h = global.stage.height;
            let time = CURTAIN_SLIDE_TIME * (-this._lockScreenGroup.y) / h;
            Tweener.removeTweens(this._lockScreenGroup);
            Tweener.addTween(this._lockScreenGroup,
                             { y: 0,
                               time: time,
                               transition: 'linear',
                               onComplete: function() {
                                   this.fixed_position_set = false;
                               }
                             });
        }

        return true;
    },

    _onArrowMotion: function(actor, event) {
        if (!this._arrowGrabbed)
            return false;

        let [x,y] = event.get_coords();
        this._lockScreenGroup.y = -(global.stage.height - y);

        if (this._lockScreenGroup.y < -(ARROW_DRAG_TRESHOLD * global.stage.height)) {
            Clutter.ungrab_pointer();
            this._arrowGrabbed = false;

            // Complete motion automatically
            this._showUnlockDialog();
        }

        return true;
    },

    _onStatusChanged: function(status) {
        if (status == GnomeSession.PresenceStatus.IDLE) {
            if (this._dialog) {
                this._dialog.cancel();
                this._dialog = null;
            }

            if (!this._isModal) {
                Main.pushModal(this.actor);
                this._isModal = true;
            }

            this._lightbox.show();
        } else {
            let lightboxWasShown = this._lightbox.shown;
            this._lightbox.hide();

            let shouldLock = lightboxWasShown && this._settings.get_boolean(LOCK_ENABLED_KEY);
            if (this._isLocked || shouldLock) {
                this.lock();
            } else if (this._isModal) {
                this.unlock();
            }
        }
    },

    get locked() {
        return this._isLocked;
    },

    unlock: function() {
        if (this._dialog) {
            this._dialog.destroy();
            this._dialog = null;
        }

        this._lightbox.hide();

        Main.popModal(this.actor);
        this.actor.hide();

        this._isModal = false;
        this._isLocked = false;

        this.emit('lock-status-changed', false);
    },

    lock: function() {
        if (!this._isModal) {
            Main.pushModal(this.actor);
            this._isModal = true;
        }

        this._isLocked = true;
        this.actor.show();
        this._resetLockScreen();

        this.emit('lock-status-changed', true);
    },

    _showUnlockDialog: function() {
        if (this._dialog) {
            log ('_showUnlockDialog called again when the dialog was already there');
            return;
        }

        // Tween the lock screen out of screen
        // try to use the same speed regardless of original position
        let h = global.stage.height;
        let time = CURTAIN_SLIDE_TIME * (h + this._lockScreenGroup.y) / h;
        Tweener.removeTweens(this._lockScreenGroup);
        Tweener.addTween(this._lockScreenGroup,
                         { y: -h,
                           time: time,
                           transition: 'linear',
                           onComplete: Lang.bind(this, this._hideLockScreen),
                         });

        this._dialog = new UnlockDialog.UnlockDialog(this._lockDialogGroup);
        this._dialog.connect('failed', Lang.bind(this, this._onUnlockFailed));
        this._dialog.connect('unlocked', Lang.bind(this, this._onUnlockSucceded));
        this._dialog.open(global.get_current_time());
    },

    _onUnlockFailed: function() {
        this._dialog.destroy();
        this._dialog = null;

        this._resetLockScreen();
    },

    _onUnlockSucceded: function() {
        this.unlock();
    },

    _hideLockScreen: function() {
        this._arrow.hide();
        this._lockScreenGroup.hide();
    },

    _resetLockScreen: function() {
        this._lockScreenGroup.fixed_position_set = false;
        this._lockScreenGroup.show();
        this._arrow.show();

        this._lockScreenGroup.grab_key_focus();
    }
});
Signals.addSignalMethods(ScreenShield.prototype);
