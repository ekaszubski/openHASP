/* MIT License - Copyright (c) 2019-2021 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

/* ********************************************************************************************
 *
 *  HASP Event Handlers
 *     - Value Senders       : Convert values and events into topic/payload before forwarding
 *     - Event Handlers      : Callbacks for object event processing
 *
 ******************************************************************************************** */

#include "hasp_conf.h"
#include "hasplib.h"

static lv_style_int_t last_value_sent;
static lv_color_t last_color_sent;

/* ============================== Event Senders ============================ */

/* Takes and lv_obj and finds the pageid and objid
   Then sends the data out on the state/pxby topic
*/

// ##################### Value Senders ########################################################

void event_obj_data(lv_obj_t* obj, const char* data)
{
    uint8_t pageid;
    uint8_t objid;

    if(hasp_find_id_from_obj(obj, &pageid, &objid)) {
        if(!data) return;
        object_dispatch_state(pageid, objid, data);
    } else {
        LOG_ERROR(TAG_MSGR, F(D_OBJECT_UNKNOWN));
    }
}

void event_dispatch(lv_obj_t* obj, uint8_t eventid, const char* data)
{}

// Send out the event that occured
void event_object_generic_event(lv_obj_t* obj, uint8_t eventid)
{
    char data[40];
    char eventname[8];

    Parser::get_event_name(eventid, eventname, sizeof(eventname));
    snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\"}"), eventname);
    event_obj_data(obj, data);
}

// Send out the on/off event, with the val
void event_object_val_event(lv_obj_t* obj, uint8_t eventid, int16_t val)
{
    char data[40];
    char eventname[8];

    Parser::get_event_name(eventid, eventname, sizeof(eventname));
    snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"val\":%d}"), eventname, val);
    event_obj_data(obj, data);
}

// Send out the changed event, with the val and text
void event_object_selection_changed(lv_obj_t* obj, int16_t val, const char* text)
{
    char data[200];

    snprintf_P(data, sizeof(data), PSTR("{\"event\":\"changed\",\"val\":%d,\"text\":\"%s\"}"), val, text);
    event_obj_data(obj, data);
}

// Send out the changed event, with the color
void event_object_color_event(lv_obj_t* obj, uint8_t eventid, lv_color_t color)
{
    char data[80];
    char eventname[8];
    lv_color32_t c32;
    c32.full = lv_color_to32(color);

    Parser::get_event_name(eventid, eventname, sizeof(eventname));
    snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"color\":\"#%02x%02x%02x\",\"r\":%d,\"g\":%d,\"b\":%d}"),
               eventname, c32.ch.red, c32.ch.green, c32.ch.blue, c32.ch.red, c32.ch.green, c32.ch.blue);
    event_obj_data(obj, data);
}

// ##################### Event Handlers ########################################################

#if HASP_USE_GPIO > 0
void event_gpio_input(uint8_t pin, uint8_t group, uint8_t eventid)
{
    char payload[64];
    char topic[8];
    char eventname[8];
    Parser::get_event_name(eventid, eventname, sizeof(eventname));
    snprintf_P(payload, sizeof(payload), PSTR("{\"pin\":%d,\"group\":%d,\"event\":\"%s\"}"), pin, group, eventname);

    memcpy_P(topic, PSTR("input"), 6);
    dispatch_state_subtopic(topic, payload);

    // update outputstates
    // dispatch_group_onoff(group, Parser::get_event_state(eventid), NULL);
}
#endif

void event_delete_object(lv_obj_t* obj)
{
    switch(obj->user_data.objid) {
        case LV_HASP_LINE:
            line_clear_points(obj);
            break;

        case LV_HASP_BTNMATRIX:
            my_btnmatrix_map_clear(obj);
            _LV_WIN_PART_REAL_LAST;
            _LV_WIN_PART_VIRTUAL_LAST;
            break;

        case LV_HASP_GAUGE:
            break;
    }

    // TODO: delete value_str data for ALL parts
    my_obj_set_value_str_txt(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, NULL);
}

