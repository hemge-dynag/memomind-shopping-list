#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_libc.h"

/* Paired with PhoneSDK/examples/shopping-list. The phone pushes the full list
 * on every change; this plugin renders the still-unchecked items as a 5-row
 * carousel with the selected item centered. Head movement navigates and the
 * primary button checks the centered item, which then drops out of the list
 * while the wearer's hands are busy with groceries. */

#define LIST_CHANNEL UINT16_C(0x534C)  /* 'SL': phone -> glasses */
#define EVENT_CHANNEL UINT16_C(0x5345) /* 'SE': glasses -> phone */
#define PROTOCOL_VERSION 1U

#define COMMAND_SET_LIST 1U
#define COMMAND_SESSION_END 2U

#define EVENT_CHECK UINT8_C(1)
#define EVENT_UNCHECK UINT8_C(2)

#define MAX_ITEMS 30U
#define MAX_ITEM_TEXT_BYTES 48U
/* version + command + count(2) */
#define MIN_SET_LIST_PAYLOAD 4U

#define VISIBLE_ROWS 5U
#define CENTER_SLOT 2U

#define SCREEN_MARGIN 20
#define HINT_HEIGHT 32
#define ROW_MIN_HEIGHT 32

typedef struct {
    char text[MAX_ITEM_TEXT_BYTES + 1U];
    bool checked;
} shopping_item_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const gm_plugin_libc_extension_api_t *libc;
    gm_plugin_lvgl_obj_t *screen;
    gm_plugin_lvgl_obj_t *selection_panel;
    gm_plugin_lvgl_obj_t *row_label[VISIBLE_ROWS];
    gm_plugin_lvgl_obj_t *hint_label;
    shopping_item_t items[MAX_ITEMS];
    uint16_t item_count;
    /* Indices (into items[]) of the still-unchecked entries, in order. Checked
     * items are excluded so they vanish from the carousel; selected_index is a
     * position inside this visible list, not into items[]. */
    uint16_t visible[MAX_ITEMS];
    uint16_t visible_count;
    uint16_t selected_index;
    int32_t row_height;
    uint8_t language; /* 0=fr 1=en 2=es 3=it 4=de 5=zh-CN */
} shopping_list_t;

/* Picks the string for the detected UI locale. */
#define L(fr_str, en_str, es_str, it_str, de_str, zh_str) \
    (sl.language == 1U ? (en_str) : sl.language == 2U ? (es_str) : \
     sl.language == 3U ? (it_str) : sl.language == 4U ? (de_str) : \
     sl.language == 5U ? (zh_str) : (fr_str))

static shopping_list_t sl;

static uint8_t shopping_list_detect_language(const char *locale)
{
    if (locale[0] == 'e' && locale[1] == 'n') return 1U;
    if (locale[0] == 'e' && locale[1] == 's') return 2U;
    if (locale[0] == 'i' && locale[1] == 't') return 3U;
    if (locale[0] == 'd' && locale[1] == 'e') return 4U;
    if (locale[0] == 'z' && locale[1] == 'h') return 5U;
    return 0U;
}

