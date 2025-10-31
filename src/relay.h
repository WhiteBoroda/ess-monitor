#ifndef _RELAY_H
#define _RELAY_H

#include <stdint.h>

namespace RELAY {

void begin();
void triggerPulse();
bool isEnabled();

} // namespace RELAY

#endif
