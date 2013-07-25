// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GdkPixbuf = imports.gi.GdkPixbuf;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const FileUtils = imports.misc.fileUtils;
const Search = imports.ui.search;

const KEY_FILE_GROUP = 'Shell Search Provider';

const SearchProviderIface = <interface name="org.gnome.Shell.SearchProvider">
<method name="GetInitialResultSet">
    <arg type="as" direction="in" />
    <arg type="as" direction="out" />
</method>
<method name="GetSubsearchResultSet">
    <arg type="as" direction="in" />
    <arg type="as" direction="in" />
    <arg type="as" direction="out" />
</method>
<method name="GetResultMetas">
    <arg type="as" direction="in" />
    <arg type="aa{sv}" direction="out" />
</method>
<method name="ActivateResult">
    <arg type="s" direction="in" />
</method>
</interface>;

const SearchProvider2Iface = <interface name="org.gnome.Shell.SearchProvider2">
<method name="GetInitialResultSet">
    <arg type="as" direction="in" />
    <arg type="as" direction="out" />
</method>
<method name="GetSubsearchResultSet">
    <arg type="as" direction="in" />
    <arg type="as" direction="in" />
    <arg type="as" direction="out" />
</method>
<method name="GetResultMetas">
    <arg type="as" direction="in" />
    <arg type="aa{sv}" direction="out" />
</method>
<method name="ActivateResult">
    <arg type="s" direction="in" />
    <arg type="as" direction="in" />
    <arg type="u" direction="in" />
</method>
<method name="LaunchSearch">
    <arg type="as" direction="in" />
    <arg type="u" direction="in" />
</method>
</interface>;

var SearchProviderProxy = Gio.DBusProxy.makeProxyWrapper(SearchProviderIface);
var SearchProvider2Proxy = Gio.DBusProxy.makeProxyWrapper(SearchProvider2Iface);

function loadRemoteSearchProviders(addProviderCallback) {
    let data = { loadedProviders: [],
                 objectPaths: {},
                 addProviderCallback: addProviderCallback };
    FileUtils.collectFromDatadirsAsync('search-providers',
                                       { loadedCallback: remoteProvidersLoaded,
                                         processFile: loadRemoteSearchProvider,
                                         data: data
                                       });
}

function loadRemoteSearchProvider(file, info, data) {
    let keyfile = new GLib.KeyFile();
    let path = file.get_path();

    try {
        keyfile.load_from_file(path, 0);
    } catch(e) {
        return;
    }

    if (!keyfile.has_group(KEY_FILE_GROUP))
        return;

    let remoteProvider;
    try {
        let group = KEY_FILE_GROUP;
        let busName = keyfile.get_string(group, 'BusName');
        let objectPath = keyfile.get_string(group, 'ObjectPath');

        if (data.objectPaths[objectPath])
            return;

        let appInfo = null;
        try {
            let desktopId = keyfile.get_string(group, 'DesktopId');
            appInfo = Gio.DesktopAppInfo.new(desktopId);
        } catch (e) {
            log('Ignoring search provider ' + path + ': missing DesktopId');
            return;
        }

        let version = '1';
        try {
            version = keyfile.get_string(group, 'Version');
        } catch (e) {
            // ignore error
        }

        if (version >= 2)
            remoteProvider = new RemoteSearchProvider2(appInfo, busName, objectPath);
        else
            remoteProvider = new RemoteSearchProvider(appInfo, busName, objectPath);

        data.objectPaths[objectPath] = remoteProvider;
        data.loadedProviders.push(remoteProvider);
    } catch(e) {
        log('Failed to add search provider %s: %s'.format(path, e.toString()));
    }
}

function remoteProvidersLoaded(loadState) {
    let searchSettings = new Gio.Settings({ schema: Search.SEARCH_PROVIDERS_SCHEMA });
    let sortOrder = searchSettings.get_strv('sort-order');

    // Special case gnome-control-center to be always active and always first
    sortOrder.unshift('gnome-control-center.desktop');

    loadState.loadedProviders.sort(
        function(providerA, providerB) {
            let idxA, idxB;
            let appIdA, appIdB;

            appIdA = providerA.appInfo.get_id();
            appIdB = providerB.appInfo.get_id();

            idxA = sortOrder.indexOf(appIdA);
            idxB = sortOrder.indexOf(appIdB);

            // if no provider is found in the order, use alphabetical order
            if ((idxA == -1) && (idxB == -1)) {
                let nameA = providerA.appInfo.get_name();
                let nameB = providerB.appInfo.get_name();

                return GLib.utf8_collate(nameA, nameB);
            }

            // if providerA isn't found, it's sorted after providerB
            if (idxA == -1)
                return 1;

            // if providerB isn't found, it's sorted after providerA
            if (idxB == -1)
                return -1;

            // finally, if both providers are found, return their order in the list
            return (idxA - idxB);
        });

    loadState.loadedProviders.forEach(
        function(provider) {
            loadState.addProviderCallback(provider);
        });
}

