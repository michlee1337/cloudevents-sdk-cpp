#ifndef CLOUDEVENTSCPPSDK_V1_BINDING_BINDER_H
#define CLOUDEVENTSCPPSDK_V1_BINDING_BINDER_H

#include <memory>

#include <google/protobuf/message.h>

#include "third_party/statusor/statusor.h"
#include "third_party/base64/base64.h"
#include "proto/cloud_event.pb.h"
#include "external/com_google_googleapis/google/pubsub/v1/pubsub.pb.h"
#include "v1/event_format/structured_cloud_event.h"
#include "v1/event_format/json_formatter.h"
#include "v1/util/formatter_util.h"
#include "v1/util/cloud_events_util.h"

namespace cloudevents {
namespace binding {

/* 
 * Template for marshalling between protocol-specific Messages and CloudEvents.
 * Each supported ProtocolBinding will implement template specialization 
 * for the protected functions.
 *
 * User will interact with the public method functions Bind() and Unbind().
 * Bind() will marhsal a CloudEvent to a protocol-specific Message.
 * Unbind() will marshal a protocol-specific message to a CloudEvent.
 * To create a StructuredContentMode Message, pass a EventFormat parameter to Bind().
 * Code samples available in README.md.
 */
template <typename Message>
class Binder {
 public:
  // Create Binary-ContentMode Message containing CloudEvent
  absl::StatusOr<Message> Bind(
      const io::cloudevents::v1::CloudEvent& cloud_event) {
    if (auto valid = cloudevents::cloudevents_util::CloudEventsUtil::IsValid(
        cloud_event); !valid.ok()) {
      return valid;
    }

    Message msg;

    absl::StatusOr<absl::flat_hash_map<
      std::string, io::cloudevents::v1::CloudEvent_CloudEventAttribute>>
      attrs = cloudevents::cloudevents_util::CloudEventsUtil::
      GetMetadata(cloud_event);

    if (!attrs.ok()) {
      return attrs.status();
    }

    for (auto const& attr : (*attrs)) {
      std::string key = kMetadataPrefix + attr.first;
      if (auto is_md = BindMetadata(key, attr.second, msg); !is_md.ok()) {
        return is_md;
      }
    }

    switch (cloud_event.data_oneof_case()) {
      case io::cloudevents::v1::CloudEvent::DataOneofCase::kBinaryData:
        if (auto set_bin_data = BindDataBinary(cloud_event.binary_data(), msg);
          !set_bin_data.ok()) {
          return set_bin_data;
        }
      case io::cloudevents::v1::CloudEvent::DataOneofCase::kTextData:
        if (auto set_text_data = BindDataText(cloud_event.text_data(), msg);
          !set_text_data.ok()) {
          return set_text_data;
        }
      case io::cloudevents::v1::CloudEvent::DataOneofCase::kProtoData:
        // TODO (#17): CloudEvent Any in JsonFormatter
        return absl::UnimplementedError("protobuf::Any not supported yet.");
      case io::cloudevents::v1::CloudEvent::DATA_ONEOF_NOT_SET:
        break;
    }

    return msg;    
  }

  // Create Structured-ContentMode Message
  // containing Format-serialized CloudEvents
  absl::StatusOr<Message> Bind(const io::cloudevents::v1::CloudEvent& cloud_event,
      const cloudevents::format::Format& format) {
    if (auto valid = cloudevents::cloudevents_util::CloudEventsUtil::IsValid(
        cloud_event); !valid.ok()) {
      return valid;
    }

    absl::StatusOr<std::unique_ptr<cloudevents::format::Formatter>>
      get_formatter = cloudevents::formatter_util::FormatterUtil::
      GetFormatter(format);
    if (!get_formatter.ok()) {
      return get_formatter.status();
    }

    absl::StatusOr<std::unique_ptr<cloudevents::format::StructuredCloudEvent>>
      serialization = (*get_formatter)->Serialize(cloud_event);
    if (!serialization.ok()) {
      return serialization.status();
    }

    absl::StatusOr<std::string> format_str = cloudevents::formatter_util::
      FormatterUtil::FormatToStr((*serialization)->format);
    if (!format_str.ok()) {
        return format_str.status();
    }

    std::string contenttype = kContenttypePrefix;
    contenttype += *format_str;

    Message msg;
    absl::Status set_contenttype = BindContentType(contenttype, msg);
    if (!set_contenttype.ok()) {
      return set_contenttype;
    }

    if (auto set_payload = BindDataStructured(
        (*serialization)->serialized_data, msg); !set_payload.ok()) {
      return set_payload;
    }

    return msg;
  }

