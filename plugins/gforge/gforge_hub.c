#include "../../filly-core/widget.h"
#include "../../filly-core/widgets/hub.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"

Widget *gforge_hub_factory(const WidgetRequest *req) {
    return hub_widget_new(
        cJSON_GetObjectItem(req->params, "title")->valuestring,
        cJSON_GetObjectItem(req->params, "categories"),
        cJSON_GetObjectItem(req->params, "actions")
    );
}