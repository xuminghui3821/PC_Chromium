// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/automated_tests/cache_replayer.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace autofill {
namespace test {
namespace {

// Only run these tests on Linux because there are issues with other platforms.
// Testing on one platform gives enough confidence.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)

using base::JSONWriter;
using base::Value;

// Request Response Pair for the API server
using RequestResponsePair =
    std::pair<AutofillPageQueryRequest, AutofillQueryResponse>;

constexpr char kTestHTTPResponseHeader[] = "Fake HTTP Response Header";
constexpr char kHTTPBodySep[] = "\r\n\r\n";
// The host name of the autofill server.
constexpr char kHostname[] = "content-autofill.googleapis.com";

struct LightField {
  uint32_t signature;
  uint32_t prediction;
};

struct LightForm {
  uint64_t signature;
  std::vector<LightField> fields;
};

std::string CreateQueryUrl(const std::string& base64_encoded_query) {
  constexpr char base_url[] =
      "https://content-autofill.googleapis.com/v1/pages:get";
  if (base64_encoded_query.empty())
    return base_url;
  return base::StrCat({base_url, "/", base64_encoded_query});
}

bool GetServerResponseForQuery(const ServerCacheReplayer& cache_replayer,
                               const AutofillPageQueryRequest& query,
                               std::string* http_text) {
  return cache_replayer.GetApiServerResponseForQuery(query, http_text);
}

RequestResponsePair MakeQueryRequestResponsePair(
    const std::vector<LightForm>& forms) {
  AutofillPageQueryRequest query;
  AutofillQueryResponse response;
  for (const auto& form : forms) {
    auto* query_form = query.add_forms();
    query_form->set_signature(form.signature);
    auto* response_form = response.add_form_suggestions();
    for (const auto& field : form.fields) {
      query_form->add_fields()->set_signature(field.signature);
      auto* response_field = response_form->add_field_suggestions();
      response_field->set_field_signature(field.signature);
      response_field->set_primary_type_prediction(field.prediction);
      response_field->add_predictions()->set_type(field.prediction);
    }
  }
  return RequestResponsePair({std::move(query), std::move(response)});
}

// Returns a query request URL. If |query| is not empty, the corresponding
// query is encoded into the URL.
bool MakeQueryRequestURL(const base::Optional<AutofillPageQueryRequest>& query,
                         std::string* request_url) {
  if (!query.has_value()) {
    *request_url = CreateQueryUrl("");
    return true;
  }
  std::string encoded_query;
  std::string serialized_query;
  if (!(*query).SerializeToString(&serialized_query)) {
    VLOG(1) << "could not serialize Query proto";
    return false;
  }
  base::Base64Encode(serialized_query, &encoded_query);
  *request_url = CreateQueryUrl(encoded_query);
  return true;
}

// Make HTTP request header given |url|.
inline std::string MakeRequestHeader(base::StringPiece url) {
  return base::StrCat({"GET ", url, " ", "HTTP/1.1"});
}

// Makes string value for "SerializedRequest" json node that contains HTTP
// request content.
bool MakeSerializedRequest(const AutofillPageQueryRequest& query,
                           RequestType type,
                           std::string* serialized_request,
                           std::string* request_url) {
  // Make body and query content for URL depending on the |type|.
  std::string body;
  base::Optional<AutofillPageQueryRequest> query_for_url;
  if (type == RequestType::kQueryProtoGET) {
    query_for_url = std::move(query);
  } else {
    std::string serialized_query;
    std::string encoded_query;
    query.SerializeToString(&serialized_query);
    base::Base64Encode(serialized_query, &encoded_query);
    // Wrap query payload in a request proto to interface with API Query method.
    AutofillPageResourceQueryRequest request;
    request.set_serialized_request(encoded_query);
    request.SerializeToString(&body);
    query_for_url = base::nullopt;
  }

  // Make header according to query content for URL.
  std::string url;
  if (!MakeQueryRequestURL(query_for_url, &url))
    return false;
  *request_url = url;
  std::string header = MakeRequestHeader(url);

  // Fill HTTP text.
  std::string http_text =
      base::JoinString(std::vector<std::string>{header, body}, kHTTPBodySep);
  base::Base64Encode(http_text, serialized_request);
  return true;
}

std::string MakeSerializedResponse(
    const AutofillQueryResponse& query_response) {
  std::string serialized_response;
  query_response.SerializeToString(&serialized_response);

  // The Api Environment expects the response body to be base64 encoded.
  std::string tmp;
  base::Base64Encode(serialized_response, &tmp);
  serialized_response = tmp;

  std::string compressed_query;
  compression::GzipCompress(serialized_response, &compressed_query);
  // TODO(vincb): Put a real header here.
  std::string http_text = base::JoinString(
      std::vector<std::string>{kTestHTTPResponseHeader, compressed_query},
      kHTTPBodySep);
  std::string encoded_http_text;
  base::Base64Encode(http_text, &encoded_http_text);
  return encoded_http_text;
}

// Write json node to file in text format.
bool WriteJSONNode(const base::FilePath& file_path, const base::Value& node) {
  std::string json_text;
  JSONWriter::WriteWithOptions(node, JSONWriter::Options::OPTIONS_PRETTY_PRINT,
                               &json_text);

  std::string compressed_json_text;
  if (!compression::GzipCompress(json_text, &compressed_json_text)) {
    VLOG(1) << "Cannot compress json to gzip.";
    return false;
  }

  if (!base::WriteFile(file_path, compressed_json_text)) {
    VLOG(1) << "Could not write json at file: " << file_path;
    return false;
  }
  return true;
}

// Write cache to file in json text format.
bool WriteJSON(const base::FilePath& file_path,
               const std::vector<RequestResponsePair>& request_response_pairs,
               RequestType request_type = RequestType::kQueryProtoPOST) {
  // Make json list node that contains all query requests.
  base::Value::DictStorage urls_dict;
  for (const auto& request_response_pair : request_response_pairs) {
    std::string serialized_request;
    std::string url;
    if (!MakeSerializedRequest(request_response_pair.first, request_type,
                               &serialized_request, &url)) {
      return false;
    }

    Value::DictStorage request_response_node;
    request_response_node.emplace("SerializedRequest",
                                  std::move(serialized_request));
    request_response_node.emplace(
        "SerializedResponse",
        MakeSerializedResponse(request_response_pair.second));
    // Populate json dict node that contains Autofill Server requests per URL.
    // This will construct an empty list for `url` if it didn't exist already.
    auto& url_list = urls_dict.emplace(url, Value::Type::LIST).first->second;
    url_list.Append(Value(std::move(request_response_node)));
  }

  // Make json dict node that contains requests per domain.
  base::Value::DictStorage domains_dict;
  domains_dict.emplace(kHostname, std::move(urls_dict));

  // Make json root dict.
  base::Value::DictStorage root_dict;
  root_dict.emplace("Requests", std::move(domains_dict));

  // Write content to JSON file.
  return WriteJSONNode(file_path, Value(std::move(root_dict)));
}

TEST(AutofillCacheReplayerDeathTest,
     ServerCacheReplayerConstructor_CrashesWhenNoDomainNode) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // JSON structure is not right.
  const std::string invalid_json = "{\"NoDomainNode\": \"invalid_field\"}";

