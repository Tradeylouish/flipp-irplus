#include <furi.h>
#include <furi_hal_bt.h>
#include <furi_hal_bt_serial.h>
#include <infrared_transmit.h>
#include <bt/bt_service/bt.h>
#include <gui/gui.h>
#include <gui/icon_i.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/popup.h>

#define TAG "app"
const uint16_t BT_SERIAL_BUFFER_SIZE = 128;

/* generated by fbt from .png files in images folder */
#include <flipp_irplus_icons.h>

/** ids for all scenes used by the app */
typedef enum { AppScene_Popup, AppScene_count } AppScene;

/** ids for the 2 types of view used by the app */
typedef enum { AppView_Popup } AppView;

/** the app context struct */
typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Popup* popup;

    Bt* bt;
    bool bt_connected;
} App;

// THESE ARE THE FUNCTIONS FOR THE ACTUAL POPUP

void app_scene_on_enter_popup(void* context) {
    FURI_LOG_T(TAG, "app_scene_on_enter_popup_one");
    App* app = context;
    popup_reset(app->popup);
    popup_set_context(app->popup, app);
    popup_set_header(app->popup, "Forwarding", 64, 10, AlignLeft, AlignTop);
    popup_set_icon(app->popup, 10, 10, &I_cvc_36x36);
    popup_set_text(app->popup, "irplus signals \nvia bluetooth", 64, 20, AlignLeft, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Popup);
}

bool app_scene_on_event_popup(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "app_scene_on_event_popup");
    UNUSED(context);
    UNUSED(event);
    return false; // don't handle any events
}

void app_scene_on_exit_popup(void* context) {
    FURI_LOG_T(TAG, "app_scene_on_exit_popup");
    App* app = context;
    popup_reset(app->popup);
}

// APP BOILERPLATE

/** collection of all scene on_enter handlers - in the same order as their enum */
void (*const app_scene_on_enter_handlers[])(void*) = {app_scene_on_enter_popup};

/** collection of all scene on event handlers - in the same order as their enum */
bool (*const app_scene_on_event_handlers[])(void*, SceneManagerEvent) = {app_scene_on_event_popup};

/** collection of all scene on exit handlers - in the same order as their enum */
void (*const app_scene_on_exit_handlers[])(void*) = {app_scene_on_exit_popup};

/** collection of all on_enter, on_event, on_exit handlers */
const SceneManagerHandlers app_scene_event_handlers = {
    .on_enter_handlers = app_scene_on_enter_handlers,
    .on_event_handlers = app_scene_on_event_handlers,
    .on_exit_handlers = app_scene_on_exit_handlers,
    .scene_num = AppScene_count};

/** custom event handler - passes the event to the scene manager */
bool app_scene_manager_custom_event_callback(void* context, uint32_t custom_event) {
    FURI_LOG_T(TAG, "app_scene_manager_custom_event_callback");
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

/** navigation event handler - passes the event to the scene manager */
bool app_scene_manager_navigation_event_callback(void* context) {
    FURI_LOG_T(TAG, "app_scene_manager_navigation_event_callback");
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/** initialise the scene manager with all handlers */
void app_scene_manager_init(App* app) {
    FURI_LOG_T(TAG, "app_scene_manager_init");
    app->scene_manager = scene_manager_alloc(&app_scene_event_handlers, app);
}

/** initialise the views, and initialise the view dispatcher with all views */
void app_view_dispatcher_init(App* app) {
    FURI_LOG_T(TAG, "app_view_dispatcher_init");
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);

    // allocate each view
    FURI_LOG_D(TAG, "app_view_dispatcher_init allocating views");
    app->popup = popup_alloc();

    // assign callback that pass events from views to the scene manager
    FURI_LOG_D(TAG, "app_view_dispatcher_init setting callbacks");
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, app_scene_manager_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, app_scene_manager_navigation_event_callback);

    // add views to the dispatcher, indexed by their enum value
    FURI_LOG_D(TAG, "app_view_dispatcher_init adding view popup");
    view_dispatcher_add_view(app->view_dispatcher, AppView_Popup, popup_get_view(app->popup));
}

