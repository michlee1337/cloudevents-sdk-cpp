#include "pubsub_binder.h"

namespace cloudevents {
namespace binding {

using ::google::pubsub::v1::PubsubMessage;
using ::io::cloudevents::v1::CloudEvent;
using ::io::cloudevents::v1::CloudEvent_CloudEventAttribute;
using ::cloudevents::format::StructuredCloudEvent;
using ::cloudevents::format::Format;
using ::cloudevents::format::Formatter;
using ::cloudevents::formatter_util::FormatterUtil;
using ::cloudevents::cloudevents_util::CloudEventsUtil;

typedef absl::flat_hash_map<std::string, CloudEvent_CloudEventAttribute>
  CeAttrMap;

inline static constexpr char kPubsubContentKey[] = "content-type";


template <>
absl::StatusOr<std::string> Binder<PubsubMessage>::GetContentType(
    const PubsubMessage& pubsub_msg) {
  google::protobuf::Map<std::string, std::string> attrs =
    pubsub_msg.attributes();
  auto ind = attrs.find(kPubsubContentKey);
  if (ind == attrs.end()) {
    return std::string("");
  }
  return ind -> second;
}

template <>
absl::StatusOr<std::string> Binder<PubsubMessage>::GetPayload(
    const PubsubMessage& pubsub_msg) {
  // get payload and base64 decode
  return base64::base64_decode(pubsub_msg.data());
}

template <>
absl::StatusOr<CloudEvent> Binder<PubsubMessage>::UnbindBinary(
    const PubsubMessage& pubsub_msg) {
  CloudEvent cloud_event;

  for (auto const& attr : pubsub_msg.attributes()) {
    std::string key;
    if (attr.first == kPubsubContentKey) {
      key = kContenttypeKey;
    } else if (attr.first.rfind(kMetadataPrefix, 0) == 0){
      size_t len_prefix = strlen(kMetadataPrefix);
      key = attr.first.substr(len_prefix, std::string::npos);
    }
    if (auto set_md = CloudEventsUtil::SetMetadata(key, attr.second,
        cloud_event); !set_md.ok()) {
      return set_md;
    }
  }

  std::string pubsub_data = pubsub_msg.data();
  if (!pubsub_data.empty()) {
    absl::StatusOr<std::string> decoded = base64::base64_decode(pubsub_data);
    if (!decoded.ok()) {
      return decoded.status();
    }
    cloud_event.set_binary_data((*decoded));
  }

  if (auto is_valid = CloudEventsUtil::IsValid(cloud_event); !is_valid.ok()) {
    return is_valid;
  }

  return cloud_event;
}

template <>
absl::Status Binder<PubsubMessage>::SetContentType(
    const std::string& contenttype, PubsubMessage& pubsub_msg) {
  (*pubsub_msg.mutable_attributes())[kPubsubContentKey] = contenttype;
  return absl::Status();
}

template <>
absl::Status Binder<PubsubMessage>::SetPayload(
    const std::string& payload, PubsubMessage& pubsub_msg) {
  pubsub_msg.set_data(payload);
  return absl::Status();
}

template <>
absl::Status Binder<PubsubMessage>::SetMetadata(const std::string& key,
    const CloudEvent_CloudEventAttribute& val, PubsubMessage& pubsub_msg) {
  absl::StatusOr<std::string> val_str = CloudEventsUtil::ToString(val);
  if (!val_str.ok()) {
    return val_str.status();
  }
  (*pubsub_msg.mutable_attributes())[key] = *val_str;
  return absl::OkStatus();
}

template <>
absl::Status Binder<PubsubMessage>::SetBinaryData(const std::string& bin_data,
    PubsubMessage& pubsub_msg) {
  // both CloudEvent.data and pubsub_msg.data are base64 encoded
  pubsub_msg.set_data(bin_data);
  return absl::OkStatus();
}

template <>
absl::Status Binder<PubsubMessage>::SetTextData(const std::string& text_data,
    PubsubMessage& pubsub_msg) {
  absl::StatusOr<std::string> encoded = base64::base64_encode(text_data);
  if (!encoded.ok()) {
    return encoded.status();
  }
  pubsub_msg.set_data(*encoded);
  return absl::OkStatus();
}

}  // namespace binding
}  // namespace cloudevents
