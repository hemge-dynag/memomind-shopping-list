import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
import {
  LIST_CHANNEL,
  EVENT_CHANNEL,
  EVENT_CHECK,
  decodeEvent,
  encodeSessionEnd,
  encodeSetList,
} from './protocol.js';

const STORAGE_KEY = 'items';

const gm = createGMPlugin();

const statusEl = document.querySelector('#status');
const itemListEl = document.querySelector('#item-list');
const itemCountEl = document.querySelector('#item-count');
const startButton = document.querySelector('#start-shopping');
const addForm = document.querySelector('#add-item-form');
const itemInput = document.querySelector('#item-input');
const setupSection = document.querySelector('#setup-section');
const shoppingSection = document.querySelector('#shopping-section');
const stopButton = document.querySelector('#stop-shopping');

let items = [];
let connected = false;
let sessionActive = false;

function makeItemId() {
  return `i-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 8)}`;
}

function setStatus(text, state = '') {
  statusEl.textContent = text;
  statusEl.className = `status ${state}`.trim();
}

async function loadItems() {
  const result = await gm.storage.get(STORAGE_KEY);
  if (result.value) {
    items = result.value;
    return;
  }
  items = [
    { id: makeItemId(), text: 'Lait', checked: false },
    { id: makeItemId(), text: 'Pain', checked: false },
  ];
  await persistItems();
}

async function persistItems() {
  await gm.storage.set(STORAGE_KEY, items);
}

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

function renderList() {
  itemListEl.innerHTML = '';
  // During an active shopping session, checked items drop out of the list
  // (matching the glasses). Out of session they stay visible so they can be
  // unchecked, edited or deleted.
  const listedItems = sessionActive ? items.filter((item) => !item.checked) : items;
  for (const item of listedItems) {
    const row = document.createElement('li');
    row.className = 'item-row';
    row.innerHTML = `
      <label class="item-row-text">
        <input type="checkbox" class="item-checkbox" data-id="${item.id}" ${item.checked ? 'checked' : ''}>
        <span class="item-text${item.checked ? ' checked' : ''}">${escapeHtml(item.text)}</span>
      </label>
      <button class="delete-item" data-id="${item.id}" aria-label="Supprimer">✕</button>
    `;
    itemListEl.appendChild(row);
  }
  itemCountEl.textContent = String(items.filter((item) => !item.checked).length);
  startButton.disabled = items.length === 0 || !connected || sessionActive;
  setupSection.hidden = sessionActive;
  addForm.hidden = sessionActive;
  shoppingSection.hidden = !sessionActive;
}

async function syncList() {
  try {
    await gm.plugin.sendMessage(LIST_CHANNEL, encodeSetList(items));
  } catch (error) {
    setStatus(error.message || String(error), 'error');
  }
}

async function onItemsChanged() {
  await persistItems();
  renderList();
  if (sessionActive) await syncList();
}

async function stopShopping() {
  sessionActive = false;
  try {
    await gm.plugin.sendMessage(LIST_CHANNEL, encodeSessionEnd());
  } catch (error) {
    setStatus(error.message || String(error), 'error');
  }
  renderList();
}

itemListEl.addEventListener('click', async (event) => {
  const deleteButton = event.target.closest('.delete-item');
  if (deleteButton) {
    items = items.filter((item) => item.id !== deleteButton.dataset.id);
    await onItemsChanged();
  }
});

itemListEl.addEventListener('change', async (event) => {
  const checkbox = event.target.closest('.item-checkbox');
  if (!checkbox) return;
  const item = items.find((entry) => entry.id === checkbox.dataset.id);
  if (!item) return;
  item.checked = checkbox.checked;
  await onItemsChanged();
  if (sessionActive && items.length > 0 && items.every((entry) => entry.checked)) {
    await stopShopping();
  }
});

addForm.addEventListener('submit', async (event) => {
  event.preventDefault();
  const text = itemInput.value.trim();
  if (!text) return;
  items.push({ id: makeItemId(), text, checked: false });
  addForm.reset();
  await onItemsChanged();
  itemInput.focus();
});

startButton.addEventListener('click', async () => {
  if (items.length === 0) return;
  sessionActive = true;
  renderList();
  await syncList();
});

stopButton.addEventListener('click', () => void stopShopping());

const offListMessages = gm.plugin.onMessage((message) => {
  if (message.channel !== EVENT_CHANNEL) return;
  const decoded = decodeEvent(message.data);
  if (!decoded || !sessionActive) return;
  const item = items[decoded.index];
  if (!item) return;
  item.checked = decoded.event === EVENT_CHECK;
  void (async () => {
    await persistItems();
    renderList();
    if (items.length > 0 && items.every((entry) => entry.checked)) {
      await stopShopping();
    }
  })();
});

async function start() {
  try {
    await gm.ready();
    await loadItems();
    const info = await gm.device.getInfo();
    connected = Boolean(info.connected);
    setStatus(connected ? 'Lunettes connectées' : 'Lunettes déconnectées', connected ? 'ready' : 'error');
    renderList();
    await gm.device.subscribeEvents(['connection']);
    gm.device.onConnection((event) => {
      connected = Boolean(event.connected);
      setStatus(connected ? 'Lunettes connectées' : 'Lunettes déconnectées', connected ? 'ready' : 'error');
      if (!connected && sessionActive) void stopShopping();
      renderList();
    });
  } catch (error) {
    setStatus(error.message || String(error), 'error');
  }
}

window.addEventListener('pagehide', () => {
  offListMessages();
  gm.close();
});

void start();
