#pragma once

#include <ArduinoJson.h>

class Plugin {
private:
  int id = -1;

public:
  virtual ~Plugin() = default;

  virtual void teardown();
  virtual void websocketHook(const DynamicJsonDocument &request);
  virtual void setup() = 0;
  virtual void loop();
  virtual const char *getName() const = 0;

  void setId(int id);
  int getId() const;
};
