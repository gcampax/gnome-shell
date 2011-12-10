// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;

const ConsoleKitManagerIface = <interface name='org.freedesktop.ConsoleKit.Manager'>
<method name='CanRestart'>
    <arg type='b' direction='out'/>
</method>
<method name='CanStop'>
    <arg type='b' direction='out'/>
</method>
<method name='Restart' />
<method name='Stop' />
</interface>;

const ConsoleKitManager = new Gio.DBusProxyClass({
    Name: 'ConsoleKitManagerProxy',
    Interface: ConsoleKitManagerIface,
    BusType: Gio.BusType.SESSION,
    BusName: 'org.freedesktop.ConsoleKit',
    ObjectPath: '/org/freedesktop/ConsoleKit/Manager'
});
