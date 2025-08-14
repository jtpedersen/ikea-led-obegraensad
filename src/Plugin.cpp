#include "Plugin.h"

void Plugin::setId(int id) { this->id = id; }

int Plugin::getId() const { return id; }

void Plugin::teardown() {}
void Plugin::loop() {}

void Plugin::websocketHook(const DynamicJsonDocument &request){}
