// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const Params = imports.misc.params;
const Util = imports.misc.util;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Calendar = imports.ui.calendar;

function _onVertSepRepaint (area)
{
    let cr = area.get_context();
    let themeNode = area.get_theme_node();
    let [width, height] = area.get_surface_size();
    let stippleColor = themeNode.get_color('-stipple-color');
    let stippleWidth = themeNode.get_length('-stipple-width');
    let x = Math.floor(width/2) + 0.5;
    cr.moveTo(x, 0);
    cr.lineTo(x, height);
    Clutter.cairo_set_source_color(cr, stippleColor);
    cr.setDash([1, 3], 1); // Hard-code for now
    cr.setLineWidth(stippleWidth);
    cr.stroke();
};

const DateMenuButton = new Lang.Class({
    Name: 'DateMenuButton',
    Extends: PanelMenu.Button,

    _init: function() {
        let item;
        let hbox;
        let vbox;

        let menuAlignment = 0.25;
        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            menuAlignment = 1.0 - menuAlignment;
        this.parent(menuAlignment);

        // At this moment calendar menu is not keyboard navigable at
        // all (so not accessible), so it doesn't make sense to set as
        // role ATK_ROLE_MENU like other elements of the panel.
        this.actor.accessible_role = Atk.Role.LABEL;

        this._clockDisplay = new St.Label();
        this.actor.add_actor(this._clockDisplay);

        hbox = new St.BoxLayout({name: 'calendarArea' });
        this.menu.addActor(hbox);

        // Fill up the first column

        vbox = new St.BoxLayout({vertical: true});
        hbox.add(vbox);

        // Date
        this._date = new St.Label();
        this.actor.label_actor = this._clockDisplay;
        this._date.style_class = 'datemenu-date-label';
        vbox.add(this._date);

        this._eventList = new Calendar.EventsList();
        this._calendar = new Calendar.Calendar();

        this._calendar.connect('selected-date-changed',
                               Lang.bind(this, function(calendar, date) {
                                  // we know this._eventList is defined here, because selected-data-changed
                                  // only gets emitted when the user clicks a date in the calendar,
                                  // and the calender makes those dates unclickable when instantiated with
                                  // a null event source
                                   this._eventList.setDate(date);
                               }));
        vbox.add(this._calendar.actor);

        item = this.menu.addSettingsAction(_("Date and Time Settings"), 'gnome-datetime-panel.desktop');
        if (item) {
            let separator = new PopupMenu.PopupSeparatorMenuItem();
            separator.setColumnWidths(1);
            vbox.add(separator.actor, {y_align: St.Align.END, expand: true, y_fill: false});

            item.actor.show_on_set_parent = false;
            item.actor.can_focus = false;
            item.actor.reparent(vbox);
            this._dateAndTimeSeparator = separator;
        }

        this._separator = new St.DrawingArea({ style_class: 'calendar-vertical-separator',
                                               pseudo_class: 'highlighted' });
        this._separator.connect('repaint', Lang.bind(this, _onVertSepRepaint));
        hbox.add(this._separator);

        // Fill up the second column
        vbox = new St.BoxLayout({ name: 'calendarEventsArea',
                                  vertical: true });
        hbox.add(vbox, { expand: true });

        // Event list
        vbox.add(this._eventList.actor, { expand: true });

        this._openCalendarItem = new PopupMenu.PopupMenuItem(_("Open Calendar"));
        this._openCalendarItem.connect('activate', Lang.bind(this, this._onOpenCalendarActivate));
        this._openCalendarItem.actor.can_focus = false;
        vbox.add(this._openCalendarItem.actor, {y_align: St.Align.END, expand: true, y_fill: false});

        this._calendarSettings = new Gio.Settings({ schema: 'org.gnome.desktop.default-applications.office.calendar' });
        this._calendarSettings.connect('changed::exec',
                                       Lang.bind(this, this._calendarSettingsChanged));
        this._calendarSettingsChanged();

        // Whenever the menu is opened, select today
        this.menu.connect('open-state-changed', Lang.bind(this, function(menu, isOpen) {
            if (isOpen) {
                let now = new Date();
                /* Passing true to setDate() forces events to be reloaded. We
                 * want this behavior, because
                 *
                 *   o It will cause activation of the calendar server which is
                 *     useful if it has crashed
                 *
                 *   o It will cause the calendar server to reload events which
                 *     is useful if dynamic updates are not supported or not
                 *     properly working
                 *
                 * Since this only happens when the menu is opened, the cost
                 * isn't very big.
                 */
                this._calendar.setDate(now, true);
                // No need to update this._eventList as ::selected-date-changed
                // signal will fire
            }
        }));

        // Done with hbox for calendar and event list

        this._clock = new GnomeDesktop.WallClock();
        this._clock.connect('notify::clock', Lang.bind(this, this._updateClockAndDate));
        this._updateClockAndDate();

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();
    },

    _calendarSettingsChanged: function() {
        let exec = this._calendarSettings.get_string('exec');
        let fullExec = GLib.find_program_in_path(exec);
        this._openCalendarItem.actor.visible = fullExec != null;
    },

    _setEventsVisibility: function(visible) {
        this._openCalendarItem.actor.visible = visible;
        this._separator.visible = visible;
        if (visible) {
          let alignment = 0.25;
          if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            alignment = 1.0 - alignment;
          this.menu._arrowAlignment = alignment;
          this._eventList.actor.get_parent().show();
        } else {
          this.menu._arrowAlignment = 0.5;
          this._eventList.actor.get_parent().hide();
        }
    },

    _setEventSource: function(eventSource) {
        if (this._eventSource)
            this._eventSource.destroy();

        this._calendar.setEventSource(eventSource);
        this._eventList.setEventSource(eventSource);

        this._eventSource = eventSource;
    },

    _sessionUpdated: function() {
        let eventSource;
        let showEvents = Main.sessionMode.showCalendarEvents;
        if (showEvents) {
            eventSource = new Calendar.DBusEventSource();
        } else {
            eventSource = new Calendar.EmptyEventSource();
        }
        this._setEventSource(eventSource);
        this._setEventsVisibility(showEvents);

        // This needs to be handled manually, as the code to
        // autohide separators doesn't work across the vbox
        this._dateAndTimeSeparator.actor.visible = Main.sessionMode.allowSettings;
    },

    _updateClockAndDate: function() {
        this._clockDisplay.set_text(this._clock.clock);
        /* Translators: This is the date format to use when the calendar popup is
         * shown - it is shown just below the time in the shell (e.g. "Tue 9:29 AM").
         */
        let dateFormat = _("%A %B %e, %Y");
        let displayDate = new Date();
        this._date.set_text(displayDate.toLocaleFormat(dateFormat));
    },

    _onOpenCalendarActivate: function() {
        this.menu.close();
        let tool = this._calendarSettings.get_string('exec');
        if (tool.length == 0 || tool.substr(0, 9) == 'evolution') {
            // TODO: pass the selected day
            let app = Shell.AppSystem.get_default().lookup_app('evolution-calendar.desktop');
            app.activate();
        } else {
            let needTerm = this._calendarSettings.get_boolean('needs-term');
            if (needTerm) {
                let terminalSettings = new Gio.Settings({ schema: 'org.gnome.desktop.default-applications.terminal' });
                let term = terminalSettings.get_string('exec');
                let arg = terminalSettings.get_string('exec-arg');
                if (arg != '')
                    Util.spawn([term, arg, tool]);
                else
                    Util.spawn([term, tool]);
            } else {
                Util.spawnCommandLine(tool)
            }
        }
    }
});
