#include "WebBoot.h"
#include "Globals.h"
#include "WebInterfaceManager.h"
#include "WebPolicy.h"

void WebBoot::plan() {
    beginWebInterface();
    setWebInterfaceActive(true);
    PL("[Conduct][Plan] Web interface initialized");
    WebPolicy::configure();
}
