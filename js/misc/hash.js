// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const System = imports.system;

const HashTable = new Lang.Class({
    Name: 'HashTable',

    _init: function(hashKey, equalKey) {
        this.hashKey = hashKey || System.addressOf;
        this.equalKey = equalKey || function(a, b) { return a == b; };

        this._pool = { };
    },

    lookup: function(key) {
        let hash = this.hashKey(key);
        let node = this._pool[hash];

        if (node && this.equalKey(node.key, key))
            return node.value;
        else
            return null;
    },

    replace: function(key, value) {
        let hash = this.hashKey(key);
        let node = this._pool[hash];

        if (node) {
            node.key = key;
            node.value = value;
        } else {
            this._pool[hash] = { key: key, value: value };
        }
    },

    remove: function(key) {
        let hash = this.hashKey(key);
        let node = this._pool[hash];

        if (node && this.equalKey(node.key, key)) {
            delete this._pool[hash];
            return [node.key, node.value];
        } else {
            return [null, null];
        }
    },

    contains: function(key) {
        return this.lookup(key) !== null;
    },

    forEach: function(callback, scope) {
        for (let hash in this._pool) {
            let node = this._pool[hash];

            callback.call(scope, node.key, node.value);
        }
    },

    get size() {
        return Object.getOwnPropertyNames(this._pool).length;
    },
});
