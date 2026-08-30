#include "app_state.h"

namespace app_state {

Runtime &get() {
  static Runtime runtime;
  return runtime;
}

}  // namespace app_state