  // Write json to file.
  ASSERT_TRUE(base::WriteFile(file_path, invalid_json))
      << "there was an error when writing content to json file: " << file_path;

  // Crash since json content is invalid.
  ASSERT_DEATH_IF_SUPPORTED(
      ServerCacheReplayer(file_path,
                          ServerCacheReplayer::kOptionFailOnInvalidJsonRecord),
      ".*");
}

TEST(AutofillCacheReplayerDeathTest,
     ServerCacheReplayerConstructor_CrashesWhenNoQueryNodesAndFailOnEmpty) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make empty request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));

  // Crash since there are no Query nodes and set to fail on empty.
  ASSERT_DEATH_IF_SUPPORTED(
      ServerCacheReplayer(file_path,
                          ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                              ServerCacheReplayer::kOptionFailOnEmpty),
      ".*");
}

// Test suite for GET Query death test.
class AutofillCacheReplayerGETQueryDeathTest
    : public testing::TestWithParam<std::string> {};

TEST_P(
    AutofillCacheReplayerGETQueryDeathTest,
    ApiServerCacheReplayerConstructor_CrashesWhenInvalidRequestURLForGETQuery) {
  // Parameterized death test for populating cache when keys that are obtained
  // from the URL's query parameter are invalid.

  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make JSON content.

  // Make json list node that contains the problematic query request.
  Value::DictStorage request_response_node;
  // Put some textual content for HTTP request. Content does not matter because
  // the Query content will be parsed from the URL that corresponds to the
  // dictionary key.
  request_response_node.emplace(
      "SerializedRequest", base::StrCat({"GET ", CreateQueryUrl("1234").c_str(),
                                         " HTTP/1.1\r\n\r\n"}));
  request_response_node.emplace(
      "SerializedResponse", MakeSerializedResponse(AutofillQueryResponse()));

  base::Value::ListStorage url_list;
  url_list.emplace_back(std::move(request_response_node));

  // Populate json dict node that contains Autofill Server requests per URL.
  base::Value::DictStorage urls_dict;
  // The query parameter in the URL cannot be parsed to a proto because
  // parameter value is in invalid format.
  urls_dict.emplace(CreateQueryUrl(GetParam()), std::move(url_list));

  // Make json dict node that contains requests per domain.
  base::Value::DictStorage domains_dict;
  domains_dict.emplace(kHostname, std::move(urls_dict));
  // Make json root dict.
  base::Value::DictStorage root_dict;
  root_dict.emplace("Requests", std::move(domains_dict));
  // Write content to JSON file.
  ASSERT_TRUE(WriteJSONNode(file_path, Value(std::move(root_dict))));

  // Make death assertion.

  // Crash since request cannot be parsed to a proto.
  ASSERT_DEATH_IF_SUPPORTED(
      ServerCacheReplayer(file_path,
                          ServerCacheReplayer::kOptionFailOnInvalidJsonRecord),
      ".*");
}

