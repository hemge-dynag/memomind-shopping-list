// Binary wire protocol shared with GlassSDK/examples/shopping-list/shopping-list.c.
// Keep both sides in sync if you change any layout or constant here.

export const LIST_CHANNEL = 0x534c; // 'SL', phone -> glasses
export const EVENT_CHANNEL = 0x5345; // 'SE', glasses -> phone
export const PROTOCOL_VERSION = 1;

export const COMMAND_SET_LIST = 1;
export const COMMAND_SESSION_END = 2;

export const EVENT_CHECK = 1;
export const EVENT_UNCHECK = 2;

// The glasses render a fixed 5-row carousel and hold the full list in a
// static buffer, so both ends agree on these bounds.
export const MAX_ITEMS = 30;
export const MAX_ITEM_TEXT_BYTES = 48;

const encoder = new TextEncoder();
const decoder = new TextDecoder();

// Truncates to at most maxBytes UTF-8 bytes without splitting a multi-byte
// code point, since the glass side copies the raw bytes into a fixed buffer.
function toBoundedUtf8(text, maxBytes) {
  let bytes = encoder.encode(text);
  while (bytes.length > maxBytes) {
    bytes = encoder.encode([...decoder.decode(bytes)].slice(0, -1).join(''));
  }
  return bytes;
}

// [version, COMMAND_SET_LIST, count_lo, count_hi,
//  (checked, text_len, text_bytes...) per item]
export function encodeSetList(items) {
  const limited = items.slice(0, MAX_ITEMS);
  const texts = limited.map((item) => toBoundedUtf8(item.text, MAX_ITEM_TEXT_BYTES));
  const textBytesTotal = texts.reduce((sum, bytes) => sum + bytes.length, 0);
  const payload = new Uint8Array(4 + limited.length * 2 + textBytesTotal);
  let offset = 0;
  payload[offset++] = PROTOCOL_VERSION;
  payload[offset++] = COMMAND_SET_LIST;
  payload[offset++] = limited.length & 0xff;
  payload[offset++] = (limited.length >> 8) & 0xff;
  limited.forEach((item, i) => {
    const textBytes = texts[i];
    payload[offset++] = item.checked ? 1 : 0;
    payload[offset++] = textBytes.length;
    payload.set(textBytes, offset);
    offset += textBytes.length;
  });
  return payload;
}

// Padded to 4 bytes to match the glass side's minimum header length.
export function encodeSessionEnd() {
  return new Uint8Array([PROTOCOL_VERSION, COMMAND_SESSION_END, 0, 0]);
}

// [version, event, index_lo, index_hi]
export function decodeEvent(data) {
  if (!data || data.length < 4 || data[0] !== PROTOCOL_VERSION) return null;
  const event = data[1];
  if (event !== EVENT_CHECK && event !== EVENT_UNCHECK) return null;
  const index = data[2] | (data[3] << 8);
  return { event, index, checked: event === EVENT_CHECK };
}
