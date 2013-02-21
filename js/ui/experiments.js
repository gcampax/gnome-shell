// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Meta = imports.gi.Meta;

const Tweener = imports.ui.tweener;
const Params = imports.misc.params;

function _interpolate(from, to, progress) {
    return from * (1 - progress) + to * progress;
}

function _updateMoveResize(window, progress, params) {
    let targetX = params.x >= 0 ? params.x : params.baseRect.x;
    let targetY = params.y >= 0 ? params.y : params.baseRect.y;
    let targetW = params.width >= 0 ? params.width : params.baseRect.width;
    let targetH = params.height >= 0 ? params.height : params.baseRect.height;

    window.move_resize_frame(true,
                             _interpolate(params.baseRect.x, targetX, progress),
                             _interpolate(params.baseRect.y, targetX, progress),
                             _interpolate(params.baseRect.width, targetW, progress),
                             _interpolate(params.baseRect.height, targetH, progress));
}

function animateMoveResize(window, params) {
    Params.parse(params, { time: 0.5,
                           transition: 'easeOutQuad',
                           width: -1,
                           height: -1,
                           x: -1,
                           y: -1 });
    params.baseRect = window.get_outer_rect();

    window._animationProgress = 0;
    Tweener.addTween(window,
                     { time: params.time,
                       transition: params.transition,
                       _animationProgress: 1,
                       onUpdate: function() {
                           return _updateMoveResize(this,
                                                    this._animationProgress,
                                                    params);
                       }
                     });
}