INSTANTIATE_TEST_SUITE_P(
    GetQueryParameterizedDeathTest,
    AutofillCacheReplayerGETQueryDeathTest,
    testing::Values(  // Can be base-64 decoded, but not parsed to proto.
        "1234",
        // Cannot be base-64 decoded.
        "^^^"));

TEST(AutofillCacheReplayerTest,
     CanUseReplayerWhenNoCacheContentWithNotFailOnEmpty) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make empty request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));

  // Should not crash even if no cache because kOptionFailOnEmpty is not
  // flipped.
  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     (ServerCacheReplayer::kOptionFailOnEmpty & 0));

  // Should be able to read cache, which will give nothing.
  std::string http_text;
  AutofillPageQueryRequest query_with_no_match;
  EXPECT_FALSE(GetServerResponseForQuery(cache_replayer, query_with_no_match,
                                         &http_text));
}

template <typename U, typename V>
bool ProtobufsEqual(const U& u, const V& v) {
  // Unfortunately, Chrome uses MessageLite, so we cannot use DebugString or the
  // MessageDifferencer.
  std::string u_serialized, v_serialized;
  u.SerializeToString(&u_serialized);
  v.SerializeToString(&v_serialized);
  if (u_serialized != v_serialized) {
    LOG(ERROR) << "Expected protobufs to be equal:\n" << u << "and:\n" << v;
    LOG(ERROR) << "Note that this output is based on custom written string "
                  "serializers and the protobufs may be different in ways that "
                  "are not shown here.";
  }
  return u_serialized == v_serialized;
}