void log_event(const char* name, lv_event_t event)
{
    return;

    switch(event) {
        case LV_EVENT_PRESSED:
            LOG_TRACE(TAG_EVENT, "%s Pressed", name);
            break;

        case LV_EVENT_PRESS_LOST:
            LOG_TRACE(TAG_EVENT, "%s Press lost", name);
            break;

        case LV_EVENT_SHORT_CLICKED:
            LOG_TRACE(TAG_EVENT, "%s Short clicked", name);
            break;

        case LV_EVENT_CLICKED:
            LOG_TRACE(TAG_EVENT, "%s Clicked", name);
            break;

        case LV_EVENT_LONG_PRESSED:
            LOG_TRACE(TAG_EVENT, "%S Long press", name);
            break;

        case LV_EVENT_LONG_PRESSED_REPEAT:
            LOG_TRACE(TAG_EVENT, "%s Long press repeat", name);
            break;

        case LV_EVENT_RELEASED:
            LOG_TRACE(TAG_EVENT, "%s Released", name);
            break;

        case LV_EVENT_VALUE_CHANGED:
            LOG_TRACE(TAG_EVENT, "%s Changed", name);
            break;

        case LV_EVENT_GESTURE:
            LOG_TRACE(TAG_EVENT, "%s Gesture", name);
            break;

        case LV_EVENT_FOCUSED:
            LOG_TRACE(TAG_EVENT, "%s Focussed", name);
            break;

        case LV_EVENT_DEFOCUSED:
            LOG_TRACE(TAG_EVENT, "%s Defocussed", name);
            break;

        case LV_EVENT_PRESSING:
            break;

        default:
            LOG_TRACE(TAG_EVENT, "%s Other %d", name, event);
    }
}

/**
 * Called when a press on the system layer is detected
 * @param obj pointer to a button matrix
 * @param event type of event that occured
 */
void wakeup_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("wakeup", event);

    if(event == LV_EVENT_RELEASED && obj == lv_disp_get_layer_sys(NULL)) {
        hasp_update_sleep_state(); // wakeup?
        if(!haspDevice.get_backlight_power())
            dispatch_backlight(NULL, "1"); // backlight on and also disable wakeup touch
        else {
            hasp_disable_wakeup_touch(); // only disable wakeup touch
        }
    }
}

void page_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("page", event);

    if(event == LV_EVENT_GESTURE) {
        lv_gesture_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
            case LV_GESTURE_DIR_LEFT:
                haspPages.next(LV_SCR_LOAD_ANIM_NONE);
                break;
            case LV_GESTURE_DIR_RIGHT:
                haspPages.prev(LV_SCR_LOAD_ANIM_NONE);
                break;
            case LV_GESTURE_DIR_BOTTOM:
                haspPages.back(LV_SCR_LOAD_ANIM_NONE);
                break;
        }
    }
}

/**
 * Called when a button-style object is clicked
 * @param obj pointer to a button object
 * @param event type of event that occured
 */
void generic_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("generic", event);

    switch(event) {
        case LV_EVENT_PRESSED:
            hasp_update_sleep_state(); // wakeup?
            last_value_sent = HASP_EVENT_DOWN;
            break;

        case LV_EVENT_CLICKED:
            // UP = the same object was release then was pressed and press was not lost!
            // eventid = HASP_EVENT_UP;
            if(last_value_sent != HASP_EVENT_LOST && last_value_sent != HASP_EVENT_UP)
                last_value_sent = HASP_EVENT_LONG;
            return;

        case LV_EVENT_SHORT_CLICKED:
            if(last_value_sent != HASP_EVENT_LOST) last_value_sent = HASP_EVENT_UP; // Avoid SHORT + UP double events
            break;

        case LV_EVENT_LONG_PRESSED:
            if(last_value_sent != HASP_EVENT_LOST) last_value_sent = HASP_EVENT_LONG;
            break;

        case LV_EVENT_LONG_PRESSED_REPEAT:
            if(last_value_sent != HASP_EVENT_LOST) last_value_sent = HASP_EVENT_HOLD;
            break; // we do care about hold

        case LV_EVENT_PRESS_LOST:
            last_value_sent = HASP_EVENT_LOST;
            break;

        case LV_EVENT_PRESSING:
        case LV_EVENT_FOCUSED:
        case LV_EVENT_DEFOCUSED:
        case LV_EVENT_GESTURE:
            return; // Don't care about these

        case LV_EVENT_RELEASED:
            if(last_value_sent == HASP_EVENT_UP) return;
            last_value_sent = HASP_EVENT_RELEASE;
            break;

        case LV_EVENT_DELETE:
            LOG_VERBOSE(TAG_EVENT, F(D_OBJECT_DELETED));
            event_delete_object(obj); // free and destroy persistent memory allocated for certain objects
            return;

        case LV_EVENT_VALUE_CHANGED: // Should not occur in this event handler
        default:
            LOG_WARNING(TAG_EVENT, F(D_OBJECT_EVENT_UNKNOWN), event);
            return;
    }

    if(last_value_sent == HASP_EVENT_LOST) return;

    /* If an actionid is attached, perform that action on UP event only */
    if(obj->user_data.actionid) {
        if(last_value_sent == HASP_EVENT_UP || last_value_sent == HASP_EVENT_RELEASE) {
            lv_scr_load_anim_t transitionid = (lv_scr_load_anim_t)obj->user_data.transitionid;
            switch(obj->user_data.actionid) {
                case HASP_NUM_PAGE_PREV:
                    haspPages.prev(transitionid);
                    break;
                case HASP_NUM_PAGE_BACK:
                    haspPages.back(transitionid);
                    break;
                case HASP_NUM_PAGE_NEXT:
                    haspPages.next(transitionid);
                    break;
                default:
                    haspPages.set(obj->user_data.actionid, transitionid);
            }
            dispatch_output_current_page();
        }
    } else {
        event_object_generic_event(obj, last_value_sent); // send normal object event
    }
    dispatch_normalized_group_value(obj->user_data.groupid, obj, Parser::get_event_state(last_value_sent),
                                    HASP_EVENT_OFF, HASP_EVENT_ON);
}

