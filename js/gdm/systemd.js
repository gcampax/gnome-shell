// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;

const SystemdLoginManagerIface = <interface name='org.freedesktop.login1.Manager'>
<method name='PowerOff'>
    <arg type='b' direction='in'/>
</method>
<method name='Reboot'>
    <arg type='b' direction='in'/>
</method>
<method name='CanPowerOff'>
    <arg type='s' direction='out'/>
</method>
<method name='CanReboot'>
    <arg type='s' direction='out'/>
</method>
</interface>;

const SystemdLoginManager = new Gio.DBusProxyClass({
    Name: 'SystemdLoginManager',
    Interface: SystemdLoginManagerIface,
    BusType: Gio.BusType.SYSTEM,
    BusName: 'org.freedesktop.login1',
    ObjectPath: '/org/freedesktop/login1'
});

function haveSystemd() {
    return GLib.access("/sys/fs/cgroup/systemd", 0) >= 0;
}
