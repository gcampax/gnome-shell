// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// Common utils for the extension system and the extension
// preferences tool

const Lang = imports.lang;
const Signals = imports.signals;

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const ShellJS = imports.gi.ShellJS;

const Config = imports.misc.config;
const FileUtils = imports.misc.fileUtils;

const ExtensionType = {
    SYSTEM: 1,
    PER_USER: 2
};

// Maps uuid -> metadata object
const extensions = {};

function getCurrentExtension() {
    let stack = (new Error()).stack;

    // Assuming we're importing this directly from an extension (and we shouldn't
    // ever not be), its UUID should be directly in the path here.
    let extensionStackLine = stack.split('\n')[1];
    if (!extensionStackLine)
        throw new Error('Could not find current extension');

    // The stack line is like:
    //   init([object Object])@/home/user/data/gnome-shell/extensions/u@u.id/prefs.js:8
    //
    // In the case that we're importing from
    // module scope, the first field is blank:
    //   @/home/user/data/gnome-shell/extensions/u@u.id/prefs.js:8
    let match = new RegExp('@(.+):\\d+').exec(extensionStackLine);
    if (!match)
        throw new Error('Could not find current extension');

    let path = match[1];
    let file = Gio.File.new_for_path(path);

    // Walk up the directory tree, looking for an extesion with
    // the same UUID as a directory name.
    while (file != null) {
        let extension = extensions[file.get_basename()];
        if (extension !== undefined)
            return extension;
        file = file.get_parent();
    }

    throw new Error('Could not find current extension');
}

/**
 * versionCheck:
 * @required: an array of versions we're compatible with
 * @current: the version we have
 *
 * Check if a component is compatible for an extension.
 * @required is an array, and at least one version must match.
 * @current must be in the format <major>.<minor>.<point>.<micro>
 * <micro> is always ignored
 * <point> is ignored if <minor> is even (so you can target the
 * whole stable release)
 * <minor> and <major> must match
 * Each target version must be at least <major> and <minor>
 */
function versionCheck(required, current) {
    let currentArray = current.split('.');
    let major = currentArray[0];
    let minor = currentArray[1];
    let point = currentArray[2];
    for (let i = 0; i < required.length; i++) {
        let requiredArray = required[i].split('.');
        if (requiredArray[0] == major &&
            requiredArray[1] == minor &&
            (requiredArray[2] == point ||
             (requiredArray[2] == undefined && parseInt(minor) % 2 == 0)))
            return true;
    }
    return false;
}

function isOutOfDate(extension) {
    if (!versionCheck(extension.metadata['shell-version'], Config.PACKAGE_VERSION))
        return true;

    return false;
}

function _parseJSONMetadata(file) {
    let success, contents, tag, meta;

    try {
        [success, contents, tag] = file.load_contents(null);
    } catch(e) {
        if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND))
            throw new Error('Missing metadata.json');
        else
            throw new Error('Failed to load metadata.json: ' + e.message);
    }

    try {
        meta = JSON.parse(contents);
    } catch(e) {
        throw new Error('Failed to parse metadata.json: ' + e.message);
    }

    const requiredProperties = ['uuid', 'name', 'description', 'shell-version'];
    for (let i = 0; i < requiredProperties.length; i++) {
        let prop = requiredProperties[i];
        if (!meta[prop]) {
            throw new Error('missing "' + prop + '" property in metadata.json');
        }
    }

    return meta;
}

const _SHELL_EXTENSION_GROUP = 'Shell Extension';
const _EXTRA_GROUP = 'Extra';

const ExtensionMetadataDesktop = new Lang.Class({
    Name: 'ExtensionMetadataDesktop',

    _init: function(file) {
        let success, contents, tag;

        let keyfile = new GLib.KeyFile();
        try {
            keyfile.load_from_file(file.get_path(), GLib.KeyFileFlags.NONE);
        } catch(e) {
            throw new Error('Failed to load extension.desktop: ' + e.message);
        }

        try {
            this.uuid = keyfile.get_string(_SHELL_EXTENSION_GROUP, 'UUID');
            this.name = keyfile.get_locale_string(_SHELL_EXTENSION_GROUP, 'Name', null);
            // Comment rather than Description because that's what .desktop file use
            this.description = keyfile.get_locale_string(_SHELL_EXTENSION_GROUP, 'Comment', null);
            this['shell-version'] = keyfile.get_string_list(_SHELL_EXTENSION_GROUP, 'ShellVersion');
            try {
                this['js-version'] = keyfile.get_string_list(_SHELL_EXTENSION_GROUP, 'JSVersion');
            } catch(e) { }
        } catch(e) {
            throw new Error('Invalid extension.desktop: ' + e.message);
        }

        if (keyfile.has_group(_EXTRA_GROUP)) {
            let [extraKeys, len] = keyfile.get_keys(_EXTRA_GROUP);

            for (let i = 0; i < extraKeys.length; i++)
                this[extraKeys[i]] = keyfile.get_string(_EXTRA_GROUP, extraKeys[i]);
        }
    }
});

function _createExtensionMetadata(uuid, dir) {
    let metadataJson = dir.get_child('metadata.json');
    let metadataDesktop = dir.get_child('extension.desktop');
    let meta;

    if (metadataDesktop.query_exists(null))
        meta = new ExtensionMetadataDesktop(metadataDesktop);
    else
        meta = _parseJSONMetadata(metadataJson);

    if (uuid != meta.uuid) {
        throw new Error('uuid "' + meta.uuid + '" from metadata file does not match directory name "' + uuid + '"');
    }

    return meta;
}

function createExtensionObject(uuid, dir, type) {
    let extension = {};

    extension.metadata = _createExtensionMetadata(uuid, dir);
    extension.uuid = extension.metadata.uuid;
    extension.type = type;
    extension.dir = dir;
    extension.path = dir.get_path();
    extension.error = '';
    extension.hasPrefs = dir.get_child('prefs.js').query_exists(null);

    extensions[uuid] = extension;

    return extension;
}

var _extension = null;

function installImporter(extension) {
    _extension = extension;
    ShellJS.add_extension_importer('imports.misc.extensionUtils._extension', 'imports', extension.path);
    _extension = null;
}

const ExtensionFinder = new Lang.Class({
    Name: 'ExtensionFinder',

    _loadExtension: function(extensionDir, info, perUserDir) {
        let fileType = info.get_file_type();
        if (fileType != Gio.FileType.DIRECTORY)
            return;
        let uuid = info.get_name();
        let existing = extensions[uuid];
        if (existing) {
            log('Extension %s already installed in %s. %s will not be loaded'.format(uuid, existing.path, extensionDir.get_path()));
            return;
        }

        let extension;
        let type = extensionDir.has_prefix(perUserDir) ? ExtensionType.PER_USER
                                                       : ExtensionType.SYSTEM;
        try {
            extension = createExtensionObject(uuid, extensionDir, type);
        } catch(e) {
            logError(e, 'Could not load extension %s'.format(uuid));
            return;
        }
        this.emit('extension-found', extension);
    },

    _extensionsLoaded: function() {
        this.emit('extensions-loaded');
    },

    scanExtensions: function() {
        let perUserDir = Gio.File.new_for_path(global.userdatadir);
        FileUtils.collectFromDatadirsAsync('extensions',
                                           { processFile: Lang.bind(this, this._loadExtension),
                                             loadedCallback: Lang.bind(this, this._extensionsLoaded),
                                             includeUserDir: true,
                                             data: perUserDir });
    }
});
Signals.addSignalMethods(ExtensionFinder.prototype);