TEST(AutofillCacheReplayerTest, ProtobufConversion) {
  AutofillRandomizedFormMetadata form_metadata;
  form_metadata.mutable_id()->set_encoded_bits("foobar");

  AutofillRandomizedFieldMetadata field_metadata;
  field_metadata.mutable_id()->set_encoded_bits("foobarbaz");

  // Form 1 (fields 101, 102), Form 2 (fields 201).
  LegacyEnv::Query legacy_query;
  {
    legacy_query.set_client_version("DummyClient");
    auto* form1 = legacy_query.add_form();
    form1->set_signature(1);
    form1->mutable_form_metadata()->CopyFrom(form_metadata);
    auto* field101 = form1->add_field();
    field101->set_signature(101);
    field101->set_name("field_101");
    field101->set_type("text");
    field101->mutable_field_metadata()->CopyFrom(field_metadata);
    auto* field102 = form1->add_field();
    field102->set_signature(102);
    field102->set_name("field_102");
    field102->set_type("text");

    auto* form2 = legacy_query.add_form();
    form2->set_signature(2);
    auto* field201 = form2->add_field();
    field201->set_signature(201);
    field201->set_name("field_201");
    field201->set_type("text");

    legacy_query.add_experiments(50);
    legacy_query.add_experiments(51);
  }

  ApiEnv::Query api_query;
  {
    auto* form1 = api_query.add_forms();
    form1->set_signature(1);
    form1->mutable_metadata()->CopyFrom(form_metadata);
    auto* field101 = form1->add_fields();
    field101->set_signature(101);
    field101->set_name("field_101");
    field101->set_control_type("text");
    field101->mutable_metadata()->CopyFrom(field_metadata);
    auto* field102 = form1->add_fields();
    field102->set_signature(102);
    field102->set_name("field_102");
    field102->set_control_type("text");

    auto* form2 = api_query.add_forms();
    form2->set_signature(2);
    auto* field201 = form2->add_fields();
    field201->set_signature(201);
    field201->set_name("field_201");
    field201->set_control_type("text");

    api_query.add_experiments(50);
    api_query.add_experiments(51);
  }

  LegacyEnv::Response legacy_response;
  {
    auto* field101 = legacy_response.add_field();
    field101->set_overall_type_prediction(101);
    auto* field101_prediction = field101->add_predictions();
    field101_prediction->set_type(101);
    field101_prediction->set_may_use_prefilled_placeholder(true);
    field101_prediction = field101->add_predictions();
    field101_prediction->set_type(1010);
    field101_prediction->set_may_use_prefilled_placeholder(true);
    // Todo: Password requirements
    auto* field102 = legacy_response.add_field();
    field102->set_overall_type_prediction(102);
    auto* field102_prediction = field102->add_predictions();
    field102_prediction->set_type(102);
    field102_prediction->set_may_use_prefilled_placeholder(false);

    auto* field201 = legacy_response.add_field();
    field201->set_overall_type_prediction(201);
    field201->add_predictions()->set_type(201);
  }

  ApiEnv::Response api_response;
  {
    auto* form1 = api_response.add_form_suggestions();
    auto* field101 = form1->add_field_suggestions();
    field101->set_field_signature(101);
    field101->set_primary_type_prediction(101);
    field101->add_predictions()->set_type(101);
    field101->add_predictions()->set_type(1010);
    field101->set_may_use_prefilled_placeholder(true);
    // Todo: Password requirements
    auto* field102 = form1->add_field_suggestions();
    field102->set_field_signature(102);
    field102->set_primary_type_prediction(102);
    field102->add_predictions()->set_type(102);
    field102->set_may_use_prefilled_placeholder(false);

    auto* form2 = api_response.add_form_suggestions();
    auto* field201 = form2->add_field_suggestions();
    field201->set_field_signature(201);
    field201->set_primary_type_prediction(201);
    field201->add_predictions()->set_type(201);
  }

  // Verify equivalence of converted queries.
  EXPECT_TRUE(ProtobufsEqual(api_query, api_query));
  EXPECT_TRUE(ProtobufsEqual(api_query, ConvertQuery<ApiEnv>(api_query)));
  EXPECT_TRUE(ProtobufsEqual(api_query, ConvertQuery<LegacyEnv>(legacy_query)));

  // Verify equivalence of converted responses.
  EXPECT_TRUE(ProtobufsEqual(api_response, api_response));
  EXPECT_TRUE(ProtobufsEqual(api_response,
                             ConvertResponse<ApiEnv>(api_response, api_query)));
  EXPECT_TRUE(ProtobufsEqual(
      api_response, ConvertResponse<LegacyEnv>(legacy_response, legacy_query)));
}

// Test suite for Query response retrieval test.
class AutofillCacheReplayerGetResponseForQueryTest
    : public testing::TestWithParam<RequestType> {};

TEST_P(AutofillCacheReplayerGetResponseForQueryTest,
       FillsResponseWhenNoErrors) {
  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;
  {
    LightForm form_to_add;
    form_to_add.signature = 1234;
    form_to_add.fields = {LightField{1234, 1}};
    request_response_pairs.push_back(
        MakeQueryRequestResponsePair({form_to_add}));
  }

  // Write cache to json.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs, GetParam()));

  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     ServerCacheReplayer::kOptionFailOnEmpty);

  // Verify if we can get cached response.
  std::string http_text_response;
  ASSERT_TRUE(GetServerResponseForQuery(
      cache_replayer, request_response_pairs[0].first, &http_text_response));
  std::string body = SplitHTTP(http_text_response).second;

  // The Api Environment expects the response to be base64 encoded.
  std::string tmp;
  ASSERT_TRUE(base::Base64Decode(body, &tmp));
  body = tmp;

  AutofillQueryResponse response_from_cache;
  ASSERT_TRUE(response_from_cache.ParseFromString(body));
}