#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static void set_style(gm_plugin_lvgl_obj_t *object,
                       gm_plugin_lvgl_style_prop_t property,
                       gm_plugin_lvgl_style_value_t value)
{
    sl.ui->style_set(object, property, value, GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void style_panel(gm_plugin_lvgl_obj_t *object, uint8_t fill,
                         uint8_t border, uint8_t radius)
{
    set_style(object, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(fill));
    set_style(object, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(255));
    set_style(object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR, color(0xA0));
    set_style(object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA, number(255));
    set_style(object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, number(border));
    set_style(object, GM_PLUGIN_LVGL_STYLE_RADIUS, number(radius));
    sl.ui->obj_clear_flag(object, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
}

static gm_plugin_lvgl_obj_t *make_label(gm_plugin_lvgl_obj_t *parent,
                                         const char *text, uint8_t shade)
{
    gm_plugin_lvgl_obj_t *label = sl.ui->label_create(parent);
    if (label == 0) return 0;
    sl.ui->label_set_text(label, text);
    sl.ui->label_set_long_mode(label, GM_PLUGIN_LVGL_LABEL_WRAP);
    set_style(label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(shade));
    set_style(label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
    return label;
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

/* Wraps index into [0, count) using a true modulo (not C's remainder, which
 * can be negative) so navigation loops smoothly past both ends of the list. */
static uint16_t wrap_index(int32_t index, uint16_t count)
{
    int32_t wrapped = index % (int32_t)count;
    if (wrapped < 0) wrapped += (int32_t)count;
    return (uint16_t)wrapped;
}

/* Rebuilds the visible (unchecked) index list after the item set or a checked
 * flag changes, then keeps the selection inside the new bounds. */
static void rebuild_visible(void)
{
    uint16_t i;
    sl.visible_count = 0U;
    for (i = 0U; i < sl.item_count; i++) {
        if (!sl.items[i].checked)
            sl.visible[sl.visible_count++] = i;
    }
    if (sl.visible_count == 0U)
        sl.selected_index = 0U;
    else if (sl.selected_index >= sl.visible_count)
        sl.selected_index = (uint16_t)(sl.visible_count - 1U);
}

/* Distance from the centered/selected slot drives the font, the text opacity
 * and the rendered scale, so rows shrink and fade towards the top/bottom
 * edges like a gradient carousel instead of a flat dim/bright split. */
static uint8_t opacity_for_distance(uint32_t distance)
{
    switch (distance) {
    case 0: return GM_PLUGIN_LVGL_OPA_COVER;
    case 1: return GM_PLUGIN_LVGL_OPA_70;
    default: return GM_PLUGIN_LVGL_OPA_30;
    }
}

/* LVGL TRANSFORM_ZOOM is a 1/256 fixed-point factor (256 == 100%). The center
 * row renders at full size; each step away is scaled down so the off-center
 * rows are visibly smaller, mirroring the opacity falloff above. */
static int32_t zoom_for_distance(uint32_t distance)
{
    switch (distance) {
    case 0: return 256;
    case 1: return 205;
    default: return 154;
    }
}

static void refresh_row(uint32_t slot, uint16_t item_index)
{
    const shopping_item_t *item = &sl.items[item_index];
    bool is_center = (slot == CENTER_SLOT);
    uint32_t distance = is_center ? 0U
        : (slot > CENTER_SLOT ? slot - CENTER_SLOT : CENTER_SLOT - slot);
    gm_plugin_lvgl_style_value_t font_value = {0};

    sl.ui->label_set_text(sl.row_label[slot], item->text);

    font_value.ptr = is_center ? sl.ui->font_large : sl.ui->font_default;
    set_style(sl.row_label[slot], GM_PLUGIN_LVGL_STYLE_TEXT_FONT, font_value);
    set_style(sl.row_label[slot], GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0xF0));
    set_style(sl.row_label[slot], GM_PLUGIN_LVGL_STYLE_TEXT_OPA,
              number(opacity_for_distance(distance)));
    set_style(sl.row_label[slot], GM_PLUGIN_LVGL_STYLE_TRANSFORM_ZOOM,
              number(zoom_for_distance(distance)));
}

static void refresh_list_display(void)
{
    uint32_t slot;

    if (sl.item_count == 0U) {
        for (slot = 0U; slot < VISIBLE_ROWS; slot++)
            sl.ui->label_set_text(sl.row_label[slot], "");
        sl.ui->label_set_text(sl.hint_label,
            L("En attente du telephone...", "Waiting for the phone...", "Esperando al teléfono...", "In attesa del telefono...", "Warte auf das Telefon...", "正在等待手机..."));
        return;
    }

    if (sl.visible_count == 0U) {
        for (slot = 0U; slot < VISIBLE_ROWS; slot++)
            sl.ui->label_set_text(sl.row_label[slot], "");
        sl.ui->label_set_text(sl.hint_label,
            L("Liste terminee !", "All done!", "¡Lista completada!", "Lista completata!", "Liste fertig!", "清单完成！"));
        return;
    }

    for (slot = 0U; slot < VISIBLE_ROWS; slot++) {
        int32_t slot_offset = (int32_t)slot - (int32_t)CENTER_SLOT;
        uint16_t position = wrap_index((int32_t)sl.selected_index + slot_offset,
                                        sl.visible_count);
        refresh_row(slot, sl.visible[position]);
    }
    sl.ui->label_set_text(sl.hint_label,
        L("Tete haut/bas: deplacer   Bouton: cocher",
          "Head up/down: move   Button: check",
          "Cabeza arriba/abajo: mover   Botón: marcar",
          "Testa su/giù: muovi   Pulsante: spunta",
          "Kopf hoch/runter: bewegen   Taste: abhaken",
          "抬头/低头：移动   按钮：勾选"));
}

static void move_selection(int32_t delta)
{
    if (sl.visible_count == 0U) return;
    sl.selected_index = wrap_index((int32_t)sl.selected_index + delta,
                                   sl.visible_count);
    refresh_list_display();
}

/* Checks or unchecks the centered item, tells the phone (which identifies the
 * item by its index in the full list) and refreshes the carousel. The phone
 * keeps the full list; this side hides checked entries, so a checked item
 * disappears here but remains in the phone's data model. */
static void send_toggle_event(bool checked)
{
    uint8_t payload[4];
    uint16_t item_index;
    if (sl.visible_count == 0U) return;
    item_index = sl.visible[sl.selected_index];
    sl.items[item_index].checked = checked;
    payload[0] = PROTOCOL_VERSION;
    payload[1] = checked ? EVENT_CHECK : EVENT_UNCHECK;
    payload[2] = (uint8_t)(item_index & 0xFFU);
    payload[3] = (uint8_t)((item_index >> 8) & 0xFFU);
    (void)sl.host->bt_send(EVENT_CHANNEL, payload, sizeof(payload));
    rebuild_visible();
    refresh_list_display();
}

/* The primary button checks the centered item, which then leaves the list. */
static void toggle_selection(void)
{
    if (sl.visible_count == 0U) return;
    if (!sl.items[sl.visible[sl.selected_index]].checked)
        send_toggle_event(true);
}

static gm_plugin_result_t shopping_list_start(void *context)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_obj_t *root;
    char locale[GM_PLUGIN_LOCALE_TAG_MAX];
    int32_t list_area_height;
    uint32_t slot;
    (void)context;

    sl.language = 0U;
    if (sl.host->locale_get != 0 &&
        sl.host->locale_get(locale) == GM_PLUGIN_OK)
        sl.language = shopping_list_detect_language(locale);

    if (sl.host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width <= SCREEN_MARGIN * 2U)
        return GM_PLUGIN_ESTATE;

    list_area_height = (int32_t)display.height - HINT_HEIGHT - SCREEN_MARGIN;
    sl.row_height = list_area_height / (int32_t)VISIBLE_ROWS;
    if (sl.row_height < ROW_MIN_HEIGHT)
        return GM_PLUGIN_ESTATE;

    root = sl.ui->root_get();
    if (root == 0) return GM_PLUGIN_ESTATE;
    sl.ui->obj_clean(root);

    sl.screen = sl.ui->obj_create(root);
    if (sl.screen == 0) return GM_PLUGIN_ENOMEM;
    sl.ui->obj_set_size(sl.screen, display.width, display.height);
    sl.ui->obj_align(sl.screen, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_panel(sl.screen, 0x00, 0, 0);

    sl.selection_panel = sl.ui->obj_create(sl.screen);
    if (sl.selection_panel == 0) goto no_memory;
    sl.ui->obj_set_size(sl.selection_panel, display.width - SCREEN_MARGIN,
                         sl.row_height);
    sl.ui->obj_align(sl.selection_panel, GM_PLUGIN_LVGL_ALIGN_CENTER, 0,
                      -(HINT_HEIGHT / 2));
    style_panel(sl.selection_panel, 0x10, 2, 10);

    for (slot = 0U; slot < VISIBLE_ROWS; slot++) {
        int32_t offset_y = ((int32_t)slot - (int32_t)CENTER_SLOT) * sl.row_height
                            - (HINT_HEIGHT / 2);
        sl.row_label[slot] = make_label(sl.screen, "", 0x80);
        if (sl.row_label[slot] == 0) goto no_memory;
        sl.ui->label_set_long_mode(sl.row_label[slot], GM_PLUGIN_LVGL_LABEL_DOT);
        set_style(sl.row_label[slot], GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                  number(GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT));
        sl.ui->obj_set_size(sl.row_label[slot], display.width - SCREEN_MARGIN * 2U,
                             sl.row_height - 4);
        sl.ui->obj_align(sl.row_label[slot], GM_PLUGIN_LVGL_ALIGN_CENTER, 0, offset_y);
    }

    sl.hint_label = make_label(sl.screen, "", 0x70);
    if (sl.hint_label == 0) goto no_memory;
    sl.ui->obj_set_size(sl.hint_label, display.width - SCREEN_MARGIN * 2U, HINT_HEIGHT);
    sl.ui->obj_align(sl.hint_label, GM_PLUGIN_LVGL_ALIGN_BOTTOM_MID, 0, -8);

    sl.item_count = 0U;
    sl.visible_count = 0U;
    sl.selected_index = 0U;
    refresh_list_display();

    if (sl.host->imu_enable(GM_PLUGIN_IMU_ENABLE_GESTURES) != GM_PLUGIN_OK)
        goto no_memory;
    return GM_PLUGIN_OK;

no_memory:
    sl.ui->obj_clean(root);
    sl.screen = 0;
    sl.selection_panel = 0;
    for (slot = 0U; slot < VISIBLE_ROWS; slot++) sl.row_label[slot] = 0;
    sl.hint_label = 0;
    return GM_PLUGIN_ENOMEM;
}

static void handle_list_message(const uint8_t *data, uint32_t length)
{
    uint32_t offset;
    uint16_t requested_count;
    uint16_t i;

    if (length < MIN_SET_LIST_PAYLOAD || data[0] != PROTOCOL_VERSION) return;

    if (data[1] == COMMAND_SESSION_END) {
        sl.item_count = 0U;
        sl.visible_count = 0U;
        sl.selected_index = 0U;
        refresh_list_display();
        return;
    }
    if (data[1] != COMMAND_SET_LIST) return;

    requested_count = read_u16(&data[2]);
    if (requested_count > MAX_ITEMS) requested_count = MAX_ITEMS;

    offset = 4U;
    for (i = 0U; i < requested_count; i++) {
        uint8_t checked;
        uint8_t text_len;
        if (offset + 2U > length) break;
        checked = data[offset];
        text_len = data[offset + 1U];
        offset += 2U;
        if (text_len > MAX_ITEM_TEXT_BYTES || offset + text_len > length) break;
        sl.libc->memcpy(sl.items[i].text, &data[offset], text_len);
        sl.items[i].text[text_len] = '\0';
        sl.items[i].checked = checked != 0U;
        offset += text_len;
    }
    sl.item_count = i;
    rebuild_visible();
    refresh_list_display();
}

static bool shopping_list_event(void *context, const gm_plugin_event_t *event)
{
    (void)context;
    if (event == 0) return false;

    if (event->type == GM_PLUGIN_EVENT_CONNECTION) {
        sl.host->log("shopping-list: phone connected=%u",
                      (unsigned int)event->data.connection.connected);
        return true;
    }

    if (event->type == GM_PLUGIN_EVENT_BT_MESSAGE) {
        if (event->data.bt.channel != LIST_CHANNEL || event->data.bt.data == 0)
            return false;
        handle_list_message(event->data.bt.data, event->data.bt.length);
        return true;
    }

    if (sl.item_count == 0U) return false;

    if (event->type == GM_PLUGIN_EVENT_BUTTON &&
        event->data.button.button == GM_PLUGIN_BUTTON_PRIMARY &&
        event->data.button.action == GM_PLUGIN_BUTTON_ACTION_SINGLE) {
        toggle_selection();
        return true;
    }

    if (event->type == GM_PLUGIN_EVENT_IMU_GESTURE &&
        event->data.imu_gesture.active) {
        switch (event->data.imu_gesture.gesture) {
        case GM_PLUGIN_IMU_GESTURE_HEAD_RAISE:
            move_selection(-1);
            return true;
        case GM_PLUGIN_IMU_GESTURE_HEAD_LOWER:
            move_selection(1);
            return true;
        default:
            break;
        }
    }
    return false;
}

static void shopping_list_stop(void *context)
{
    gm_plugin_lvgl_obj_t *root;
    uint32_t slot;
    (void)context;
    root = sl.ui->root_get();
    (void)sl.host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
    if (root != 0) sl.ui->obj_clean(root);
    sl.screen = 0;
    sl.selection_panel = 0;
    for (slot = 0U; slot < VISIBLE_ROWS; slot++) sl.row_label[slot] = 0;
    sl.hint_label = 0;
    sl.item_count = 0U;
    sl.visible_count = 0U;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                    gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required =
        GM_PLUGIN_CAP_BLUETOOTH | GM_PLUGIN_CAP_BUTTON | GM_PLUGIN_CAP_IMU_EVENTS;

    if (host == 0 || plugin == 0 || host->log == 0 || host->bt_send == 0 ||
        host->display_get_info == 0 || host->graphics.lvgl == 0 ||
        host->imu_enable == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & required) != required)
        return GM_PLUGIN_ENOTSUP;

    sl.host = host;
    sl.ui = host->graphics.lvgl;
    if (gm_plugin_libc_get(host, &sl.libc) != GM_PLUGIN_OK)
        return GM_PLUGIN_ENOTSUP;
    if (sl.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(sl.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;

    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &sl;
    plugin->on_start = shopping_list_start;
    plugin->on_event = shopping_list_event;
    plugin->on_stop = shopping_list_stop;
    return GM_PLUGIN_OK;
}
