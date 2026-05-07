const referenceHeight = 1600;

export const updateScreenHeightScale = () => {
  const scale = window.innerHeight / referenceHeight;
  const canvasWidth = Math.round(window.innerWidth / scale);
  document.documentElement.style.setProperty("--ui-scale", String(scale));
  document.documentElement.style.setProperty("--ui-canvas-width", canvasWidth + "px");
  document.documentElement.style.setProperty("--ui-canvas-height", referenceHeight + "px");
};

let keyboardFocusVisible = false;

const setKeyboardFocusVisible = (value) => {
  keyboardFocusVisible = value;
  document.documentElement.dataset.keyboardFocus = value ? "true" : "false";
};

export const hasKeyboardFocusVisible = () => keyboardFocusVisible;

export const sendNative = (command, payload = "") =>
  window.solarisNativeCommand(command, payload);

export const keyName = (event) => {
  const key = event.key.toLowerCase();
  if (key === " ") return "space";
  return key;
};

export const isConfirmKey = (event) => event.key === "Enter";

export const directionalFocusKey = (event) => {
  if (event.key === "ArrowLeft") return "arrowleft";
  if (event.key === "ArrowRight") return "arrowright";
  if (event.key === "ArrowUp") return "arrowup";
  if (event.key === "ArrowDown") return "arrowdown";
  return "";
};

export const moveDirectionalFocus = (event, items, options) => {
  const key = directionalFocusKey(event);
  if (key !== options.previousKey && key !== options.nextKey) return false;
  const activeIndex = items.indexOf(document.activeElement);
  if (activeIndex < 0 || !keyboardFocusVisible) return false;
  event.preventDefault();
  const step = key === options.nextKey ? 1 : -1;
  items[(activeIndex + items.length + step) % items.length].focus();
  return true;
};

export const installButtonConfirmKeys = ({ root = document, activate = (button) => button.click() } = {}) => {
  root.addEventListener("keydown", (event) => {
    if (event.defaultPrevented || !isConfirmKey(event)) return;
    const active = document.activeElement;
    if (!active || active.tagName !== "BUTTON" || active.disabled) return;
    event.preventDefault();
    activate(active);
  });
};

export const createModal = ({ modal, defaultFocus = null }) => {
  let returnFocus = null;
  const isOpen = () => modal.classList.contains("is-active");
  const open = () => {
    if (isOpen()) return;
    returnFocus = document.activeElement;
    modal.classList.add("is-active");
    if (defaultFocus) defaultFocus.focus();
    else if (returnFocus) returnFocus.blur();
  };
  const close = () => {
    if (!isOpen()) return;
    modal.classList.remove("is-active");
    if (returnFocus && document.contains(returnFocus)) returnFocus.focus();
    returnFocus = null;
  };
  return { close, isOpen, open };
};

export const createConfirmDialog = ({ modal: modalEl, cancelButton, confirmButton, onConfirm = () => {} }) => {
  const modal = createModal({ modal: modalEl });
  const dialogButtons = [cancelButton, confirmButton];

  cancelButton.addEventListener("click", modal.close);
  confirmButton.addEventListener("click", onConfirm);

  const focusDialogButton = (reverse) => {
    const activeIndex = dialogButtons.indexOf(document.activeElement);
    if (activeIndex < 0) {
      dialogButtons[reverse ? dialogButtons.length - 1 : 0].focus();
      return;
    }
    const step = reverse ? -1 : 1;
    dialogButtons[(activeIndex + dialogButtons.length + step) % dialogButtons.length].focus();
  };

  const handleKeyDown = (event) => {
    if (!modal.isOpen()) return false;

    if (event.key === "Tab") {
      event.preventDefault();
      focusDialogButton(event.shiftKey);
    } else if (event.key === "Escape") {
      event.preventDefault();
      modal.close();
    } else if (event.key === "Enter") {
      const active = document.activeElement;
      if (active === confirmButton) {
        event.preventDefault();
        onConfirm();
      } else if (active === cancelButton) {
        event.preventDefault();
        modal.close();
      }
    } else {
      moveDirectionalFocus(event, dialogButtons, { nextKey: "arrowright", previousKey: "arrowleft" });
    }
    return true;
  };

  return { close: modal.close, handleKeyDown, isOpen: modal.isOpen, open: modal.open };
};

document.addEventListener("keydown", (event) => {
  if (event.key === "Tab") setKeyboardFocusVisible(true);
});
document.addEventListener("mousedown", () => setKeyboardFocusVisible(false));
document.addEventListener("pointerdown", () => setKeyboardFocusVisible(false));

window.addEventListener("resize", updateScreenHeightScale);
updateScreenHeightScale();
setKeyboardFocusVisible(false);
