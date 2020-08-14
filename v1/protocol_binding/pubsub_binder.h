#ifndef CLOUDEVENTSCPPSDK_V1_BINDING_PUBSUBBINDER_H
#define CLOUDEVENTSCPPSDK_V1_BINDING_PUBSUBBINDER_H

#include "external/googleapis/google/pubsub/v1/pubsub.pb.h"
#include "v1/protocol_binding/binder.h"

namespace cloudevents {
namespace binding {

/*
 * Template specialization to handle PubsubMessages.
 * For the full template code and comments,
 * view `//v1/protocol_binding/binder.h`.
 */
template <>
absl::StatusOr<std::string> 
    Binder<google::pubsub::v1::PubsubMessage>::GetContentType(
    const google::pubsub::v1::PubsubMessage& pubsub_msg);

template <>
absl::StatusOr<std::string> 
    Binder<google::pubsub::v1::PubsubMessage>::GetPayload(
    const google::pubsub::v1::PubsubMessage& pubsub_msg);

template <>
absl::StatusOr<io::cloudevents::v1::CloudEvent> 
    Binder<google::pubsub::v1::PubsubMessage>::UnbindBinary(
    const google::pubsub::v1::PubsubMessage& pubsub_msg);

template <>
absl::Status Binder<google::pubsub::v1::PubsubMessage>::SetContentType(
    const std::string& contenttype, google::pubsub::v1::PubsubMessage& message);

template <>
absl::Status Binder<google::pubsub::v1::PubsubMessage>::SetPayload(
    const std::string& payload, google::pubsub::v1::PubsubMessage& message);

template <>
absl::StatusOr<google::pubsub::v1::PubsubMessage> 
    Binder<google::pubsub::v1::PubsubMessage>::BindBinary(
    const io::cloudevents::v1::CloudEvent& cloud_event);

} // binding
} // cloudevents

#endif // CLOUDEVENTSCPPSDK_V1_BINDING_PUBSUBBINDER_H
