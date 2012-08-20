// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const St  = imports.gi.St;

const Tweener = imports.ui.tweener;

const UI = imports.testcommon.ui;

function test() {
    let stage = new Clutter.Stage();
    UI.init(stage);

    let container = new St.Bin({ style: 'border: 1px solid blue;' });
    let widget = new St.Label({ width: 100,
                                height: 100,
                                style: 'background-color: red',
                                text: "Text!" });
    stage.add_actor(container);
    container.set_child(widget);

    let effect = new Shell.MaskEffect({ enabled: true });
    effect.connect('build-mask', function(effect, cr) {
        cr.moveTo(10, 10);
        cr.arc(50, 50, 40, -Math.PI, 0);
        cr.closePath();
        cr.fill();

        return true;
    });

    Tweener.addTween(effect,
                     { vertical_offset: 0.01,
                       time: 10,
                       transition: 'easeOutQuad' });

    widget.add_effect(effect);

    UI.main(stage);
}
test();
