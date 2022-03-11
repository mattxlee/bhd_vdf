#include "vdf_net.hpp"

std::string to_string(ActionType action_type) {
  switch (action_type) {
    case ActionType::Read:
      return "read";
    case ActionType::Write:
      return "write";
    case ActionType::Connect:
      return "connect";
  }
  throw std::runtime_error("invalid action type");
}
