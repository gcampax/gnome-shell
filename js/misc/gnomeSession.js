// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Signals = imports.signals;

const PresenceIface = <interface name="org.gnome.SessionManager.Presence">
<method name="SetStatus">
    <arg type="u" direction="in"/>
</method>
<property name="status" type="u" access="readwrite"/>
<signal name="StatusChanged">
    <arg type="u" direction="out"/>
</signal>
</interface>;

const PresenceStatus = {
    AVAILABLE: 0,
    INVISIBLE: 1,
    BUSY: 2,
    IDLE: 3
};

const Presence = new Gio.DBusProxyClass({
    Name: 'GnomeSessionPresence',
    Interface: PresenceIface,
    BusType: Gio.BusType.SESSION,
    BusName: 'org.gnome.SessionManager',
    ObjectPath: '/org/gnome/SessionManager/Presence'
});

// Note inhibitors are immutable objects, so they don't
// change at runtime (changes always come in the form
// of new inhibitors)
const InhibitorIface = <interface name="org.gnome.SessionManager.Inhibitor">
<method name="GetAppId">
    <arg type="s" direction="out" />
</method>
<method name="GetReason">
    <arg type="s" direction="out" />
</method>
</interface>;

const Inhibitor = new Gio.DBusProxyClass({
    Name: 'GnomeSessionInhibitor',
    Interface: InhibitorIface,
    BusType: Gio.BusType.SESSION,
    BusName: 'org.gnome.SessionManager',
});

// Not the full interface, only the methods we use
const SessionManagerIface = <interface name="org.gnome.SessionManager">
<method name="Logout">
    <arg type="u" direction="in" />
</method>
<method name="Shutdown" />
<method name="CanShutdown">
    <arg type="b" direction="out" />
</method>
</interface>;

const SessionManager = new Gio.DBusProxyClass({
    Name: 'GnomeSessionManager',
    Interface: SessionManagerIface,
    BusType: Gio.BusType.SESSION,
    BusName: 'org.gnome.SessionManager',
    ObjectPath: '/org/gnome/SessionManager'
});

