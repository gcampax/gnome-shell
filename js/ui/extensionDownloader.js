// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Soup = imports.gi.Soup;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const Config = imports.misc.config;
const ExtensionUtils = imports.misc.extensionUtils;
const ExtensionSystem = imports.ui.extensionSystem;
const FileUtils = imports.misc.fileUtils;
const MessageTray = imports.ui.messageTray;
const ModalDialog = imports.ui.modalDialog;

const Main = imports.ui.main;

const _signals = ExtensionSystem._signals;

const REPOSITORY_URL_BASE = 'https://extensions.gnome.org';
const REPOSITORY_URL_DOWNLOAD = REPOSITORY_URL_BASE + '/download-extension/%s.shell-extension.zip';
const REPOSITORY_URL_INFO     = REPOSITORY_URL_BASE + '/extension-info/';
const REPOSITORY_URL_UPDATE   = REPOSITORY_URL_BASE + '/update-info/';

let _httpSession;

function installExtension(uuid, invocation) {
    let params = { uuid: uuid,
                   shell_version: Config.PACKAGE_VERSION };

    let message = Soup.form_request_new_from_hash('GET', REPOSITORY_URL_INFO, params);

    _httpSession.queue_message(message, function(session, message) {
        if (message.status_code != Soup.KnownStatusCode.OK) {
            ExtensionSystem.logExtensionError(uuid, 'downloading info: ' + message.status_code);
            invocation.return_dbus_error('org.gnome.Shell.DownloadInfoError', message.status_code.toString());
            return;
        }

        let info;
        try {
            info = JSON.parse(message.response_body.data);
        } catch (e) {
            ExtensionSystem.logExtensionError(uuid, 'parsing info: ' + e);
            invocation.return_dbus_error('org.gnome.Shell.ParseInfoError', e.toString());
            return;
        }

        let dialog = new InstallExtensionDialog(uuid, info, invocation);
        dialog.open(global.get_current_time());
    });
}

function uninstallExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return false;

    // Don't try to uninstall system extensions
    if (extension.type != ExtensionUtils.ExtensionType.PER_USER)
        return false;

    if (!ExtensionSystem.unloadExtension(extension))
        return false;

    FileUtils.recursivelyDeleteDir(extension.dir, true);
    return true;
}

function gotExtensionZipFile(session, message, uuid, dir, callback, errback) {
    if (message.status_code != Soup.KnownStatusCode.OK) {
        errback('DownloadExtensionError', message.status_code);
        return;
    }

    try {
        if (!dir.query_exists(null))
            dir.make_directory_with_parents(null);
    } catch (e) {
        errback('CreateExtensionDirectoryError', e);
        return;
    }

    let [file, stream] = Gio.File.new_tmp('XXXXXX.shell-extension.zip');
    let contents = message.response_body.flatten().get_as_bytes();
    stream.output_stream.write_bytes(contents, null);
    stream.close(null);
    let [success, pid] = GLib.spawn_async(null,
                                          ['unzip', '-uod', dir.get_path(), '--', file.get_path()],
                                          null,
                                          GLib.SpawnFlags.SEARCH_PATH | GLib.SpawnFlags.DO_NOT_REAP_CHILD,
                                          null);

    if (!success) {
        errback('ExtractExtensionError');
        return;
    }

    GLib.child_watch_add(GLib.PRIORITY_DEFAULT, pid, function(pid, status) {
        GLib.spawn_close_pid(pid);

        if (status != 0)
            errback('ExtractExtensionError');
        else
            callback();
    });
}

let _source;
function _ensureSource() {
    if (_source)
        return _source;

    _source = new MessageTray.Source(_("Extension System"), 'applications-system');
    _source.connect('destroy', function() { _source = null; });
    Main.messageTray.add(_source);
    return _source;
}

function _notifyMessage(title, message) {
    let source = _ensureSource();
    let n = new MessageTray.Notification(source, title, message ? String(message) : '');
    n.setTransient(true);

    source.notify(n);
}

