// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_contact_handler.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/logging/proto_to_dictionary_conversion.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"

namespace {

std::string FormatAsJSON(const base::Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

base::Value GetJavascriptTimestamp() {
  return base::Value(base::Time::Now().ToJsTimeIgnoringNull());
}

// Keys in the JSON representation of a contact message
const char kContactMessageTimeKey[] = "time";
const char kContactMessageContactsChangedKey[] = "contactsChanged";
const char kContactMessageAllowedIdsKey[] = "allowedIds";
const char kContactMessageContactRecordKey[] = "contactRecords";
const char kContactMessageNumUnreachableContactsKey[] =
    "numUnreachableContacts";

// Converts Contact to a raw dictionary value used as a JSON argument to
// JavaScript functions.
// TODO(nohle): We should probably break up this dictionary into smaller
// dictionaries corresponding to each contact-manager observer functions. This
// will require changes at the javascript layer as well.
base::Value ContactMessageToDictionary(
    base::Optional<bool> did_contacts_change_since_last_upload,
    const base::Optional<std::set<std::string>>& allowed_contact_ids,
    const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
        contacts,
    base::Optional<uint32_t> num_unreachable_contacts_filtered_out) {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  dictionary.SetKey(kContactMessageTimeKey, GetJavascriptTimestamp());
  if (did_contacts_change_since_last_upload.has_value()) {
    dictionary.SetBoolKey(kContactMessageContactsChangedKey,
                          *did_contacts_change_since_last_upload);
  }
  if (allowed_contact_ids) {
    base::Value::ListStorage allowed_ids_list;
    allowed_ids_list.reserve(allowed_contact_ids->size());
    for (const auto& contact_id : *allowed_contact_ids) {
      allowed_ids_list.push_back(base::Value(contact_id));
    }
    dictionary.SetStringKey(
        kContactMessageAllowedIdsKey,
        FormatAsJSON(base::Value(std::move(allowed_ids_list))));
  }
  if (contacts) {
    base::Value::ListStorage contact_list;
    contact_list.reserve(contacts->size());
    for (const auto& contact : *contacts)
      contact_list.push_back(
          base::Value(ContactRecordToReadableDictionary(contact)));

    dictionary.SetStringKey(kContactMessageContactRecordKey,
                            FormatAsJSON(base::Value(std::move(contact_list))));
  }
  if (num_unreachable_contacts_filtered_out.has_value()) {
    dictionary.SetIntKey(kContactMessageNumUnreachableContactsKey,
                         *num_unreachable_contacts_filtered_out);
  }
  return dictionary;
}

}  // namespace

NearbyInternalsContactHandler::NearbyInternalsContactHandler(
    content::BrowserContext* context)
    : context_(context) {}

NearbyInternalsContactHandler::~NearbyInternalsContactHandler() = default;

void NearbyInternalsContactHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeContacts",
      base::BindRepeating(&NearbyInternalsContactHandler::InitializeContents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "downloadContacts",
      base::BindRepeating(
          &NearbyInternalsContactHandler::HandleDownloadContacts,
          base::Unretained(this)));
}

void NearbyInternalsContactHandler::OnJavascriptAllowed() {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (service_) {
    observation_.Observe(service_->GetContactManager());
  } else {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
  }
}

void NearbyInternalsContactHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void NearbyInternalsContactHandler::InitializeContents(
    const base::ListValue* args) {
  AllowJavascript();
}

void NearbyInternalsContactHandler::HandleDownloadContacts(
    const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (service_) {
    service_->GetContactManager()->DownloadContacts();
  } else {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
  }
}

void NearbyInternalsContactHandler::OnContactsDownloaded(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearbyshare::proto::ContactRecord>& contacts,
    uint32_t num_unreachable_contacts_filtered_out) {
  FireWebUIListener("contacts-updated",
                    ContactMessageToDictionary(
                        /*did_contacts_change_since_last_upload=*/base::nullopt,
                        allowed_contact_ids, contacts,
                        num_unreachable_contacts_filtered_out));
}

void NearbyInternalsContactHandler::OnContactsUploaded(
    bool did_contacts_change_since_last_upload) {
  FireWebUIListener(
      "contacts-updated",
      ContactMessageToDictionary(
          did_contacts_change_since_last_upload,
          /*allowed_contact_ids=*/base::nullopt,
          /*contacts=*/base::nullopt,
          /*num_unreachable_contacts_filtered_out=*/base::nullopt));
}
