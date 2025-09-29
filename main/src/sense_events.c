#include "sense_events.h"

EventGroupHandle_t se_events;
static StaticEventGroup_t se_events_buf;

void se_init() {
    se_events = xEventGroupCreateStatic(&se_events_buf);
}