function _startsWith(str, prefix) {
    return str.substr(0, prefix.length) == prefix;
}

const RemoteSearchProvider = new Lang.Class({
    Name: 'RemoteSearchProvider',

    _init: function(appInfo, dbusName, dbusPath, proxyType) {
        if (!proxyType)
            proxyType = SearchProviderProxy;

        this.proxy = new proxyType(Gio.DBus.session,
                dbusName, dbusPath, Lang.bind(this, this._onProxyConstructed));

        this.appInfo = appInfo;
        this.id = appInfo.get_id();
        this.isRemoteProvider = true;

        this._cancellable = new Gio.Cancellable();
    },

    _onProxyConstructed: function(proxy) {
        // Do nothing
    },

    createIcon: function(size, meta) {
        let gicon;
        if (meta['icon']) {
            gicon = Gio.icon_deserialize(meta['icon']);
        } else if (meta['gicon']) {
            gicon = Gio.icon_new_for_string(meta['gicon']);
        } else if (meta['icon-data']) {
            let [width, height, rowStride, hasAlpha,
                 bitsPerSample, nChannels, data] = meta['icon-data'];
            gicon = Shell.util_create_pixbuf_from_data(data, GdkPixbuf.Colorspace.RGB, hasAlpha,
                                                       bitsPerSample, width, height, rowStride);
        }

        return new St.Icon({ gicon: gicon,
                             icon_size: size });
    },

    filterResults: function(results, maxNumber) {
        if (results.length <= maxNumber)
            return results;

        let regularResults = results.filter(function(r) { return !_startsWith(r, 'special:') });
        let specialResults = results.filter(function(r) { return _startsWith(r, 'special:'); });

        return regularResults.slice(0, maxNumber).concat(specialResults);
    },

    _getResultsFinished: function(results, error) {
        if (error)
            return;
        this.searchSystem.setResults(this, results[0]);
    },

    getInitialResultSet: function(terms) {
        this._cancellable.cancel();
        this._cancellable.reset();
        try {
            this.proxy.GetInitialResultSetRemote(terms,
                                                 Lang.bind(this, this._getResultsFinished),
                                                 this._cancellable);
        } catch(e) {
            log('Error calling GetInitialResultSet for provider %s: %s'.format(this.id, e.toString()));
            this.searchSystem.setResults(this, []);
        }
    },

    getSubsearchResultSet: function(previousResults, newTerms) {
        this._cancellable.cancel();
        this._cancellable.reset();
        try {
            this.proxy.GetSubsearchResultSetRemote(previousResults, newTerms,
                                                   Lang.bind(this, this._getResultsFinished),
                                                   this._cancellable);
        } catch(e) {
            log('Error calling GetSubsearchResultSet for provider %s: %s'.format(this.id, e.toString()));
            this.searchSystem.setResults(this, []);
        }
    },

    _getResultMetasFinished: function(results, error, callback) {
        if (error) {
            callback([]);
            return;
        }
        let metas = results[0];
        let resultMetas = [];
        for (let i = 0; i < metas.length; i++) {
            for (let prop in metas[i]) {
                // we can use the serialized icon variant directly
                if (prop != 'icon')
                    metas[i][prop] = metas[i][prop].deep_unpack();
            }

            resultMetas.push({ id: metas[i]['id'],
                               name: metas[i]['name'],
                               description: metas[i]['description'],
                               createIcon: Lang.bind(this,
                                                     this.createIcon, metas[i]) });
        }
        callback(resultMetas);
    },

    getResultMetas: function(ids, callback) {
        this._cancellable.cancel();
        this._cancellable.reset();
        try {
            this.proxy.GetResultMetasRemote(ids,
                                            Lang.bind(this, this._getResultMetasFinished, callback),
                                            this._cancellable);
        } catch(e) {
            log('Error calling GetResultMetas for provider %s: %s'.format(this.id, e.toString()));
            callback([]);
        }
    },

    activateResult: function(id) {
        this.proxy.ActivateResultRemote(id);
    },

    launchSearch: function(terms) {
        // the provider is not compatible with the new version of the interface, launch
        // the app itself but warn so we can catch the error in logs
        log('Search provider ' + this.appInfo.get_id() + ' does not implement LaunchSearch');
        this.appInfo.launch([], global.create_app_launch_context());
    }
});

const RemoteSearchProvider2 = new Lang.Class({
    Name: 'RemoteSearchProvider2',
    Extends: RemoteSearchProvider,

    _init: function(appInfo, dbusName, dbusPath) {
        this.parent(appInfo, dbusName, dbusPath, SearchProvider2Proxy);

        this.canLaunchSearch = true;
    },

    activateResult: function(id, terms) {
        this.proxy.ActivateResultRemote(id, terms, global.get_current_time());
    },

    launchSearch: function(terms) {
        this.proxy.LaunchSearchRemote(terms, global.get_current_time());
    }
});
