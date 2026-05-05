(function() {
  function activeHtmlElement() {
    return document.activeElement instanceof HTMLElement
      ? document.activeElement
      : null;
  }

  function containsElement(root, element) {
    return !!root && !!element && root.contains(element);
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
      return key;
    }

    var keyIdentifier = (event.keyIdentifier || "").toLowerCase();
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
    return "";
  }

  function isConfirmKey(event) {
    var key = keyName(event);
    return key === "enter";
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
    var defaultFocus = options.defaultFocus || modal;
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
      defaultFocus.focus();
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
      modal: options.modal,
      defaultFocus: options.cancelButton
    });
    var cancelButton = options.cancelButton;
    var confirmButton = options.confirmButton;
    var onConfirm = options.onConfirm || function() {};

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
      if (key === "arrowleft" || key === "left") {
        return "arrowleft";
      }
      if (key === "arrowright" || key === "right") {
        return "arrowright";
      }

      var eventCode = (event.code || "").toLowerCase();
      if (eventCode === "escape") {
        return "escape";
      }
      if (eventCode === "enter" || eventCode === "numpadenter") {
        return "enter";
      }
      if (eventCode === "arrowleft") {
        return "arrowleft";
      }
      if (eventCode === "arrowright") {
        return "arrowright";
      }

      var keyIdentifier = (event.keyIdentifier || "").toLowerCase();
      if (keyIdentifier === "u+001b") {
        return "escape";
      }
      if (keyIdentifier === "enter" || keyIdentifier === "u+000d") {
        return "enter";
      }

      if (keyIdentifier === "left" || keyIdentifier === "u+0025") {
        return "arrowleft";
      }
      if (keyIdentifier === "right" || keyIdentifier === "u+0027") {
        return "arrowright";
      }

      var code = event.keyCode || event.which || event.charCode || 0;
      if (code === 13) {
        return "enter";
      }
      if (code === 27) {
        return "escape";
      }
      if (code === 37) {
        return "arrowleft";
      }
      if (code === 39) {
        return "arrowright";
      }
      return "";
    }

    function handleKeyDown(event) {
      if (!modal.isOpen()) {
        return false;
      }

      var key = modalKey(event);
      if (key === "escape") {
        event.preventDefault();
        modal.close();
        return true;
      }

      if (key === "enter") {
        event.preventDefault();
        if (activeHtmlElement() === confirmButton) {
          onConfirm();
        } else {
          modal.close();
        }
        return true;
      }

      if (key === "arrowleft" || key === "arrowright") {
        event.preventDefault();
        if (activeHtmlElement() === confirmButton) {
          cancelButton.focus();
        } else {
          confirmButton.focus();
        }
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

  window.solarisUi = {
    createConfirmDialog: createConfirmDialog,
    createModal: createModal,
    installButtonConfirmKeys: installButtonConfirmKeys,
    isConfirmKey: isConfirmKey,
    keyName: keyName
  };
})();
