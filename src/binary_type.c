#include "binary_type.h"

// Returns a string representation of the Binary Type.
char *binary_type_str(enum BinaryType binary_type) {
  switch (binary_type) {
  case BT_PUT:
    return "PUT";
  case BT_DEL:
    return "DEL";
  case BT_GET:
    return "GET";
  case BT_TAKE:
    return "TAKE";
  case BT_STATS:
    return "STATS";
  case BT_OK:
    return "OK";
  case BT_EINVAL:
    return "EINVAL";
  case BT_ENOTFOUND:
    return "ENOTFOUND";
  case BT_EBINARY:
    return "EBINARY";
  case BT_EBIG:
    return "EBIG";
  case BT_EUNK:
    return "EUNK";
  default:
    return "UNKNOWN_BINARY_TYPE";
  }
}