#include "core/widget.h"
#include "core/widgets/hub.h"
#include "protocol/protocol.h"
#include "cJSON.h"

Widget *gforge_hub_factory(const WidgetRequest *req) {
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    return hub_widget_new(
        title_j && title_j->valuestring ? title_j->valuestring : "",
        cJSON_GetObjectItem(req->params, "categories"),
        cJSON_GetObjectItem(req->params, "actions")
    );
}