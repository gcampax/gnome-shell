// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const St = imports.gi.St;

const GnomeSession = imports.misc.gnomeSession;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';
const LOCK_ENABLED_KEY = 'lock-enabled';

const CURTAIN_SLIDE_TIME = 1.2;
// fraction of screen height the arrow must reach before completing
// the slide up automatically
const ARROW_DRAG_TRESHOLD = 0.4;

// Lightbox fading times
// STANDARD_FADE_TIME is used when the session goes idle, while
// SHORT_FADE_TIME is used when requesting lock explicitly from the user menu
const STANDARD_FADE_TIME = 10;
const SHORT_FADE_TIME = 2;

const Clock = new Lang.Class({
    Name: 'ScreenShieldClock',

    CLOCK_FORMAT_KEY: 'clock-format',
    CLOCK_SHOW_SECONDS_KEY: 'clock-show-seconds',

    _init: function() {
        this.actor = new St.BoxLayout({ style_class: 'screen-shield-clock',
                                        vertical: true });

        this._time = new St.Label({ style_class: 'screen-shield-clock-time' });
        this._date = new St.Label({ style_class: 'screen-shield-clock-date' });

        this.actor.add(this._time, { x_align: St.Align.MIDDLE });
        this.actor.add(this._date, { x_align: St.Align.MIDDLE });

        // I wonder why dateMenu.js doesn't use this, it was designed exactly
        // for that
        this._wallClock = new GnomeDesktop.WallClock();
        this._wallClock.connect('notify::clock', Lang.bind(this, this._updateClock));

        this._settings = new Gio.Settings({ schema: 'org.gnome.desktop.interface' });
        // No need to connect signals, GnomeWallClock does that automatically

        this._updateClock();
    },

    _updateClock: function() {
        // Ignore the :clock property of GnomeWallClock, as we want
        // to format it differently
        // Ignore clock-show-date setting, as we show it separately

        let format = this._settings.get_string(this.CLOCK_FORMAT_KEY);
        let showSeconds = this._settings.get_boolean(this.CLOCK_SHOW_SECONDS_KEY);

        let clockFormat;
        switch (format) {
            case '24h':
            /* Translators: This is the time format without date used
               in 24-hour mode. */
            clockFormat = showSeconds ? _("%0R:%S")
                : _("%0R");
            break;
            case '12h':
            default:
            /* Translators: This is a time format without date used
               for AM/PM. */
            clockFormat = showSeconds ? _("%l:%M:%S %p")
                : _("%l:%M %p");
            break;
        }

        /* Translators: This is a time format for a date in
           long format */
        let dateFormat = _("%A, %B %d");

        let date = new Date();
        this._time.text = date.toLocaleFormat(clockFormat);
        this._date.text = date.toLocaleFormat(dateFormat);
    },

    destroy: function() {
        this.actor.destroy();
        this._wallClock.run_dispose();
    }
});

