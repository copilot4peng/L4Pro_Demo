#include "SearchlightConfig.h"

namespace SearchlightControl {

SearchlightInitConfig::SearchlightInitConfig()
    // : driver("/dev/tty.usbserial-14220"),
    : driver("/dev/ttyUSB0"),
      baud_rate(115200),
      print_raw_serial(false),
      init_wait_ms(1200),
      offline_timeout_ms(2000) {}

}  // namespace SearchlightControl
