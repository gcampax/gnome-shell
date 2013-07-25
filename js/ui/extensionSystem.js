// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Signals = imports.signals;

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const St = imports.gi.St;

const ExtensionUtils = imports.misc.extensionUtils;
const Main = imports.ui.main;

const ExtensionState = {
    ENABLED: 1,
    DISABLED: 2,
    ERROR: 3,
    OUT_OF_DATE: 4,
    DOWNLOADING: 5,
    INITIALIZED: 6,

    // Used as an error state for operations on unknown extensions,
    // should never be in a real extensionMeta object.
    UNINSTALLED: 99
};

// Arrays of uuids
var enabledExtensions;
// Contains the order that extensions were enabled in.
const extensionOrder = [];

// We don't really have a class to add signals on. So, create
// a simple dummy object, add the signal methods, and export those
// publically.
var _signals = {};
Signals.addSignalMethods(_signals);

const connect = Lang.bind(_signals, _signals.connect);
const disconnect = Lang.bind(_signals, _signals.disconnect);

const ENABLED_EXTENSIONS_KEY = 'enabled-extensions';

// Timeout after which we check for updates if we have outdated extensions
// (in seconds)
const UPDATE_TIMEOUT_FOR_OUTDATED = 10;

// Timeout after which we check for updates at login, in the general case
// Should be long enough not to affect fast logins when the user is in a
// hurry (in seconds)
//
// We only check this once, at login, to avoid continously hitting the
// network
const UPDATE_TIMEOUT_NORMAL = 600;

var initted = false;
var enabled;
var _queueCheckUpdateId = 0;

function _queueCheckUpdate(timeout) {
    if (_queueCheckUpdateId != 0)
        return;

    _queueCheckUpdateId = GLib.timeout_add_seconds(GLib.PRIORITY_LOW, timeout, function() {
        Main.ExtensionDownloader.checkForUpdates();

        _queueCheckUpdateId = 0;
        return false;
    });
}

function disableExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return;

    if (extension.state != ExtensionState.ENABLED)
        return;

    // "Rebase" the extension order by disabling and then enabling extensions
    // in order to help prevent conflicts.

    // Example:
    //   order = [A, B, C, D, E]
    //   user disables C
    //   this should: disable E, disable D, disable C, enable D, enable E

    let orderIdx = extensionOrder.indexOf(uuid);
    let order = extensionOrder.slice(orderIdx + 1);
    let orderReversed = order.slice().reverse();

    for (let i = 0; i < orderReversed.length; i++) {
        let uuid = orderReversed[i];
        try {
            ExtensionUtils.extensions[uuid].stateObj.disable();
        } catch(e) {
            logExtensionError(uuid, e);
        }
    }

    if (extension.stylesheet) {
        let theme = St.ThemeContext.get_for_stage(global.stage).get_theme();
        theme.unload_stylesheet(extension.stylesheet.get_path());
    }

    extension.stateObj.disable();

    for (let i = 0; i < order.length; i++) {
        let uuid = order[i];
        try {
            ExtensionUtils.extensions[uuid].stateObj.enable();
        } catch(e) {
            logExtensionError(uuid, e);
        }
    }

    extensionOrder.splice(orderIdx, 1);

    extension.state = ExtensionState.DISABLED;
    _signals.emit('extension-state-changed', extension);
}

function enableExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return;

    if (extension.state == ExtensionState.INITIALIZED)
        initExtension(uuid);

    if (extension.state != ExtensionState.DISABLED)
        return;

    extensionOrder.push(uuid);

    let stylesheetNames = [global.session_mode + '.css', 'stylesheet.css'];
    for (let i = 0; i < stylesheetNames.length; i++) {
        let stylesheetFile = extension.dir.get_child(stylesheetNames[i]);
        if (stylesheetFile.query_exists(null)) {
            let theme = St.ThemeContext.get_for_stage(global.stage).get_theme();
            theme.load_stylesheet(stylesheetFile.get_path());
            extension.stylesheet = stylesheetFile;
            break;
        }
    }

    extension.stateObj.enable();

    extension.state = ExtensionState.ENABLED;
    _signals.emit('extension-state-changed', extension);
}

function logExtensionError(uuid, error) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return;

    let message = '' + error;

    extension.state = ExtensionState.ERROR;
    if (!extension.errors)
        extension.errors = [];
    extension.errors.push(message);

    log('Extension "%s" had error: %s'.format(uuid, message));
    _signals.emit('extension-state-changed', { uuid: uuid,
                                               error: message,
                                               state: extension.state });
}

