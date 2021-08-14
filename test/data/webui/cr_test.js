// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EventTarget;

function setUp() {
  EventTarget = cr.EventTarget;
}

function testDefineProperty() {
  var obj = new EventTarget;
  cr.defineProperty(obj, 'test');

  obj.test = 1;
  assertEquals(1, obj.test);
  assertEquals(1, obj.test_);
}

function testDefinePropertyOnClass() {
  class C extends EventTarget {}

  cr.defineProperty(C, 'test');

  var obj = new C;
  assertEquals(undefined, obj.test);

  obj.test = 1;
  assertEquals(1, obj.test);
  assertEquals(1, obj.test_);
}

function testDefinePropertyWithSetter() {
  var obj = new EventTarget;

  var hit = false;
  function onTestSet(value, oldValue) {
    assertEquals(obj, this);
    assertEquals(2, this.test);
    assertEquals(undefined, oldValue);
    assertEquals(2, value);
    hit = true;
  }
  cr.defineProperty(obj, 'test', cr.PropertyKind.JS, onTestSet);
  obj.test = 2;
  assertTrue(hit);
}

function testDefinePropertyEvent() {
  var obj = new EventTarget;
  cr.defineProperty(obj, 'test');
  obj.test = 1;

  var count = 0;
  function f(e) {
    assertEquals('testChange', e.type);
    assertEquals('test', e.propertyName);
    assertEquals(1, e.oldValue);
    assertEquals(2, e.newValue);
    count++;
  }

  obj.addEventListener('testChange', f);
  obj.test = 2;
  assertEquals(2, obj.test);
  assertEquals(1, count, 'Should have called the property change listener');

  obj.test = 2;
  assertEquals(1, count);
}

function testDefinePropertyEventWithDefault() {
  var obj = new EventTarget;
  cr.defineProperty(obj, 'test', cr.PropertyKind.JS);

  var count = 0;
  function f(e) {
    assertEquals('testChange', e.type);
    assertEquals('test', e.propertyName);
    assertEquals(undefined, e.oldValue);
    assertEquals(2, e.newValue);
    count++;
  }

  obj.addEventListener('testChange', f);

  obj.test = undefined;
  assertEquals(0, count, 'Should not have called the property change listener');

  obj.test = 2;
  assertEquals(2, obj.test);
  assertEquals(1, count, 'Should have called the property change listener');

  obj.test = 2;
  assertEquals(1, count);
}

function testDefinePropertyAttr() {
  var obj = document.createElement('div');
  cr.defineProperty(obj, 'test', cr.PropertyKind.ATTR);

  obj.test = 'a';
  assertEquals('a', obj.test);
  assertEquals('a', obj.getAttribute('test'));

  obj.test = undefined;
  assertEquals(null, obj.test);
  assertFalse(obj.hasAttribute('test'));
}

function testDefinePropertyAttrOnClass() {
  var obj = document.createElement('button');
  cr.defineProperty(HTMLButtonElement, 'test', cr.PropertyKind.ATTR);

  assertEquals(null, obj.test);

  obj.test = 'a';
  assertEquals('a', obj.test);
  assertEquals('a', obj.getAttribute('test'));

  obj.test = undefined;
  assertEquals(null, obj.test);
  assertFalse(obj.hasAttribute('test'));
}

function testDefinePropertyAttrWithSetter() {
  var obj = document.createElement('div');

  var hit = false;

  function onTestSet(value, oldValue) {
    assertEquals(obj, this);
    assertEquals(null, oldValue);
    assertEquals('b', value);
    assertEquals('b', this.test);
    hit = true;
  }
  cr.defineProperty(obj, 'test', cr.PropertyKind.ATTR, onTestSet);
  obj.test = 'b';
  assertTrue(hit);
}

function testDefinePropertyAttrEvent() {
  var obj = document.createElement('div');
  cr.defineProperty(obj, 'test', cr.PropertyKind.ATTR);

  var count = 0;
  function f(e) {
    assertEquals('testChange', e.type);
    assertEquals('test', e.propertyName);
    assertEquals(null, e.oldValue);
    assertEquals('b', e.newValue);
    count++;
  }

  obj.addEventListener('testChange', f);

  obj.test = null;
  assertEquals(0, count, 'Should not have called the property change listener');

  obj.test = 'b';
  assertEquals('b', obj.test);
  assertEquals(1, count, 'Should have called the property change listener');

  obj.test = 'b';
  assertEquals(1, count);
}

function testDefinePropertyBoolAttr() {
  var obj = document.createElement('div');
  cr.defineProperty(obj, 'test', cr.PropertyKind.BOOL_ATTR);

  assertFalse(obj.test);
  assertFalse(obj.hasAttribute('test'));

  obj.test = true;
  assertTrue(obj.test);
  assertTrue(obj.hasAttribute('test'));

  obj.test = false;
  assertFalse(obj.test);
  assertFalse(obj.hasAttribute('test'));
}

function testDefinePropertyBoolAttrEvent() {
  var obj = document.createElement('div');
  cr.defineProperty(obj, 'test', cr.PropertyKind.BOOL_ATTR);

  var count = 0;
  function f(e) {
    assertEquals('testChange', e.type);
    assertEquals('test', e.propertyName);
    assertEquals(false, e.oldValue);
    assertEquals(true, e.newValue);
    count++;
  }

  obj.addEventListener('testChange', f);
  obj.test = true;
  assertTrue(obj.test);
  assertEquals(1, count, 'Should have called the property change listener');

  obj.test = true;
  assertEquals(1, count);
}

function testDefinePropertyBoolAttrEventWithHook() {
  var obj = document.createElement('div');
  var hit = false;

  function onTestSet(value, oldValue) {
    assertEquals(obj, this);
    assertTrue(this.test);
    assertFalse(oldValue);
    assertTrue(value);
    hit = true;
  }
  cr.defineProperty(obj, 'test', cr.PropertyKind.BOOL_ATTR, onTestSet);
  obj.test = true;
  assertTrue(hit);
}

function testAddSingletonGetter() {
  function Foo() {}
  cr.addSingletonGetter(Foo);

  assertEquals(
      'function', typeof Foo.getInstance, 'Should add get instance function');

  var x = Foo.getInstance();
  assertEquals('object', typeof x, 'Should successfully create an object');
  assertNotEqual(null, x, 'Created object should not be null');

  var y = Foo.getInstance();
  assertEquals(x, y, 'Should return the same object');

  delete Foo.instance_;

  var z = Foo.getInstance();
  assertEquals('object', typeof z, 'Should work after clearing for testing');
  assertNotEqual(null, z, 'Created object should not be null');

  assertNotEqual(
      x, z, 'Should return a different object after clearing for testing');
}

function testDefineWithGetter() {
  var v = 0;
  cr.define('foo', function() {
    return {
      get v() {
        return v;
      }
    };
  });

  assertEquals(0, foo.v);

  v = 1;
  assertEquals(1, foo.v);
}
