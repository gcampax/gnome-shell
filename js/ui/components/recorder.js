
const Lang = imports.lang;
const Main = imports.ui.main;

const Gio = imports.gi.Gio;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const Recorder = new Lang.Class({
    Name: 'Recorder',

    _init: function() {
        this._recorderSettings = new Gio.Settings({ schema: 'org.gnome.shell.recorder' });
        this._desktopLockdownSettings = new Gio.Settings({ schema: 'org.gnome.desktop.lockdown' });
        this._bindingSettings = new Gio.Settings({ schema: 'org.gnome.shell.keybindings' });
        this._recorder = null;
    },

    enable: function() {
        Main.wm.addKeybinding('toggle-recording',
                              this._bindingSettings,
                              Meta.KeyBindingFlags.NONE,
                              Shell.KeyBindingMode.ALL,
                              Lang.bind(this, this._toggleRecorder));
    },

    disable: function() {
        Main.wm.removeKeybinding('toggle-recording');
    },

    _ensureRecorder: function() {
        if (this._recorder == null) {
            this._recorder = new Shell.Recorder({ stage: global.stage });
            this._recorder.connect('error', Lang.bind(this, function(recorder, error) {
                Main.notifyError(_("Screen recording failed"), error.message);
            }));
        }

        return this._recorder;
    },

    _toggleRecorder: function() {
        let recorder = this._ensureRecorder();
        if (recorder.is_recording()) {
            recorder.close();
            Meta.enable_unredirect_for_screen(global.screen);
        } else if (!this._desktopLockdownSettings.get_boolean('disable-save-to-disk')) {
            // read the parameters from GSettings always in case they have changed
            recorder.set_framerate(this._recorderSettings.get_int('framerate'));
            /* Translators: this is a filename used for screencast recording */
            // xgettext:no-c-format
            recorder.set_file_template(_("Screencast from %d %t") + '.' + this._recorderSettings.get_string('file-extension'));
            let pipeline = this._recorderSettings.get_string('pipeline');

            if (!pipeline.match(/^\s*$/))
                recorder.set_pipeline(pipeline);
            else
                recorder.set_pipeline(null);

            Meta.disable_unredirect_for_screen(global.screen);
            recorder.record();
        }
    }
});

const Component = Recorder;