function loadExtension(extension) {
    // Default to error, we set success as the last step
    extension.state = ExtensionState.ERROR;

    if (ExtensionUtils.isOutOfDate(extension)) {
        extension.state = ExtensionState.OUT_OF_DATE;

        _queueCheckUpdate(UPDATE_TIMEOUT_FOR_OUTDATED);
    } else {
        let enabled = enabledExtensions.indexOf(extension.uuid) != -1;
        if (enabled) {
            initExtension(extension.uuid);
            if (extension.state == ExtensionState.DISABLED)
                enableExtension(extension.uuid);
        } else {
            extension.state = ExtensionState.INITIALIZED;
        }
    }

    _signals.emit('extension-state-changed', extension);
}

function unloadExtension(extension) {
    // Try to disable it -- if it's ERROR'd, we can't guarantee that,
    // but it will be removed on next reboot, and hopefully nothing
    // broke too much.
    disableExtension(extension.uuid);

    extension.state = ExtensionState.UNINSTALLED;
    _signals.emit('extension-state-changed', extension);

    delete ExtensionUtils.extensions[extension.uuid];
    return true;
}

function reloadExtension(oldExtension) {
    // Grab the things we'll need to pass to createExtensionObject
    // to reload it.
    let { uuid: uuid, dir: dir, type: type } = oldExtension;

    // Then unload the old extension.
    unloadExtension(oldExtension);

    // Now, recreate the extension and load it.
    let newExtension = ExtensionUtils.createExtensionObject(uuid, dir, type);
    loadExtension(newExtension);
}

function initExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    let dir = extension.dir;

    if (!extension)
        throw new Error("Extension was not properly created. Call loadExtension first");

    let extensionJs = dir.get_child('extension.js');
    if (!extensionJs.query_exists(null))
        throw new Error('Missing extension.js');

    let extensionModule;
    let extensionState = null;

    ExtensionUtils.installImporter(extension);
    extensionModule = extension.imports.extension;

    if (extensionModule.init) {
        extensionState = extensionModule.init(extension);
    }

    if (!extensionState)
        extensionState = extensionModule;
    extension.stateObj = extensionState;

    extension.state = ExtensionState.DISABLED;
    _signals.emit('extension-loaded', uuid);
}

function getEnabledExtensions() {
    let extensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);
    if (!Array.isArray(Main.sessionMode.enabledExtensions))
        return extensions;

    return Main.sessionMode.enabledExtensions.concat(extensions);
}

function onEnabledExtensionsChanged() {
    let newEnabledExtensions = getEnabledExtensions();

    if (!enabled)
        return;

    // Find and enable all the newly enabled extensions: UUIDs found in the
    // new setting, but not in the old one.
    newEnabledExtensions.filter(function(uuid) {
        return enabledExtensions.indexOf(uuid) == -1;
    }).forEach(function(uuid) {
        try {
            enableExtension(uuid);
        } catch(e) {
            logExtensionError(uuid, e);
        }
    });

    // Find and disable all the newly disabled extensions: UUIDs found in the
    // old setting, but not in the new one.
    enabledExtensions.filter(function(item) {
        return newEnabledExtensions.indexOf(item) == -1;
    }).forEach(function(uuid) {
        try {
            disableExtension(uuid);
        } catch(e) {
            logExtensionError(uuid, e);
        }
    });

    enabledExtensions = newEnabledExtensions;
}

function _loadExtensions() {
    global.settings.connect('changed::' + ENABLED_EXTENSIONS_KEY, onEnabledExtensionsChanged);
    enabledExtensions = getEnabledExtensions();

    let finder = new ExtensionUtils.ExtensionFinder();
    finder.connect('extension-found', function(signals, extension) {
        try {
            loadExtension(extension);
        } catch(e) {
            logExtensionError(extension.uuid, e);
        }
    });
    finder.connect('extensions-loaded', function() {
        _queueCheckUpdate(UPDATE_TIMEOUT_NORMAL);
    });
    finder.scanExtensions();
}

function enableAllExtensions() {
    if (enabled)
        return;

    if (!initted) {
        _loadExtensions();
        initted = true;
    } else {
        enabledExtensions.forEach(function(uuid) {
            enableExtension(uuid);
        });
    }
    enabled = true;
}

function disableAllExtensions() {
    if (!enabled)
        return;

    if (initted) {
        extensionOrder.slice().reverse().forEach(function(uuid) {
            disableExtension(uuid);
        });
    }

    enabled = false;
}

function _sessionUpdated() {
    // For now sessionMode.allowExtensions controls extensions from both the
    // 'enabled-extensions' preference and the sessionMode.enabledExtensions
    // property; it might make sense to make enabledExtensions independent
    // from allowExtensions in the future
    if (Main.sessionMode.allowExtensions) {
        if (initted)
            onEnabledExtensionsChanged();
        enableAllExtensions();
    } else {
        disableAllExtensions();
    }
}

function init() {
    Main.sessionMode.connect('updated', _sessionUpdated);
    _sessionUpdated();
}