function updateExtension(uuid, callback, errback) {
    // This gets a bit tricky. We want the update to be seamless -
    // if we have any error during downloading or extracting, we
    // want to not unload the current version.

    let oldExtensionTmpDir = Gio.File.new_for_path(GLib.Dir.make_tmp('XXXXXX-shell-extension'));
    let newExtensionTmpDir = Gio.File.new_for_path(GLib.Dir.make_tmp('XXXXXX-shell-extension'));

    let params = { shell_version: Config.PACKAGE_VERSION };

    let url = REPOSITORY_URL_DOWNLOAD.format(uuid);
    let message = Soup.form_request_new_from_hash('GET', url, params);

    _httpSession.queue_message(message, Lang.bind(this, function(session, message) {
        gotExtensionZipFile(session, message, uuid, newExtensionTmpDir, function() {
            let oldExtension = ExtensionUtils.extensions[uuid];
            let extensionDir = oldExtension.dir;

            if (!ExtensionSystem.unloadExtension(oldExtension))
                return;

            FileUtils.recursivelyMoveDir(extensionDir, oldExtensionTmpDir);
            FileUtils.recursivelyMoveDir(newExtensionTmpDir, extensionDir);

            let extension = ExtensionUtils.createExtensionObject(uuid, extensionDir, ExtensionUtils.ExtensionType.PER_USER);

            try {
                ExtensionSystem.loadExtension(extension);
            } catch(e) {
                ExtensionSystem.unloadExtension(extension);

                logError(e, 'Error loading extension %s'.format(uuid));

                FileUtils.recursivelyDeleteDir(extensionDir, false);
                FileUtils.recursivelyMoveDir(oldExtensionTmpDir, extensionDir);

                // Restore what was there before. We can't do much if we
                // fail here.
                ExtensionSystem.loadExtension(oldExtension);

                errback(uuid, e.code, e.message);
                return;
            }

            FileUtils.recursivelyDeleteDir(oldExtensionTmpDir, true);

            callback(uuid);
        }, function(code, message) {
            errback(uuid, code, message);
        });
    }));
}

function _notifyUpdateSuccessOne(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    let name = extension.metadata.name;

    _notifyMessage(_("Extension %s successfully updated").format(name));
}

function _notifyUpdateFailure(uuid, code, message) {
    let extension = ExtensionUtils.extensions[uuid];
    let name = extension ? extension.metadata.name : uuid;

    _notifyMessage(_("There was a problem updating %s").format(name), code, message);
}

function _notifyUpdateSuccessMany(number) {
    let title = ngettext("%d extension successfully updated",
                         "%d extensions successfully updated", number);

    _notifyMessage(title.format(number));
}

function _applyUpdates(updates) {
    let n = updates.length;
    let callback;

    if (updates.length == 1) {
        callback = _notifyUpdateSuccessOne;
    } else {
        callback = function() {
            if (n <= 0)
                return;

            n--;
            if (n == 0)
                _notifyUpdateSuccessMany(updates.length);
        };
    }

    for (let i = 0; i < updates.length; i++)
        updateExtension(updates[i].uuid, callback, _notifyUpdateFailure);
}

function _notifyForUpdates(updates) {
    let title = ngettext("Extension update available",
                         "Extension updates available", updates.length);
    let body;

    if (updates.length == 1) {
        body = _("Version %d of %s is available for update.").format(updates[0].version, updates[0].name);
    } else {
        body = ngettext("%d extension can be updated",
                        "%d extensions can be updated", updates.length).
            format(updates.length);
    }

    let source = _ensureSource();
    let n = new MessageTray.Notification(source, title, body);
    // This is wrong in principle, but is needed because otherwise action-invoked
    // destroys the notification at the end
    n.setResident(true);
    n.addButton('update', _("Update now"));
    if (updates.length > 1 && updates.some(function(u) { return !!u.url; }))
        n.addButton('details', _("Show details"));

    n.connect('action-invoked', function(n, id) {
        if (id == 'details') {
            n.update(title, _("The following updates are available"),
                     { clear: true });

            for (let i = 0; i < updates.length; i++) {
                let b = new St.Button({ label: updates[i].name,
                                        style: 'shell-link-notification' });
                let url = updates[i].url;
                if (url) {
                    b.connect('clicked', function() {
                        Gtk.show_uri(null, url, global.get_current_time());
                    });
                }

                n.addActor(b, { x_align: St.Align.START });
            }

            n.addButton('update', _("Update now"));
        } else if (id == 'update') {
            _applyUpdates(updates, true);

            n.emit('done-displaying');
            n.destroy();
        }
    });

    source.notify(n);
}