INSTANTIATE_TEST_SUITE_P(GetResponseForQueryParameterizeTest,
                         AutofillCacheReplayerGetResponseForQueryTest,
                         testing::Values(
                             // Read Query content from URL "q" param.
                             RequestType::kQueryProtoGET,
                             // Read Query content from HTTP body.
                             RequestType::kQueryProtoPOST));
TEST(AutofillCacheReplayerTest, GetResponseForQueryGivesFalseWhenNullptr) {
  ServerCacheReplayer cache_replayer(ServerCache{{}});
  EXPECT_FALSE(GetServerResponseForQuery(cache_replayer,
                                         AutofillPageQueryRequest(), nullptr));
}

TEST(AutofillCacheReplayerTest, GetResponseForQueryGivesFalseWhenNoKeyMatch) {
  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;
  {
    LightForm form_to_add;
    form_to_add.signature = 1234;
    form_to_add.fields = {LightField{1234, 1}};
    request_response_pairs.push_back(
        MakeQueryRequestResponsePair({form_to_add}));
  }

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));
  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     ServerCacheReplayer::kOptionFailOnEmpty);

  // Verify if we get false when there is no cache for the query.
  std::string http_text;
  AutofillPageQueryRequest query_with_no_match;
  EXPECT_FALSE(GetServerResponseForQuery(cache_replayer, query_with_no_match,
                                         &http_text));
}

TEST(AutofillCacheReplayerTest,
     GetResponseForQueryGivesFalseWhenDecompressFailsBecauseInvalidHTTP) {
  // Make query request and key.
  LightForm form_to_add;
  form_to_add.signature = 1234;
  form_to_add.fields = {LightField{1234, 1}};
  const AutofillPageQueryRequest query_request_for_key =
      MakeQueryRequestResponsePair({form_to_add}).first;
  const std::string key = GetKeyFromQuery<ApiEnv>(query_request_for_key);

  const char invalid_http[] = "Dumb Nonsense That Doesn't Have a HTTP Header";
  ServerCacheReplayer cache_replayer(ServerCache{{key, invalid_http}});

  // Verify if we get false when invalid HTTP response to decompress.
  std::string response_http_text;
  EXPECT_FALSE(GetServerResponseForQuery(cache_replayer, query_request_for_key,
                                         &response_http_text));
}

TEST(AutofillCacheReplayerTest,
     GetResponseForQueryGivesTrueWhenDecompressSucceededBecauseEmptyBody) {
  // Make query request and key.
  LightForm form_to_add;
  form_to_add.signature = 1234;
  form_to_add.fields = {LightField{1234, 1}};
  const AutofillPageQueryRequest query_request_for_key =
      MakeQueryRequestResponsePair({form_to_add}).first;
  const std::string key = GetKeyFromQuery<ApiEnv>(query_request_for_key);

  const char http_without_body[] = "Test HTTP Header\r\n\r\n";
  ServerCacheReplayer cache_replayer(ServerCache{{key, http_without_body}});

  // Verify if we get true when no HTTP body.
  std::string response_http_text;
  EXPECT_TRUE(GetServerResponseForQuery(cache_replayer, query_request_for_key,
                                        &response_http_text));
}

// Returns whether the forms in |response| and |forms| match. If both contain
// the same number of forms, a boolean is appended to the output for each form
// indicating whether the expectation and actual form matched. In case of
// gross mismatch, the function may return an empty vector.
std::vector<bool> DoFormsMatch(const AutofillQueryResponse& response,
                               const std::vector<LightForm>& forms) {
  std::vector<bool> found;
  for (int i = 0; i < std::min(static_cast<int>(forms.size()),
                               response.form_suggestions_size());
       ++i) {
    const auto& expected_form = forms[i];
    const auto& response_form = response.form_suggestions(i);
    if (static_cast<int>(expected_form.fields.size()) !=
        response_form.field_suggestions_size()) {
      LOG(ERROR) << "Expected form " << i << " to have suggestions for "
                 << expected_form.fields.size() << " fields but got "
                 << response_form.field_suggestions_size();
      found.push_back(false);
      continue;
    }
    bool found_all_fields = true;
    for (size_t j = 0; j < expected_form.fields.size(); ++j) {
      const auto& expected_field = expected_form.fields[j];
      if (expected_field.signature !=
          response_form.field_suggestions(j).field_signature()) {
        LOG(ERROR) << "Expected field " << j << " of form " << i
                   << " to have signature " << expected_field.signature
                   << " but got "
                   << response_form.field_suggestions(j).field_signature();
        found_all_fields = false;
      }
      if (expected_field.prediction !=
          static_cast<unsigned int>(
              response_form.field_suggestions(j).primary_type_prediction())) {
        LOG(ERROR)
            << "Expected field " << j << " of form " << i
            << " to have primary type prediction " << expected_field.prediction
            << " but got "
            << response_form.field_suggestions(j).primary_type_prediction();
        found_all_fields = false;
      }
    }
    found.push_back(found_all_fields);
  }
  return found;
}

