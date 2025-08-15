#pragma once

#include "PluginManager.h"

class DrawPlugin : public Plugin
{
public:
  void setup() override;
  void teardown() override;
  const char *getName() const override;
  void websocketHook(const DynamicJsonDocument &request) override;
};