function _logDowngradeFailure(uuid, code, message) {
    log('Error downgrading %s: %d: %s'.format(uuid, code, message));
}

function _notifyForDowngraded(updates) {
    for (let i = 0; i < updates.length; i++)
        updateExtension(updates[i].uuid, function() { },
                        _logDowngradeFailure);

    let title = ngettext("One extension was downgraded",
                         "Some extensions were downgraded", updates.length);
    let body;
    if (updates.length == 1) {
        body = _("For security reasons, %s was downgraded to version %d").
            format(updates[0].name, updates[0].version);
    } else {
        body = ngettext("For security reasons, the following extension was downgraded:\n",
                        "For security reasons, the following extensions were downgraded:\n",
                        updates.length);
        for (let i = 0; i < updates.length; i++)
            body += (i > 0 ? '\n' : '') + updates[i].name;
    }

    let source = _ensureSource();
    let n = new MessageTray.Notification(source, title, body);
    source.notify(n);
}

function _notifyForBlacklisted(updates) {
    if (0) {
        /* Don't really do it, extensions.gnome.org is broken and
           emits 'blacklisted' for outdated extensions for which
           no update exists
        */

        for (let i = 0; i < updates.length; i++)
            uninstallExtension(updates[i].uuid);
    }

    let title = ngettext("One extension was blacklisted",
                         "Some extensions were blacklisted", updates.length);
    let body;
    if (updates.length == 1) {
        body = _("For security reasons, %s was uninstalled").format(updates[0].name);
    } else {
        body = ngettext("For security reasons, the following extension was uninstalled:\n",
                        "For security reasons, the following extensions were uninstalled:\n",
                        updates.length);
        for (let i = 0; i < updates.length; i++)
            body += (i > 0 ? '\n' : '') + updates[i].name;
    }

    let source = _ensureSource();
    let n = new MessageTray.Notification(source, title, body);
    source.notify(n);
}

function checkForUpdates() {
    let current = [];
    for (let uuid in ExtensionUtils.extensions) {
        let extension = ExtensionUtils.extensions[uuid];
        if (extension.metadata.version)
            current.push([uuid, extension.metadata.version]);

        // don't bother checking for newer version of extensions
        // that were not installed from extensions.gnome.org,
        // the server will ignore them anyway
    }

    let params = { shell_version: Config.PACKAGE_VERSION,
                   current: JSON.stringify(current) };

    let url = REPOSITORY_URL_UPDATE;
    let message = Soup.form_request_new_from_hash('GET', url, params);
    _httpSession.queue_message(message, function(session, message) {
        if (message.status_code != Soup.KnownStatusCode.OK)
            return;

        let operations = JSON.parse(message.response_body.data);
        let toUpdate = [], toBlacklist = [], toDowngrade = [];

        for (let uuid in operations) {
            let operation = operations[uuid];
            let extension = ExtensionUtils.extensions[uuid];

            if (!extension) {
                // Something else operated on this extension
                // while we were checking for updates
                continue;
            }

            let obj = { };
            let type;
            if (typeof operation == 'string') {
                type = operation;
                obj.comment = null;
                obj.version = 0;
                obj.url = null;
            } else {
                type = operation.type;
                obj.comment = operation.comment;
                obj.version = operation.version;
                obj.url = REPOSITORY_URL_BASE + operation.url;
            }

            if (!obj.version)
                obj.version = extension.metadata.version + 1;
            obj.name = extension.metadata.name;
            obj.uuid = uuid;

            switch (type) {
            case 'blacklist':
                toBlacklist.push(obj);
                break;

            case 'upgrade':
                toUpdate.push(obj);
                break;

            case 'downgrade':
                toDowngrade.push(obj);
                break;

            default:
                log('Unknown operation %s for extension %s'.
                    format(operation, uuid));
                break;
            }
        }

        if (toBlacklist.length > 0)
            _notifyForBlacklisted(toBlacklist);
        if (toDowngrade.length > 0)
            _notifyForDowngraded(toDowngrade);
        if (toUpdate.length > 0)
            _notifyForUpdates(toUpdate);
    });
}