std::vector<bool> CheckFormsInCache(const ServerCacheReplayer& cache_replayer,
                                    const std::vector<LightForm>& forms) {
  RequestResponsePair request_response_pair =
      MakeQueryRequestResponsePair(forms);
  std::string http_text;
  if (!GetServerResponseForQuery(cache_replayer, request_response_pair.first,
                                 &http_text)) {
    VLOG(1) << "Server did not respond to the query.";
    return std::vector<bool>();
  }
  std::string body = SplitHTTP(http_text).second;

  // The Api Environment expects the response to be base64 encoded.
  std::string tmp;
  if (!base::Base64Decode(body, &tmp)) {
    LOG(ERROR) << "Unable to base64 decode contents" << body;
    return std::vector<bool>();
  }
  body = tmp;

  AutofillQueryResponse response;
  CHECK(response.ParseFromString(body)) << body;
  return DoFormsMatch(response, forms);
}

TEST(AutofillCacheReplayerTest, CrossEnvironmentIntegrationTest) {
  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  LightForm form1;
  form1.signature = 1111;
  form1.fields = {LightField{1111, 1}, LightField{1112, 31},
                  LightField{1113, 33}};
  LightForm form2;
  form2.signature = 2222;
  form2.fields = {LightField{2221, 2}};
  LightForm form3;
  form3.signature = 3333;
  form3.fields = {LightField{3331, 3}};
  LightForm form4;
  form4.signature = 4444;
  form4.fields = {LightField{4441, 4}};
  LightForm form5;
  form5.signature = 5555;
  form5.fields = {LightField{5551, 42}};

  // Make request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;
  request_response_pairs.push_back(
      MakeQueryRequestResponsePair({form1, form2}));
  request_response_pairs.push_back(
      MakeQueryRequestResponsePair({form3, form4}));

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));
  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     ServerCacheReplayer::kOptionFailOnEmpty);

  std::string http_text;

  // First, check the exact same key combos we sent properly respond
  EXPECT_EQ(std::vector<bool>({true, true}),
            CheckFormsInCache(cache_replayer, {form1, form2}));
  EXPECT_EQ(std::vector<bool>({true, true}),
            CheckFormsInCache(cache_replayer, {form3, form4}));

  // Existing keys that were requested in a different combination are not
  // processed.
  EXPECT_EQ(std::vector<bool>(),
            CheckFormsInCache(cache_replayer, {form1, form3}));
  EXPECT_EQ(std::vector<bool>(), CheckFormsInCache(cache_replayer, {form1}));

  // Not in the cache.
  EXPECT_EQ(std::vector<bool>(), CheckFormsInCache(cache_replayer, {form5}));

  // Now, load the same thing into the cache replayer with
  // ServerCacheReplayer::kOptionSplitRequestsByForm set and expect matches
  // for all combos
  ServerCacheReplayer form_split_cache_replayer(
      file_path, ServerCacheReplayer::kOptionSplitRequestsByForm);

  // First, check the exact same key combos we sent properly respond
  EXPECT_EQ(std::vector<bool>({true, true}),
            CheckFormsInCache(form_split_cache_replayer, {form1, form2}));
  EXPECT_EQ(std::vector<bool>({true, true}),
            CheckFormsInCache(form_split_cache_replayer, {form3, form4}));

  // Existing keys that were requested in a different combination are not
  // processed.
  EXPECT_EQ(std::vector<bool>({true, true}),
            CheckFormsInCache(form_split_cache_replayer, {form1, form3}));
  EXPECT_EQ(std::vector<bool>({true}),
            CheckFormsInCache(form_split_cache_replayer, {form1}));

  // Not in the cache.
  EXPECT_EQ(std::vector<bool>(),
            CheckFormsInCache(form_split_cache_replayer, {form5}));
}
#endif  // if defined(OS_LINUX) || defined(OS_CHROMEOS)
}  // namespace
}  // namespace test
}  // namespace autofill
