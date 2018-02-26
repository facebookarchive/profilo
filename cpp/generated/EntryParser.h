// Copyright 2004-present Facebook. All Rights Reserved.
// @generated SignedSource<<4c156e08fa1208cad570e4bc93e0a96c>>

#pragma once

#include <cstdint>
#include <cstring>
#include <unistd.h>

#include <stdexcept>
#include <ostream>

#include <loom/entries/EntryType.h>
#include <loom/entries/Entry.h>

namespace facebook {
namespace profilo {
namespace entries {

class EntryVisitor {
public:

  virtual void visit(const StandardEntry& entry) = 0;
  virtual void visit(const FramesEntry& entry) = 0;
  virtual void visit(const BytesEntry& entry) = 0;
};

class EntryParser {
public:

  static void parse(const void* src, size_t size, EntryVisitor& visitor) {
    uint8_t type = entries::peek_type(src, size);
    switch (type) {
      case 1: {
        StandardEntry data;
        StandardEntry::unpack(data, src, size);
        visitor.visit(data);
        break;
      }
      
      case 2: {
        FramesEntry data;
        FramesEntry::unpack(data, src, size);
        visitor.visit(data);
        break;
      }
      
      case 3: {
        BytesEntry data;
        BytesEntry::unpack(data, src, size);
        visitor.visit(data);
        break;
      }
      
      default: throw std::invalid_argument("Unknown type in to_stream");
    }
  }
  
};

} // namespace entries
} // namespace profilo
} // namespace facebook