/**
 * Called when a object state is toggled on/off
 * @param obj pointer to a switch object
 * @param event type of event that occured
 */
void toggle_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("toggle", event);

    switch(event) {
        case LV_EVENT_PRESSED:
            hasp_update_sleep_state(); // wakeup?
        case LV_EVENT_RELEASED:

            switch(obj->user_data.objid) {
                case LV_HASP_SWITCH:
                    last_value_sent = lv_switch_get_state(obj);
                    break;

                case LV_HASP_CHECKBOX:
                    last_value_sent = lv_checkbox_is_checked(obj);
                    break;

                case LV_HASP_BUTTON: {
                    last_value_sent = lv_obj_get_state(obj, LV_BTN_PART_MAIN) & LV_STATE_CHECKED;
                    break;
                }

                default:
                    return;
            }

            event_object_val_event(obj, event == LV_EVENT_PRESSED ? HASP_EVENT_DOWN : HASP_EVENT_UP, last_value_sent);
            dispatch_normalized_group_value(obj->user_data.groupid, obj, last_value_sent, HASP_EVENT_OFF,
                                            HASP_EVENT_ON);
            break;

        case LV_EVENT_DELETE:
            LOG_VERBOSE(TAG_EVENT, F(D_OBJECT_DELETED));
            event_delete_object(obj);
            break;
    }
}

/**
 * Called when a range value has changed
 * @param obj pointer to a dropdown list or roller
 * @param event type of event that occured
 */
void selector_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("selector", event);

    if(event == LV_EVENT_VALUE_CHANGED) {
        char buffer[128];
        char property[36];
        uint16_t val = 0;
        uint16_t max = 0;
        hasp_update_sleep_state(); // wakeup?

        switch(obj->user_data.objid) {
            case LV_HASP_DROPDOWN:
                val = lv_dropdown_get_selected(obj);
                max = lv_dropdown_get_option_cnt(obj) - 1;
                lv_dropdown_get_selected_str(obj, buffer, sizeof(buffer));
                break;

            case LV_HASP_ROLLER:
                val = lv_roller_get_selected(obj);
                max = lv_roller_get_option_cnt(obj) - 1;
                lv_roller_get_selected_str(obj, buffer, sizeof(buffer));
                break;

            case LV_HASP_BTNMATRIX: {
                val             = lv_btnmatrix_get_active_btn(obj);
                const char* txt = lv_btnmatrix_get_btn_text(obj, val);
                strncpy(buffer, txt, sizeof(buffer));
                break;
            }

            case LV_HASP_TABLE: {
                uint16_t row;
                uint16_t col;
                if(lv_table_get_pressed_cell(obj, &row, &col) != LV_RES_OK) return; // outside any cell

                const char* txt = lv_table_get_cell_value(obj, row, col);
                strncpy(buffer, txt, sizeof(buffer));

                snprintf_P(property, sizeof(property), PSTR("row\":%d,\"col\":%d,\"text"), row, col);
                attr_out_str(obj, property, buffer);
                return;
            }

            default:
                return;
        }

        // set the property
        // snprintf_P(property, sizeof(property), PSTR("val\":%d,\"text"), val);
        // attr_out_str(obj, property, buffer);

        event_object_selection_changed(obj, val, buffer);
        if(max > 0) dispatch_normalized_group_value(obj->user_data.groupid, obj, val, 0, max);

    } else if(event == LV_EVENT_DELETE) {
        LOG_VERBOSE(TAG_EVENT, F(D_OBJECT_DELETED));
        event_delete_object(obj);
    }
}