const NotificationsBox = new Lang.Class({
    Name: 'NotificationsBox',

    _init: function() {
        this.actor = new St.BoxLayout({ vertical: true,
                                        name: 'screenShieldNotifications',
                                        margin_top: 20
                                      });

        this._residentNotificationBox = new St.BoxLayout({ vertical: true,
                                                           style_class: 'screen-shield-notifications-box' });
        this._persistentNotificationBox = new St.BoxLayout({ vertical: true,
                                                             style_class: 'screen-shield-notifications-box' });

        this.actor.add(this._residentNotificationBox, { x_fill: true });
        this.actor.add(this._persistentNotificationBox, { x_fill: false, x_align: St.Align.MIDDLE });

        this._items = [];
        Main.messageTray.getSummaryItems().forEach(Lang.bind(this, function(item) {
            this._summaryItemAdded(Main.messageTray, item);
        }));

        this._summaryAddedId = Main.messageTray.connect('summary-item-added', Lang.bind(this, this._summaryItemAdded));
    },

    destroy: function() {
        if (this._summaryAddedId) {
            Main.messageTray.disconnect(this._summaryAddedId);
            this._summaryAddedId = 0;
        }

        for (let i = 0; i < this._items.length; i++)
            this._removeItem(this._items[i]);
        this._items = [];

        this.actor.destroy();
    },

    _updateVisibility: function() {
        if (this._residentNotificationBox.get_n_children() > 0) {
            this.actor.show();
            return;
        }

        let children = this._persistentNotificationBox.get_children()
        this.actor.visible = children.some(function(a) { return a.visible; });
    },

    _sourceIsResident: function(source) {
        return source.hasResidentNotification() && !source.isChat;
    },

    _makeNotificationCountText: function(source) {
        if (source.isChat)
            return ngettext("%d new message", "%d new messages", source.count).format(source.count);
        else
            return ngettext("%d new notification", "%d new notifications", source.count).format(source.count);
    },

    _makeNotificationSource: function(source) {
        let box = new St.BoxLayout({ style_class: 'screen-shield-notification-source' });

        // FIXME: iconClone is 24px right now, but will become 48px when the message tray
        // is redesigned, so for now just scale it
        let iconClone = new Clutter.Clone({ source: source.getSummaryIcon(),
                                            width: 48, height: 48 });
        box.add(iconClone, { y_fill: true });

        let textBox = new St.BoxLayout({ vertical: true });
        box.add(textBox, { y_fill: true, expand: true });

        let label = new St.Label({ text: source.title,
                                   style_class: 'screen-shield-notification-label' });
        textBox.add(label);

        let countLabel = new St.Label({ text: this._makeNotificationCountText(source),
                                        style_class: 'screen-shield-notification-count-text' });
        textBox.add(countLabel);

        box.visible = source.count != 0;
        return [box, countLabel];
    },

    _summaryItemAdded: function(tray, item) {
        // Ignore transient sources
        if (item.source.isTransient)
            return;

        let obj = {
            item: item,
            source: item.source,
            resident: this._sourceIsResident(item.source),
            contentUpdatedId: 0,
            sourceDestroyId: 0,
            sourceBox: null,
            countLabel: null,
        };

        if (obj.resident) {
            item.prepareNotificationStackForShowing();
            this._residentNotificationBox.add(item.notificationStackView);
        } else {
            [obj.sourceBox, obj.countLabel] = this._makeNotificationSource(item.source);
            this._persistentNotificationBox.add(obj.sourceBox);
        }

        obj.contentUpdatedId = item.connect('content-updated', Lang.bind(this, this._onItemContentUpdated));
        obj.sourceDestroyId = item.source.connect('destroy', Lang.bind(this, this._onSourceDestroy));
        this._items.push(obj);

        this._updateVisibility();
    },

    _findSource: function(source) {
        for (let i = 0; i < this._items.length; i++) {
            if (this._items[i].source == source)
                return i;
        }

        return -1;
    },

    _onItemContentUpdated: function(item) {
        let obj = this._items[this._findSource(item.source)];

        let itemShouldBeResident = this._sourceIsResident(item.source);

        if (itemShouldBeResident && obj.resident) {
            // Nothing to do here, the actor is already updated
            return;
        }

        if (obj.resident && !itemShouldBeResident) {
            // make into a regular item
            this._residentNotificationBox.remove_actor(item.notificationStackView);

            [obj.sourceBox, obj.countLabel] = this._makeNotificationSource(item.source);
            this._persistentNotificationBox.add(obj.sourceBox);
        } else if (itemShouldBeResident && !obj.resident) {
            // make into a resident item
            obj.sourceBox.destroy();
            obj.sourceBox = obj.countLabel = null;

            item.prepareNotificationStackForShowing();
            this._residentNotificationBox.add(item.notificationStackView);
        } else {
            // just update the counter
            obj.countLabel.text = this._makeNotificationCountText(item.source);
            obj.sourceBox.visible = source.count != 0;
        }

        this._updateVisibility();
    },

    _onSourceDestroy: function(source) {
        let idx = this._findSource(item.source);

        this._removeItem(this._items[idx]);
        this._items.splice(idx, 1);

        this._updateVisibility();
    },

    _removeItem: function(obj) {
        if (obj.resident) {
            this._residentNotificationBox.remove_actor(obj.item.notificationStackView);
            obj.item.doneShowingNotificationStack();
        } else {
            obj.sourceBox.destroy();
        }

        obj.item.disconnect(obj.contentUpdatedId);
        obj.source.disconnect(obj.sourceDestroyId);
    },
});

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
        this._hasLockScreen = false;

        this._lightbox = new Lightbox.Lightbox(Main.uiGroup,
                                               { inhibitEvents: true,
                                                 fadeInTime: STANDARD_FADE_TIME,
                                                 fadeFactor: 1 });
    },

    _onLockScreenKeyRelease: function(actor, event) {
        if (event.get_key_symbol() == Clutter.KEY_Escape) {
            if (this._arrowGrabbed) {
                Clutter.ungrab_pointer();
                this._arrowGrabbed = false;
            }

            this._showUnlockDialog(true);
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
            this._showUnlockDialog(true);
        }

        return true;
    },

    _onStatusChanged: function(status) {
        if (status == GnomeSession.PresenceStatus.IDLE) {
            if (this._dialog) {
                this._dialog.cancel();
                if (!this._keepDialog) {
                    this._dialog = null;
                }
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
                this.lock(false);
            } else if (this._isModal) {
                this.unlock();
            }
        }
    },

    get locked() {
        return this._isLocked;
    },

    unlock: function() {
        if (this._hasLockScreen)
            this._clearLockScreen();

        if (this._keepDialog) {
            // The dialog must be kept alive,
            // so immediately go back to it
            // This will also reset _isLocked
            this._showUnlockDialog(false);
            return;
        }

        if (this._dialog) {
            this._dialog.destroy();
            this._dialog = null;
        }

        this._lightbox.hide();

        if (this._isModal)
            Main.popModal(this.actor);
        this.actor.hide();

        this._isModal = false;
        this._isLocked = false;

        this.emit('lock-status-changed', false);
    },

    lock: function(animate) {
        if (!this._hasLockScreen)
            this._prepareLockScreen();

        if (!this._isModal) {
            Main.pushModal(this.actor);
            this._isModal = true;
        }

        this._isLocked = true;
        this.actor.show();
        this._resetLockScreen(animate);

        this.emit('lock-status-changed', true);
    },

    showDialog: function() {
        this.lock(true);
        this._showUnlockDialog(false);
    },

    _showUnlockDialog: function(animate) {
        if (animate) {
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
        } else {
            this._hideLockScreen();
        }

        if (!this._dialog) {
            [this._dialog, this._keepDialog] = Main.sessionMode.createUnlockDialog(this._lockDialogGroup);
            if (!this._dialog) {
                // This session mode has no locking capabilities
                this.unlock();
                return;
            }

            this._dialog.connect('failed', Lang.bind(this, this._onUnlockFailed));
            this._dialog.connect('unlocked', Lang.bind(this, this._onUnlockSucceded));
        }

        if (this._keepDialog) {
            // Notify the other components that even though we are showing the
            // screenshield, we're not in a locked state
            // (this happens for the gdm greeter)

            this._isLocked = false;
            this.emit('lock-status-changed', false);
        }
    },

    _onUnlockFailed: function() {
        this._dialog.destroy();
        this._dialog = null;

        this._resetLockScreen(false);
    },

    _onUnlockSucceded: function() {
        this.unlock();
    },

    _hideLockScreen: function() {
        this._arrow.hide();
        this._lockScreenGroup.hide();
    },

    _resetLockScreen: function(animate) {
        if (animate) {
            this.actor.opacity = 0;
            Tweener.removeTweens(this.actor);
            Tweener.addTween(this.actor,
                             { opacity: 255,
                               time: SHORT_FADE_TIME,
                               transition: 'easeOutQuad'
                               });
        }

        this._lockScreenGroup.fixed_position_set = false;
        this._lockScreenGroup.show();
        this._arrow.show();

        this._lockScreenGroup.grab_key_focus();
    },

    // Some of the actors in the lock screen are heavy in
    // resources, so we only create them when needed
    _prepareLockScreen: function() {
        this._lockScreenContentsBox = new St.BoxLayout({ x_align: Clutter.ActorAlign.CENTER,
                                                         y_align: Clutter.ActorAlign.CENTER,
                                                         x_expand: true,
                                                         y_expand: true,
                                                         vertical: true });
        this._clock = new Clock();
        this._lockScreenContentsBox.add(this._clock.actor, { x_fill: true,
                                                             y_fill: true });

        this._lockScreenGroup.add_actor(this._lockScreenContentsBox);

        if (this._settings.get_boolean('show-notifications')) {
            this._notificationsBox = new NotificationsBox();
            this._lockScreenContentsBox.add(this._notificationsBox.actor, { x_fill: true,
                                                                            y_fill: true,
                                                                            expand: true });
        }

        this._hasLockScreen = true;
    },

    _clearLockScreen: function() {
        this._clock.destroy();
        this._clock = null;

        if (this._notificationsBox) {
            this._notificationsBox.destroy();
            this._notificationsBox = null;
        }

        this._lockScreenContentsBox.destroy();

        this._hasLockScreen = false;
    }
});
Signals.addSignalMethods(ScreenShield.prototype);