  // Create CloudEvent from given Message
  absl::StatusOr<io::cloudevents::v1::CloudEvent> Unbind(const Message& message) {
    absl::StatusOr<std::string> contenttype = GetContentType(message);
    if (!contenttype.ok()) {
      return contenttype.status();
    }
    if (contenttype->empty() ||
        contenttype->rfind(kContenttypePrefix, 0) != 0) {
      // Unbind Binary-ContentMode Message
      io::cloudevents::v1::CloudEvent cloud_event;
      if (auto unbind_metadata = UnbindMetadata(message, cloud_event);
          !unbind_metadata.ok()) {
        return unbind_metadata;
      }
      if (auto unbind_data = UnbindData(message, cloud_event);
          !unbind_data.ok()) {
        return unbind_data;
      }
      if (auto is_valid = cloudevents::cloudevents_util::CloudEventsUtil::
          IsValid(cloud_event); !is_valid.ok()) {
        return is_valid;
      }
      return cloud_event;
    }

    // Unbind Structured-ContentMode Message
    std::string format_str = contenttype->erase(
      0, kContenttypePrefix.length());

    absl::StatusOr<cloudevents::format::Format> format =
      cloudevents::formatter_util::FormatterUtil::FormatFromStr(format_str);
    if (!format.ok()){
      return format.status();
    }

    absl::StatusOr<std::string> get_payload = GetPayload(message);
    if (!get_payload.ok()) {
      return get_payload.status();
    }

    absl::StatusOr<std::unique_ptr<cloudevents::format::Formatter>>
      get_formatter = cloudevents::formatter_util::FormatterUtil::
      GetFormatter(*format);
    if (!get_formatter.ok()) {
      return get_formatter.status();
    }

    cloudevents::format::StructuredCloudEvent structured_cloud_event;
    structured_cloud_event.format = *format;
    structured_cloud_event.serialized_data = *get_payload;

    absl::StatusOr<io::cloudevents::v1::CloudEvent> deserialization =
      (*get_formatter)->Deserialize(structured_cloud_event);
    if (!deserialization.ok()){
      return deserialization.status();
    }

    if (auto valid = cloudevents::cloudevents_util::CloudEventsUtil::IsValid(
        *deserialization); !valid.ok()) {
      return valid;
    }

    return *deserialization;
  }

// The following operations are protocol-specific and
// will be overriden for each supported ProtocolBinding
 private:
  // Const keys used accross ProtocolBindings
  inline static const std::string kMetadataPrefix = "ce-";
  inline static const std::string kContenttypePrefix = "application/cloudevents+";
  inline static const std::string kContenttypeKey = "datacontenttype";

  // _____ Operations used in Unbind Structured _____

  absl::StatusOr<std::string> GetContentType(const Message&) {
    return absl::InternalError("Unimplemented operation");
  }

  absl::StatusOr<std::string> GetPayload(const Message&) {
    return absl::InternalError("Unimplemented operation");
  }

  // _____ Operations used in Unbind Binary _____

  absl::Status UnbindMetadata(const Message&, 
      io::cloudevents::v1::CloudEvent&) {
    return absl::InternalError("Unimplemented operation");
  }

  absl::Status UnbindData(const Message&, 
      io::cloudevents::v1::CloudEvent&) {
    return absl::InternalError("Unimplemented operation");
  }

  // _____ Operations used in Bind Structured _____

  absl::Status BindContentType(const std::string&, Message&) {
    return absl::InternalError("Unimplemented operation");
  }

  absl::Status BindDataStructured(const std::string&, Message&) {
    return absl::InternalError("Unimplemented operation");
  }

  // _____ Operations used in Bind Binary _____
  absl::Status BindMetadata(const std::string&, 
      const io::cloudevents::v1::CloudEvent_CloudEventAttribute&,
      Message&) {
    return absl::InternalError("UnimplementedOperation");  
  }

  absl::Status BindDataBinary(const std::string&, Message&) {
    return absl::InternalError("UnimplementedOperation");  
  }

  absl::Status BindDataText(const std::string&, Message&) {
    return absl::InternalError("UnimplementedOperation");  
  }
};

}  // namespace binding
}  // namespace cloudevents

#endif  // CLOUDEVENTSCPPSDK_V1_BINDING_BINDER_H