/**
 * Called when a slider or adjustable arc is clicked
 * @param obj pointer to a slider
 * @param event type of event that occured
 */
void slider_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("slider", event);

    uint16_t evt;
    switch(event) {

        case LV_EVENT_PRESSED:
            hasp_update_sleep_state(); // wakeup on press down?
            evt = HASP_EVENT_DOWN;
        case LV_EVENT_LONG_PRESSED_REPEAT:
            if(event == LV_EVENT_LONG_PRESSED_REPEAT) evt = HASP_EVENT_CHANGED;
        case LV_EVENT_RELEASED: {
            if(event == LV_EVENT_RELEASED) evt = HASP_EVENT_UP;

            // case LV_EVENT_PRESSED || LV_EVENT_LONG_PRESSED_REPEAT || LV_EVENT_RELEASED
            int16_t val;
            int16_t min;
            int16_t max;

            if(obj->user_data.objid == LV_HASP_SLIDER) {
                val = lv_slider_get_value(obj);
                min = lv_slider_get_min_value(obj);
                max = lv_slider_get_max_value(obj);
            } else if(obj->user_data.objid == LV_HASP_ARC) {
                val = lv_arc_get_value(obj);
                min = lv_arc_get_min_value(obj);
                max = lv_arc_get_max_value(obj);
            } else {
                return;
            }

            if((event == LV_EVENT_LONG_PRESSED_REPEAT && last_value_sent != val) ||
               event != LV_EVENT_LONG_PRESSED_REPEAT) {
                last_value_sent = val;
                event_object_val_event(obj, evt, val);
                dispatch_normalized_group_value(obj->user_data.groupid, obj, val, min, max);
            }
            break;
        }

        case LV_EVENT_VALUE_CHANGED:
            break;

        case LV_EVENT_DELETE:
            LOG_VERBOSE(TAG_EVENT, F(D_OBJECT_DELETED));
            event_delete_object(obj);
            break;

        default:
            // LOG_VERBOSE(TAG_EVENT, F("Event ID: %d"), event);
            ;
    }
}

/**
 * Called when a color picker is clicked
 * @param obj pointer to a color picker
 * @param event type of event that occured
 */
void cpicker_event_handler(lv_obj_t* obj, lv_event_t event)
{
    char color[6];
    snprintf_P(color, sizeof(color), PSTR("color"));
    log_event("cpicker", event);

    uint16_t evt;
    switch(event) {

        case LV_EVENT_PRESSED:
            hasp_update_sleep_state(); // wakeup on press down?
            evt = HASP_EVENT_DOWN;
        case LV_EVENT_LONG_PRESSED_REPEAT:
            if(event == LV_EVENT_LONG_PRESSED_REPEAT) evt = HASP_EVENT_CHANGED;
        case LV_EVENT_RELEASED: {
            if(event == LV_EVENT_RELEASED) evt = HASP_EVENT_UP;
            lv_color_t color = lv_cpicker_get_color(obj);

            if((event == LV_EVENT_LONG_PRESSED_REPEAT && last_color_sent.full != color.full) ||
               event != LV_EVENT_LONG_PRESSED_REPEAT) {
                last_color_sent = color;
                event_object_color_event(obj, evt, color);
                // dispatch_normalized_group_value(obj->user_data.groupid, obj, val, min, max);
            }
            break;
        }

        case LV_EVENT_VALUE_CHANGED:
            break;

        case LV_EVENT_DELETE:
            LOG_VERBOSE(TAG_EVENT, F(D_OBJECT_DELETED));
            event_delete_object(obj);
            break;

        default:
            // LOG_VERBOSE(TAG_EVENT, F("Event ID: %d"), event);
            ;
    }
}