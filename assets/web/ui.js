(function() {
  function activeHtmlElement() {
    return document.activeElement instanceof HTMLElement
      ? document.activeElement
      : null;
  }

  function containsElement(root, element) {
    return !!root && !!element && root.contains(element);
  }

  var keyboardFocusVisible = false;

  function setKeyboardFocusVisible(value) {
    keyboardFocusVisible = value;
    document.documentElement.setAttribute("data-keyboard-focus", value ? "true" : "false");
  }

  function keyName(event) {
    var key = (event.key || "").toLowerCase();
    if (key) {
      if (key === " " || key === "spacebar") {
        return "space";
      }
      if (key === "esc") {
        return "escape";
      }
      if (key === "tab" || key === "u+0009") {
        return "tab";
      }
      return key;
    }

    var keyIdentifier = (event.keyIdentifier || "").toLowerCase();
    if (keyIdentifier === "tab" || keyIdentifier === "u+0009") {
      return "tab";
    }
    if (keyIdentifier === "u+000d") {
      return "enter";
    }
    if (keyIdentifier === "u+0020") {
      return "space";
    }
    if (keyIdentifier === "u+001b") {
      return "escape";
    }

    var code = event.keyCode || event.which || event.charCode || 0;
    if (code === 13) {
      return "enter";
    }
    if (code === 32) {
      return "space";
    }
    if (code === 27) {
      return "escape";
    }
    if (code === 9) {
      return "tab";
    }
    return "";
  }

  function isConfirmKey(event) {
    var key = keyName(event);
    return key === "enter";
  }

  function directionalFocusKey(event) {
    var key = (event.key || "").toLowerCase();
    if (key === "arrowleft" || key === "left") {
      return "arrowleft";
    }
    if (key === "arrowright" || key === "right") {
      return "arrowright";
    }
    if (key === "arrowup" || key === "up") {
      return "arrowup";
    }
    if (key === "arrowdown" || key === "down") {
      return "arrowdown";
    }

    var eventCode = (event.code || "").toLowerCase();
    if (eventCode === "arrowleft") {
      return "arrowleft";
    }
    if (eventCode === "arrowright") {
      return "arrowright";
    }
    if (eventCode === "arrowup") {
      return "arrowup";
    }
    if (eventCode === "arrowdown") {
      return "arrowdown";
    }

    var keyIdentifier = (event.keyIdentifier || "").toLowerCase();
    if (keyIdentifier === "left" || keyIdentifier === "u+0025") {
      return "arrowleft";
    }
    if (keyIdentifier === "right" || keyIdentifier === "u+0027") {
      return "arrowright";
    }
    if (keyIdentifier === "up" || keyIdentifier === "u+0026") {
      return "arrowup";
    }
    if (keyIdentifier === "down" || keyIdentifier === "u+0028") {
      return "arrowdown";
    }

    var code = event.keyCode || event.which || event.charCode || 0;
    if (code === 37) {
      return "arrowleft";
    }
    if (code === 39) {
      return "arrowright";
    }
    if (code === 38) {
      return "arrowup";
    }
    if (code === 40) {
      return "arrowdown";
    }
    return "";
  }

  function moveDirectionalFocus(event, items, options) {
    var settings = options || {};
    var key = directionalFocusKey(event);
    if (key !== settings.previousKey && key !== settings.nextKey) {
      return false;
    }

    var activeIndex = items.indexOf(document.activeElement);
    if (activeIndex < 0 || !keyboardFocusVisible) {
      return false;
    }

    event.preventDefault();
    var step = key === settings.nextKey ? 1 : -1;
    items[(activeIndex + items.length + step) % items.length].focus();
    return true;
  }

  function installButtonConfirmKeys(options) {
    var settings = options || {};
    var target = settings.root || document;
    var activate = settings.activate || function(button) {
      button.click();
    };
    target.addEventListener("keydown", function(event) {
      if (event.defaultPrevented || !isConfirmKey(event)) {
        return;
      }

      var active = activeHtmlElement();
      if (!active || active.tagName !== "BUTTON" || active.disabled) {
        return;
      }

      event.preventDefault();
      activate(active);
    });
  }

  function createModal(options) {
    var modal = options.modal;
    var defaultFocus = options.defaultFocus || null;
    var returnFocus = null;

    function isOpen() {
      return modal.classList.contains("is-active");
    }

    function open() {
      if (isOpen()) {
        return;
      }

      returnFocus = activeHtmlElement();
      modal.classList.add("is-active");
      modal.setAttribute("aria-hidden", "false");
      if (defaultFocus) {
        defaultFocus.focus();
      } else if (returnFocus && typeof returnFocus.blur === "function") {
        returnFocus.blur();
      }
    }

    function close() {
      if (!isOpen()) {
        return;
      }

      modal.classList.remove("is-active");
      modal.setAttribute("aria-hidden", "true");
      if (containsElement(document, returnFocus)) {
        returnFocus.focus();
      }
      returnFocus = null;
    }

    return {
      close: close,
      isOpen: isOpen,
      open: open
    };
  }

  function createConfirmDialog(options) {
    var modal = createModal({
      modal: options.modal
    });
    var cancelButton = options.cancelButton;
    var confirmButton = options.confirmButton;
    var onConfirm = options.onConfirm || function() {};
    var dialogButtons = [cancelButton, confirmButton];

    cancelButton.addEventListener("click", modal.close);
    confirmButton.addEventListener("click", onConfirm);

    function modalKey(event) {
      var key = (event.key || "").toLowerCase();
      if (key === "escape" || key === "esc" || key === "u+001b") {
        return "escape";
      }
      if (key === "enter" || key === "return" || key === "u+000d") {
        return "enter";
      }

      var eventCode = (event.code || "").toLowerCase();
      if (eventCode === "escape") {
        return "escape";
      }
      if (eventCode === "enter" || eventCode === "numpadenter") {
        return "enter";
      }

      var keyIdentifier = (event.keyIdentifier || "").toLowerCase();
      if (keyIdentifier === "u+001b") {
        return "escape";
      }
      if (keyIdentifier === "enter" || keyIdentifier === "u+000d") {
        return "enter";
      }

      var code = event.keyCode || event.which || event.charCode || 0;
      if (code === 13) {
        return "enter";
      }
      if (code === 27) {
        return "escape";
      }
      return "";
    }

    function focusDialogButton(reverse) {
      var activeIndex = dialogButtons.indexOf(document.activeElement);
      if (activeIndex < 0) {
        dialogButtons[reverse ? dialogButtons.length - 1 : 0].focus();
        return;
      }

      var step = reverse ? -1 : 1;
      dialogButtons[(activeIndex + dialogButtons.length + step) % dialogButtons.length].focus();
    }

    function handleKeyDown(event) {
      if (!modal.isOpen()) {
        return false;
      }

      if (keyName(event) === "tab") {
        event.preventDefault();
        focusDialogButton(event.shiftKey);
        return true;
      }

      var key = modalKey(event);
      if (key === "escape") {
        event.preventDefault();
        modal.close();
        return true;
      }

      if (key === "enter") {
        var active = document.activeElement;
        if (active !== cancelButton && active !== confirmButton) {
          return true;
        }

        event.preventDefault();
        if (active === confirmButton) {
          onConfirm();
        } else {
          modal.close();
        }
        return true;
      }

      if (moveDirectionalFocus(event, dialogButtons, {
        nextKey: "arrowright",
        previousKey: "arrowleft"
      })) {
        return true;
      }

      if (directionalFocusKey(event)) {
        return true;
      }

      return true;
    }

    return {
      close: modal.close,
      handleKeyDown: handleKeyDown,
      isOpen: modal.isOpen,
      open: modal.open
    };
  }

  document.addEventListener("keydown", function(event) {
    if (keyName(event) === "tab") {
      setKeyboardFocusVisible(true);
    }
  });

  document.addEventListener("mousedown", function() {
    setKeyboardFocusVisible(false);
  });

  document.addEventListener("pointerdown", function() {
    setKeyboardFocusVisible(false);
  });

  setKeyboardFocusVisible(false);

  window.solarisUi = {
    createConfirmDialog: createConfirmDialog,
    createModal: createModal,
    directionalFocusKey: directionalFocusKey,
    hasKeyboardFocusVisible: function() {
      return keyboardFocusVisible;
    },
    installButtonConfirmKeys: installButtonConfirmKeys,
    isConfirmKey: isConfirmKey,
    keyName: keyName,
    moveDirectionalFocus: moveDirectionalFocus
  };
})();
