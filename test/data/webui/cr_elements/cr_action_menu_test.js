// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';

import {AnchorAlignment, ShowAtPositionConfig} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise, flushTasks} from '../test_util.m.js';
// clang-format on

/**
 * @fileoverview Tests for cr-action-menu element. Runs as an interactive UI
 * test, since many of these tests check focus behavior.
 */
suite('CrActionMenu', function() {
  /** @type {!CrActionMenuElement} */
  let menu;

  /** @type {!HTMLDialogElement} */
  let dialog;

  /** @type {!NodeList<!Element>} */
  let items;

  /** @type {!HTMLElement} */
  let dots;

  /** @type {HTMLElement} */
  let container = null;

  /** @type {Element} */
  let checkboxFocusableElement = null;

  /** @override */
  suiteSetup(() => {
  });

  setup(function() {
    FocusOutlineManager.forDocument(document).visible = false;
    document.body.innerHTML = `
      <button id="dots">...</button>
      <cr-action-menu>
        <button class="dropdown-item">Un</button>
        <hr>
        <button class="dropdown-item">Dos</button>
        <cr-checkbox class="dropdown-item">Tres</cr-checkbox>
      </cr-action-menu>
    `;

    menu = /** @type {!CrActionMenuElement} */ (
        document.querySelector('cr-action-menu'));
    dialog = menu.getDialog();
    items = menu.querySelectorAll('.dropdown-item');
    checkboxFocusableElement =
        /** @type {!CrCheckboxElement} */ (items[2]).getFocusableElement();
    dots = /** @type {!HTMLElement} */ (document.querySelector('#dots'));
    assertEquals(3, items.length);
  });

  teardown(function() {
    document.body.style.direction = 'ltr';

    if (dialog.open) {
      menu.close();
    }
  });

  function down() {
    keyDownOn(menu, 0, [], 'ArrowDown');
  }

  function up() {
    keyDownOn(menu, 0, [], 'ArrowUp');
  }

  function enter() {
    keyDownOn(menu, 0, [], 'Enter');
  }

  test('open-changed event fires', async function() {
    let whenFired = eventToPromise('open-changed', menu);
    menu.showAt(dots);
    let event = await whenFired;
    assertTrue(event.detail.value);

    whenFired = eventToPromise('open-changed', menu);
    menu.close();
    event = await whenFired;
    assertFalse(event.detail.value);
  });

  test('close event bubbles', function() {
    menu.showAt(dots);
    const whenFired = eventToPromise('close', menu);
    menu.close();
    return whenFired;
  });

  test('hidden or disabled items', function() {
    menu.showAt(dots);
    down();
    assertEquals(items[0], getDeepActiveElement());

    menu.close();
    items[0].hidden = true;
    menu.showAt(dots);
    down();
    assertEquals(items[1], getDeepActiveElement());

    menu.close();
    items[1].disabled = true;
    menu.showAt(dots);
    down();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
  });

  test('focus after down/up arrow', function() {
    menu.showAt(dots);

    // The menu should be focused when shown, but not on any of the items.
    assertEquals(menu, document.activeElement);
    assertNotEquals(items[0], getDeepActiveElement());
    assertNotEquals(items[1], getDeepActiveElement());
    assertNotEquals(checkboxFocusableElement, getDeepActiveElement());

    down();
    assertEquals(items[0], getDeepActiveElement());
    down();
    assertEquals(items[1], getDeepActiveElement());
    down();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
    down();
    assertEquals(items[0], getDeepActiveElement());
    up();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
    up();
    assertEquals(items[1], getDeepActiveElement());
    up();
    assertEquals(items[0], getDeepActiveElement());
    up();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());

    items[1].disabled = true;
    up();
    assertEquals(items[0], getDeepActiveElement());
  });

  test('focus skips cr-checkbox when disabled or hidden', () => {
    menu.showAt(dots);
    const crCheckbox = document.querySelector('cr-checkbox');
    assertEquals(items[2], crCheckbox);

    // Check checkbox is focusable when not disabled or hidden.
    down();
    assertEquals(items[0], getDeepActiveElement());
    down();
    assertEquals(items[1], getDeepActiveElement());
    down();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());

    // Check checkbox is not focusable when either disabled or hidden.
    [[false, true],
     [true, false],
     [true, true],
    ].forEach(([disabled, hidden]) => {
      crCheckbox.disabled = disabled;
      crCheckbox.hidden = hidden;
      getDeepActiveElement().blur();
      down();
      assertEquals(items[0], getDeepActiveElement());
      down();
      assertEquals(items[1], getDeepActiveElement());
      down();
      assertEquals(items[0], getDeepActiveElement());
    });
  });

  test('pressing up arrow when no focus will focus last item', function() {
    menu.showAt(dots);
    assertEquals(menu, document.activeElement);

    up();
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
  });

  test('pressing enter when no focus', function() {
    if (isWindows || isMac) {
      return testFocusAfterClosing('Enter');
    }

    // First item is selected
    menu.showAt(dots);
    assertEquals(menu, document.activeElement);
    enter();
    assertEquals(items[0], getDeepActiveElement());
  });

  test('pressing enter when when item has focus', function() {
    menu.showAt(dots);
    down();
    enter();
    assertEquals(items[0], getDeepActiveElement());
  });

  test('can navigate to dynamically added items', async function() {
    // Can modify children after attached() and before showAt().
    const item = document.createElement('button');
    item.classList.add('dropdown-item');
    menu.insertBefore(item, items[0]);
    menu.showAt(dots);
    await flushTasks();

    down();
    assertEquals(item, getDeepActiveElement());
    down();
    assertEquals(items[0], getDeepActiveElement());

    // Can modify children while menu is open.
    menu.removeChild(item);

    up();
    // Focus should have wrapped around to final item.
    assertEquals(checkboxFocusableElement, getDeepActiveElement());
  });

  test('close on click away', function() {
    menu.showAt(dots);
    assertTrue(dialog.open);
    menu.click();
    assertFalse(dialog.open);
  });

  test('close on resize', function() {
    menu.showAt(dots);
    assertTrue(dialog.open);

    window.dispatchEvent(new CustomEvent('resize'));
    assertFalse(dialog.open);
  });

  test('close on popstate', function() {
    menu.showAt(dots);
    assertTrue(dialog.open);

    window.dispatchEvent(new CustomEvent('popstate'));
    assertFalse(dialog.open);
  });

  /** @param {string} key The key to use for closing. */
  function testFocusAfterClosing(key) {
    return new Promise(function(resolve) {
      menu.showAt(dots);
      assertTrue(dialog.open);

      let anchorHasFocus = false;
      let tabkeyCloseEventFired = false;

      const checkTestDone = () => {
        assertFalse(dialog.open);
        if (key !== 'Tab') {
          resolve();
        } else if (anchorHasFocus && tabkeyCloseEventFired) {
          resolve();
        }
      };

      // Check that focus returns to the anchor element.
      dots.addEventListener('focus', () => {
        anchorHasFocus = true;
        checkTestDone();
      });

      // Check that a Tab key close fires a custom event.
      menu.addEventListener('tabkeyclose', () => {
        tabkeyCloseEventFired = true;
        checkTestDone();
      });

      keyDownOn(menu, 0, [], key);
    });
  }

  test('close on Tab', () => testFocusAfterClosing('Tab'));

  test('close on Escape', () => testFocusAfterClosing('Escape'));

  /** @param {!EventTarget} eventTarget */
  function dispatchMouseoverEvent(eventTarget) {
    eventTarget.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));
  }

  test('moving mouse on option 1 should focus it', () => {
    menu.showAt(dots);
    assertNotEquals(items[0], getDeepActiveElement());
    dispatchMouseoverEvent(items[0]);
    assertEquals(items[0], getDeepActiveElement());
  });

  test('moving mouse on the menu (not on option) should focus the menu', () => {
    menu.showAt(dots);
    items[0].focus();
    dispatchMouseoverEvent(menu);
    assertEquals(dialog.querySelector('[role="menu"]'), getDeepActiveElement());
  });

  test('moving mouse on a disabled item should focus the menu', () => {
    menu.showAt(dots);
    items[2].toggleAttribute('disabled', true);
    items[0].focus();
    dispatchMouseoverEvent(items[2]);
    assertEquals(dialog.querySelector('[role="menu"]'), getDeepActiveElement());
  });

  test('mouse movements should override keyboard focus', () => {
    menu.showAt(dots);
    items[0].focus();
    down();
    assertEquals(items[1], getDeepActiveElement());
    dispatchMouseoverEvent(items[0]);
    assertEquals(items[0], getDeepActiveElement());
  });

  test('items automatically given accessibility role', async function() {
    const newItem = document.createElement('button');
    newItem.classList.add('dropdown-item');

    items[1].setAttribute('role', 'checkbox');
    menu.showAt(dots);

    await flushTasks();
    assertEquals('menuitem', items[0].getAttribute('role'));
    assertEquals('checkbox', items[1].getAttribute('role'));

    menu.insertBefore(newItem, items[0]);
    await flushTasks();
    assertEquals('menuitem', newItem.getAttribute('role'));
  });

  test('positioning', function() {
    // A 40x10 box at (100, 250).
    const config = {
      left: 100,
      top: 250,
      width: 40,
      height: 10,
      maxX: 1000,
      maxY: 2000,
    };

    // Show right and bottom aligned by default.
    menu.showAtPosition(config);
    assertTrue(dialog.open);
    assertEquals('100px', dialog.style.left);
    assertEquals('250px', dialog.style.top);
    menu.close();

    // Center the menu horizontally.
    menu.showAtPosition(
        /** @type {!ShowAtPositionConfig} */ (Object.assign({}, config, {
          anchorAlignmentX: AnchorAlignment.CENTER,
        })));
    const menuWidth = dialog.offsetWidth;
    const menuHeight = dialog.offsetHeight;
    assertEquals(`${120 - menuWidth / 2}px`, dialog.style.left);
    assertEquals('250px', dialog.style.top);
    menu.close();

    // Center the menu in both axes.
    menu.showAtPosition(
        /** @type {!ShowAtPositionConfig} */ (Object.assign({}, config, {
          anchorAlignmentX: AnchorAlignment.CENTER,
          anchorAlignmentY: AnchorAlignment.CENTER,
        })));
    assertEquals(`${120 - menuWidth / 2}px`, dialog.style.left);
    assertEquals(`${255 - menuHeight / 2}px`, dialog.style.top);
    menu.close();

    // Left and top align the menu.
    menu.showAtPosition(
        /** @type {!ShowAtPositionConfig} */ (Object.assign({}, config, {
          anchorAlignmentX: AnchorAlignment.BEFORE_END,
          anchorAlignmentY: AnchorAlignment.BEFORE_END,
        })));
    assertEquals(`${140 - menuWidth}px`, dialog.style.left);
    assertEquals(`${260 - menuHeight}px`, dialog.style.top);
    menu.close();

    // Being left and top aligned at (0, 0) should anchor to the bottom right.
    menu.showAtPosition(
        /** @type {!ShowAtPositionConfig} */ (Object.assign({}, config, {
          anchorAlignmentX: AnchorAlignment.BEFORE_END,
          anchorAlignmentY: AnchorAlignment.BEFORE_END,
          left: 0,
          top: 0,
        })));
    assertEquals(`0px`, dialog.style.left);
    assertEquals(`0px`, dialog.style.top);
    menu.close();

    // Being aligned to a point in the bottom right should anchor to the top
    // left.
    menu.showAtPosition({
      left: 1000,
      top: 2000,
      maxX: 1000,
      maxY: 2000,
    });
    assertEquals(`${1000 - menuWidth}px`, dialog.style.left);
    assertEquals(`${2000 - menuHeight}px`, dialog.style.top);
    menu.close();

    // If the viewport can't fit the menu, align the menu to the viewport.
    menu.showAtPosition({
      left: menuWidth - 5,
      top: 0,
      width: 0,
      height: 0,
      maxX: menuWidth * 2 - 10,
    });
    assertEquals(`${menuWidth - 10}px`, dialog.style.left);
    assertEquals(`0px`, dialog.style.top);
    menu.close();

    // Alignment is reversed in RTL.
    document.body.style.direction = 'rtl';
    menu.showAtPosition(config);
    assertTrue(dialog.open);
    assertEquals(140 - menuWidth, dialog.offsetLeft);
    assertEquals('250px', dialog.style.top);
    menu.close();
  });

  /** @suppress {missingProperties} */
  (function() {
    // TODO(dpapad): fix flakiness and re-enable this test.
    test.skip(
        '[auto-reposition] enables repositioning if content changes',
        function(done) {
          menu.autoReposition = true;

          dots.style.marginLeft = '800px';

          const dotsRect = dots.getBoundingClientRect();

          // Anchored at right-top by default.
          menu.showAt(dots);
          assertTrue(dialog.open);
          let menuRect = menu.getBoundingClientRect();
          assertEquals(
              Math.round(dotsRect.left + dotsRect.width),
              Math.round(menuRect.left + menuRect.width));
          assertEquals(dotsRect.top, menuRect.top);

          const lastMenuLeft = menuRect.left;
          const lastMenuWidth = menuRect.width;

          menu.addEventListener('cr-action-menu-repositioned', () => {
            assertTrue(dialog.open);
            menuRect = menu.getBoundingClientRect();
            // Test that menu width got larger.
            assertTrue(menuRect.width > lastMenuWidth);
            // Test that menu upper-left moved further left.
            assertTrue(menuRect.left < lastMenuLeft);
            // Test that right and top did not move since it is anchored there.
            assertEquals(
                Math.round(dotsRect.left + dotsRect.width),
                Math.round(menuRect.left + menuRect.width));
            assertEquals(dotsRect.top, menuRect.top);
            done();
          });

          // Still anchored at the right place after content size changes.
          items[0].textContent = 'this is a long string to make menu wide';
        });
  })();

  suite('offscreen scroll positioning', function() {
    const bodyHeight = 10000;
    const bodyWidth = 20000;
    const containerLeft = 5000;
    const containerTop = 10000;
    const containerWidth = 500;
    const containerHeight = 500;

    suiteSetup(function() {
      document.body.innerHTML = `
        <dom-module id="test-element">
          <template>
            <style>
              #container {
                overflow: auto;
                position: absolute;
                top: ${containerTop}px;
                left: ${containerLeft}px;
                right: ${containerLeft}px;
                height: ${containerHeight}px;
                width: ${containerWidth}px;
              }

              #inner-container {
                height: 1000px;
                width: 1000px;
              }
            </style>
            <div id="container">
              <div id="inner-container">
                <button id="dots">...</button>
                <cr-action-menu>
                  <button class="dropdown-item">Un</button>
                  <hr>
                  <button class="dropdown-item">Dos</button>
                  <button class="dropdown-item">Tres</button>
                </cr-action-menu>
              </div>
            </div>
          </template>
        </dom-module>
      `;

      Polymer({
        is: 'test-element',
      });
    });

    setup(function() {
      document.body.scrollTop = 0;
      document.body.scrollLeft = 0;
      document.body.innerHTML = `
        <style>
          test-element {
            height: ${bodyHeight}px;
            width: ${bodyWidth}px;
          }
        </style>
        <test-element></test-element>`;

      const testElement = document.querySelector('test-element');
      menu = testElement.root.querySelector('cr-action-menu');
      dialog = menu.getDialog();
      dots = testElement.root.querySelector('#dots');
      container = testElement.root.querySelector('#container');
    });

    // Show the menu, scrolling the body to the button.
    test('simple offscreen', function() {
      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      assertEquals(`${containerLeft}px`, dialog.style.left);
      assertEquals(`${containerTop}px`, dialog.style.top);
      menu.close();
    });

    // Show the menu, scrolling the container to the button, and the body to the
    // button.
    test('offscreen and out of scroll container viewport', function() {
      document.body.scrollLeft = bodyWidth;
      document.body.scrollTop = bodyHeight;

      container.scrollLeft = containerLeft;
      container.scrollTop = containerTop;

      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      assertEquals(`${containerLeft}px`, dialog.style.left);
      assertEquals(`${containerTop}px`, dialog.style.top);
      menu.close();
    });

    // Show the menu for an already onscreen button. The anchor should be
    // overridden so that no scrolling happens.
    test('onscreen forces anchor change', function() {
      const rect = dots.getBoundingClientRect();
      document.body.scrollLeft = rect.right - document.body.clientWidth + 10;
      document.body.scrollTop = rect.bottom - document.body.clientHeight + 10;

      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      const buttonWidth = dots.offsetWidth;
      const buttonHeight = dots.offsetHeight;
      const menuWidth = dialog.offsetWidth;
      const menuHeight = dialog.offsetHeight;
      assertEquals(containerLeft - menuWidth + buttonWidth, dialog.offsetLeft);
      assertEquals(containerTop - menuHeight + buttonHeight, dialog.offsetTop);
      menu.close();
    });

    test('scroll position maintained for showAtPosition', function() {
      document.body.scrollLeft = 500;
      document.body.scrollTop = 1000;
      menu.showAtPosition({top: 50, left: 50});
      assertEquals(550, dialog.offsetLeft);
      assertEquals(1050, dialog.offsetTop);
      menu.close();
    });

    test('rtl', function() {
      // Anchor to an item in RTL.
      document.body.style.direction = 'rtl';
      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      const menuWidth = dialog.offsetWidth;
      assertEquals(
          container.offsetLeft + containerWidth - menuWidth, dialog.offsetLeft);
      assertEquals(containerTop, dialog.offsetTop);
      menu.close();
    });

    test('FocusFirstItemWhenOpenedWithKeyboard', async () => {
      FocusOutlineManager.forDocument(document).visible = true;
      menu.showAtPosition({top: 50, left: 50});
      await new Promise(resolve => requestAnimationFrame(resolve));
      assertEquals(
          menu.querySelector('.dropdown-item'), getDeepActiveElement());
    });
  });
});