/** initialise app data, scene manager, and view dispatcher */
App* app_init() {
    FURI_LOG_T(TAG, "app_init");
    App* app = malloc(sizeof(App));
    app_scene_manager_init(app);
    app_view_dispatcher_init(app);

    app->bt_connected = false;
    app->bt = furi_record_open(RECORD_BT);

    return app;
}

/** free all app data, scene manager, view dispatcher, and bluetooth connection */
void app_free(App* app) {
    FURI_LOG_T(TAG, "app_free");
    scene_manager_free(app->scene_manager);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Popup);
    view_dispatcher_free(app->view_dispatcher);
    popup_free(app->popup);

    furi_record_close(RECORD_BT);
    app->bt = NULL;

    free(app);
}

// THIS IS THE BLUETOOTH STUFF

static uint16_t bt_serial_event_callback(SerialServiceEvent event, void* context) {
    furi_assert(context);
    Bt* bt = context;
    UNUSED(bt);
    uint16_t ret = 0;

    //printf("Receiving a BT serial message: ");

    if(event.event == SerialServiceEventTypeDataReceived) {
        FURI_LOG_D(TAG, "SerialServiceEventTypeDataReceived");
        FURI_LOG_D(TAG, "Size: %u", event.data.size);
        FURI_LOG_D(TAG, "Data: ");
        for(size_t i = 0; i < event.data.size; i++) {
            printf("%X ", event.data.buffer[i]);
        }
        printf("\r\n");
    } else if(event.event == SerialServiceEventTypeDataSent) {
        FURI_LOG_D(TAG, "SerialServiceEventTypeDataSent");
        FURI_LOG_D(TAG, "Size: %u", event.data.size);
        FURI_LOG_D(TAG, "Data: ");
        for(size_t i = 0; i < event.data.size; i++) {
            printf("%X ", event.data.buffer[i]);
        }
        printf("\r\n");
    }

    // Sending the received signal over IR

    // Hard-coded signal for testing
    //const uint32_t rawData[] = {
    //    6212, 560, 1605, 560, 1498, 560, 1493, 560, 560}; // Using exact NEC timing

    // Last two are stop bytes, so can leave out
    const uint16_t numberOfDataElements = (event.data.size - 2) / 2;

    uint32_t rawData[numberOfDataElements];

    for(size_t i = 0; i < numberOfDataElements; i++) {
        //Combine adjacent pairs of bytes by bitshifting
        uint32_t firstByte = event.data.buffer[2 * i];
        uint32_t secondByte = event.data.buffer[2 * i + 1];

        rawData[i] = (firstByte << 8) | secondByte;
        printf("%lX ", rawData[i]);
    }
    printf("\r\n");

    infrared_send_raw_ext(rawData, sizeof(rawData) / sizeof(rawData[0]), true, 38000, 0.33);

    return ret;
}

// THIS IS THE MAIN ENTRY POINT FUNCTION

int32_t template_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "App starting...");

    // create the app context struct, scene manager, and view dispatcher

    App* app = app_init();

    // Bluetooth stuff
    if(furi_hal_bt_is_active()) {
        FURI_LOG_D(TAG, "BT is working, hijacking the serial connection...");
        furi_hal_bt_serial_set_event_callback(
            BT_SERIAL_BUFFER_SIZE, bt_serial_event_callback, app);
        furi_hal_bt_start_advertising();
    } else {
        FURI_LOG_D(TAG, "Please, enable the Bluetooth and restart the app");
    }

    // set the scene and launch the main loop
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, AppScene_Popup);
    FURI_LOG_D(TAG, "Starting dispatcher...");
    view_dispatcher_run(app->view_dispatcher);

    furi_hal_bt_serial_set_event_callback(0, NULL, NULL);

    // free all memory
    FURI_LOG_I(TAG, "App finishing...");
    furi_record_close(RECORD_GUI);
    app_free(app);

    return 0;
}