const InstallExtensionDialog = new Lang.Class({
    Name: 'InstallExtensionDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(uuid, info, invocation) {
        this.parent({ styleClass: 'extension-dialog' });

        this._uuid = uuid;
        this._info = info;
        this._invocation = invocation;

        this.setButtons([{ label: _("Cancel"),
                           action: Lang.bind(this, this._onCancelButtonPressed),
                           key:    Clutter.Escape
                         },
                         { label:  _("Install"),
                           action: Lang.bind(this, this._onInstallButtonPressed),
                           default: true
                         }]);

        let message = _("Download and install '%s' from extensions.gnome.org?").format(info.name);

        let box = new St.BoxLayout();
        this.contentLayout.add(box);

        let gicon = new Gio.FileIcon({ file: Gio.File.new_for_uri(REPOSITORY_URL_BASE + info.icon) })
        let icon = new St.Icon({ gicon: gicon });
        box.add(icon);

        let label = new St.Label({ text: message });
        box.add(label);
    },

    _onCancelButtonPressed: function(button, event) {
        this.close();
        this._invocation.return_value(GLib.Variant.new('(s)', ['cancelled']));
    },

    _onInstallButtonPressed: function(button, event) {
        let params = { shell_version: Config.PACKAGE_VERSION };

        let url = REPOSITORY_URL_DOWNLOAD.format(this._uuid);
        let message = Soup.form_request_new_from_hash('GET', url, params);

        let uuid = this._uuid;
        let dir = Gio.File.new_for_path(GLib.build_filenamev([global.userdatadir, 'extensions', uuid]));
        let invocation = this._invocation;
        function errback(code, message) {
            invocation.return_dbus_error('org.gnome.Shell.' + code, message ? message.toString() : '');
        }

        function callback() {
            // Add extension to 'enabled-extensions' for the user, always...
            let enabledExtensions = global.settings.get_strv(ExtensionSystem.ENABLED_EXTENSIONS_KEY);
            if (enabledExtensions.indexOf(uuid) == -1) {
                enabledExtensions.push(uuid);
                global.settings.set_strv(ExtensionSystem.ENABLED_EXTENSIONS_KEY, enabledExtensions);
            }

            let extension = ExtensionUtils.createExtensionObject(uuid, dir, ExtensionUtils.ExtensionType.PER_USER);

            try {
                ExtensionSystem.loadExtension(extension);
            } catch(e) {
                uninstallExtension(uuid);
                errback('LoadExtensionError', e);
                return;
            }

            invocation.return_value(GLib.Variant.new('(s)', 'successful'));
        }

        _httpSession.queue_message(message, Lang.bind(this, function(session, message) {
            gotExtensionZipFile(session, message, uuid, dir, callback, errback);
        }));

        this.close();
    }
});

function init() {
    _httpSession = new Soup.SessionAsync({ ssl_use_system_ca_file: true });

    // See: https://bugzilla.gnome.org/show_bug.cgi?id=655189 for context.
    // _httpSession.add_feature(new Soup.ProxyResolverDefault());
    Soup.Session.prototype.add_feature.call(_httpSession, new Soup.ProxyResolverDefault());
}
