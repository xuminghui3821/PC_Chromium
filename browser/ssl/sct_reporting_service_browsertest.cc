// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/sct_reporting_service.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/network_service_test_helper.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/test/test_url_loader_factory.h"

namespace {

// These LogId constants allow test cases to specify SCTs from both Google and
// non-Google logs, allowing tests to vary how they meet (or don't meet) the
// Chrome CT policy. To be compliant, the cert used by the embedded test server
// currently requires three embedded SCTs, including at least one from a Google
// log and one from a non-Google log.
//
// Google's "Argon2023" log ("6D7Q2j71BjUy51covIlryQPTy9ERa+zraeF3fW0GvW4="):
const char kTestGoogleLogId[] = {
    0xe8, 0x3e, 0xd0, 0xda, 0x3e, 0xf5, 0x06, 0x35, 0x32, 0xe7, 0x57,
    0x28, 0xbc, 0x89, 0x6b, 0xc9, 0x03, 0xd3, 0xcb, 0xd1, 0x11, 0x6b,
    0xec, 0xeb, 0x69, 0xe1, 0x77, 0x7d, 0x6d, 0x06, 0xbd, 0x6e};
// Cloudflare's "Nimbus2023" log
// ("ejKMVNi3LbYg6jjgUh7phBZwMhOFTTvSK8E6V6NS61I="):
const char kTestNonGoogleLogId1[] = {
    0x7a, 0x32, 0x8c, 0x54, 0xd8, 0xb7, 0x2d, 0xb6, 0x20, 0xea, 0x38,
    0xe0, 0x52, 0x1e, 0xe9, 0x84, 0x16, 0x70, 0x32, 0x13, 0x85, 0x4d,
    0x3b, 0xd2, 0x2b, 0xc1, 0x3a, 0x57, 0xa3, 0x52, 0xeb, 0x52};
// DigiCert's "Yeti2023" log ("Nc8ZG7+xbFe/D61MbULLu7YnICZR6j/hKu+oA8M71kw="):
const char kTestNonGoogleLogId2[] = {
    0x35, 0xcf, 0x19, 0x1b, 0xbf, 0xb1, 0x6c, 0x57, 0xbf, 0x0f, 0xad,
    0x4c, 0x6d, 0x42, 0xcb, 0xbb, 0xb6, 0x27, 0x20, 0x26, 0x51, 0xea,
    0x3f, 0xe1, 0x2a, 0xef, 0xa8, 0x03, 0xc3, 0x3b, 0xd6, 0x4c};

// Constructs a net::SignedCertificateTimestampAndStatus with the given
// information and appends it to |sct_list|.
void MakeTestSCTAndStatus(
    net::ct::SignedCertificateTimestamp::Origin origin,
    const std::string& extensions,
    const std::string& signature_data,
    const base::Time& timestamp,
    const std::string& log_id,
    net::ct::SCTVerifyStatus status,
    net::SignedCertificateTimestampAndStatusList* sct_list) {
  scoped_refptr<net::ct::SignedCertificateTimestamp> sct(
      new net::ct::SignedCertificateTimestamp());
  sct->version = net::ct::SignedCertificateTimestamp::V1;
  sct->log_id = log_id;
  sct->extensions = extensions;
  sct->timestamp = timestamp;
  sct->signature.signature_data = signature_data;
  sct->origin = origin;
  sct_list->push_back(net::SignedCertificateTimestampAndStatus(sct, status));
}

}  // namespace

class SCTReportingServiceBrowserTest : public CertVerifierBrowserTest {
 public:
  SCTReportingServiceBrowserTest() {
    // Set sampling rate to 1.0 to ensure deterministic behavior.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSCTAuditing,
          {{features::kSCTAuditingSamplingRate.name, "1.0"}}}},
        {});
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
    // The report server must be initialized here so the reporting URL can be
    // set before the network service is initialized.
    ignore_result(report_server()->InitializeAndListen());
    SCTReportingService::GetReportURLInstance() = report_server()->GetURL("/");
  }
  ~SCTReportingServiceBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }

  SCTReportingServiceBrowserTest(const SCTReportingServiceBrowserTest&) =
      delete;
  const SCTReportingServiceBrowserTest& operator=(
      const SCTReportingServiceBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    report_server()->RegisterRequestHandler(base::BindRepeating(
        &SCTReportingServiceBrowserTest::HandleReportRequest,
        base::Unretained(this)));
    report_server()->StartAcceptingConnections();
    ASSERT_TRUE(https_server()->Start());

    // Mock the cert verify results so that it has valid CT verification
    // results.
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = https_server()->GetCertificate().get();
    verify_result.is_issued_by_known_root = true;
    // Add three "valid" SCTs and mark the certificate as compliant.
    // The default test set up is embedded SCTs where one SCT is from a Google
    // log and two are from non-Google logs (to meet the Chrome CT policy).
    MakeTestSCTAndStatus(
        net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
        "signature1", base::Time::Now(),
        std::string(kTestGoogleLogId, base::size(kTestGoogleLogId)),
        net::ct::SCT_STATUS_OK, &verify_result.scts);
    MakeTestSCTAndStatus(
        net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
        "signature2", base::Time::Now(),
        std::string(kTestNonGoogleLogId1, base::size(kTestNonGoogleLogId1)),
        net::ct::SCT_STATUS_OK, &verify_result.scts);
    MakeTestSCTAndStatus(
        net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
        "signature3", base::Time::Now(),
        std::string(kTestNonGoogleLogId2, base::size(kTestNonGoogleLogId2)),
        net::ct::SCT_STATUS_OK, &verify_result.scts);

    // Set up two test hosts as using publicly-issued certificates for testing.
    mock_cert_verifier()->AddResultForCertAndHost(
        https_server()->GetCertificate().get(), "a.test", verify_result,
        net::OK);
    mock_cert_verifier()->AddResultForCertAndHost(
        https_server()->GetCertificate().get(), "b.test", verify_result,
        net::OK);

    // Set up a third (internal) test host for FlushAndCheckZeroReports().
    mock_cert_verifier()->AddResultForCertAndHost(
        https_server()->GetCertificate().get(),
        "flush-and-check-zero-reports.test", verify_result, net::OK);

    CertVerifierBrowserTest::SetUpOnMainThread();
  }

 protected:
  void SetExtendedReportingEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kSafeBrowsingScoutReportingEnabled, enabled);
  }
  void SetSafeBrowsingEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 enabled);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }
  net::EmbeddedTestServer* report_server() { return &report_server_; }

  void WaitForRequests(size_t num_requests) {
    // Each loop iteration will account for one request being processed. (This
    // simplifies the request handler code below, and reduces the state that
    // must be tracked and handled under locks.)
    while (true) {
      base::RunLoop run_loop;
      {
        base::AutoLock auto_lock(requests_lock_);
        if (requests_seen_ >= num_requests)
          return;
        requests_closure_ = run_loop.QuitClosure();
      }
      run_loop.Run();
    }
  }

  size_t requests_seen() {
    base::AutoLock auto_lock(requests_lock_);
    return requests_seen_;
  }

  sct_auditing::SCTClientReport GetLastSeenReport() {
    base::AutoLock auto_lock(requests_lock_);
    sct_auditing::SCTClientReport auditing_report;
    if (last_seen_request_.has_content)
      auditing_report.ParseFromString(last_seen_request_.content);
    return auditing_report;
  }

  // Checks that no reports have been sent. To do this, opt-in the profile,
  // make a new navigation, and check that there is only a single report and it
  // was for this new navigation specifically. This should be used at the end of
  // any negative tests to reduce the chance of false successes.
  bool FlushAndCheckZeroReports() {
    SetSafeBrowsingEnabled(true);
    SetExtendedReportingEnabled(true);
    ui_test_utils::NavigateToURL(
        browser(),
        https_server()->GetURL("flush-and-check-zero-reports.test", "/"));
    WaitForRequests(1);
    return (1u == requests_seen() &&
            "flush-and-check-zero-reports.test" == GetLastSeenReport()
                                                       .certificate_report(0)
                                                       .context()
                                                       .origin()
                                                       .hostname());
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleReportRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(requests_lock_);
    last_seen_request_ = request;
    ++requests_seen_;
    if (requests_closure_)
      std::move(requests_closure_).Run();

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    return http_response;
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer report_server_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // `requests_lock_` is used to force sequential access to these variables to
  // avoid races that can cause test flakes.
  base::Lock requests_lock_;
  net::test_server::HttpRequest last_seen_request_;
  size_t requests_seen_ = 0;
  base::OnceClosure requests_closure_;
};

// Tests that reports should not be sent when extended reporting is not opted
// in.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       NotOptedIn_ShouldNotEnqueueReport) {
  SetExtendedReportingEnabled(false);

  // Visit an HTTPS page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));

  // Check that no reports are sent.
  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that reports should be sent when extended reporting is opted in.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       OptedIn_ShouldEnqueueReport) {
  SetExtendedReportingEnabled(true);

  // Visit an HTTPS page and wait for the report to be sent.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  WaitForRequests(1);

  // Check that one report was sent and contains the expected details.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

// Tests that disabling Safe Browsing entirely should cause reports to not get
// sent.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, DisableSafebrowsing) {
  SetSafeBrowsingEnabled(false);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that we don't send a report for a navigation with a cert error.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CertErrorDoesNotEnqueueReport) {
  SetExtendedReportingEnabled(true);

  // Visit a page with an invalid cert.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("invalid.test", "/"));

  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that reports aren't sent for Incognito windows.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       IncognitoWindow_ShouldNotEnqueueReport) {
  // Enable SBER in the main profile.
  SetExtendedReportingEnabled(true);

  // Create a new Incognito window.
  auto* incognito = CreateIncognitoBrowser();

  ui_test_utils::NavigateToURL(incognito, https_server()->GetURL("/"));

  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

// Tests that disabling Extended Reporting causes the cache to be cleared.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       OptingOutClearsSCTAuditingCache) {
  // Enable SCT auditing and enqueue a report.
  SetExtendedReportingEnabled(true);

  // Visit an HTTPS page and wait for a report to be sent.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  WaitForRequests(1);

  // Check that one report was sent.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());

  // Disable Extended Reporting which should clear the underlying cache.
  SetExtendedReportingEnabled(false);

  // We can check that the same report gets cached again instead of being
  // deduplicated (i.e., another report should be sent).
  SetExtendedReportingEnabled(true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  WaitForRequests(2);
  EXPECT_EQ(2u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

// Tests that reports are still sent for opted-in profiles after the network
// service crashes and is restarted.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       ReportsSentAfterNetworkServiceRestart) {
  // This test is only applicable to out-of-process network service because it
  // tests what happens when the network service crashes and restarts.
  if (content::IsInProcessNetworkService()) {
    return;
  }

  SetExtendedReportingEnabled(true);

  // Crash the NetworkService to force it to restart.
  SimulateNetworkServiceCrash();
  // Flush the network interface to make sure it notices the crash.
  content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
      ->FlushNetworkInterfaceForTesting();
  g_browser_process->system_network_context_manager()
      ->FlushNetworkInterfaceForTesting();

  // The mock cert verify result will be lost when the network service restarts,
  // so set back up the necessary rule for the test host.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = true;
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
      "signature1", base::Time::Now(),
      std::string(kTestGoogleLogId, base::size(kTestGoogleLogId)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
      "signature2", base::Time::Now(),
      std::string(kTestNonGoogleLogId1, base::size(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
      "signature3", base::Time::Now(),
      std::string(kTestNonGoogleLogId2, base::size(kTestNonGoogleLogId2)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "a.test", verify_result, net::OK);

  // Visit an HTTPS page and wait for the report to be sent.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("a.test", "/"));
  WaitForRequests(1);

  // Check that one report was enqueued.
  EXPECT_EQ(1u, requests_seen());
  EXPECT_EQ(
      "a.test",
      GetLastSeenReport().certificate_report(0).context().origin().hostname());
}

// Tests that invalid SCTs don't get reported when the overall result is
// compliant with CT policy.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CTCompliantInvalidSCTsNotReported) {
  // Set up a mocked CertVerifyResult that includes both valid and invalid SCTs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = true;
  // Add three valid SCTs and one invalid SCT. The three valid SCTs meet the
  // Chrome CT policy.
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       std::string(kTestGoogleLogId, sizeof(kTestGoogleLogId)),
                       net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
      "signature2", base::Time::Now(),
      std::string(kTestNonGoogleLogId1, sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
      "signature3", base::Time::Now(),
      std::string(kTestNonGoogleLogId2, sizeof(kTestNonGoogleLogId2)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions4",
      "signature4", base::Time::Now(),
      std::string(kTestNonGoogleLogId2, sizeof(kTestNonGoogleLogId2)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);

  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "mixed-scts.test", verify_result,
      net::OK);

  SetExtendedReportingEnabled(true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("mixed-scts.test", "/"));
  WaitForRequests(1);
  EXPECT_EQ(1u, requests_seen());

  auto report = GetLastSeenReport();
  EXPECT_EQ(3, report.certificate_report(0).included_sct_size());
}

// Tests that invalid SCTs don't get included when the overall result is
// non-compliant with CT policy. Valid SCTs should still be reported.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CTNonCompliantInvalidSCTsNotReported) {
  // Set up a mocked CertVerifyResult that includes both valid and invalid SCTs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = true;
  // Add one valid SCT and two invalid SCTs. These SCTs will not meet the Chrome
  // CT policy requirements.
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
      "signature1", base::Time::Now(),
      std::string(kTestNonGoogleLogId1, sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_OK, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
      "signature2", base::Time::Now(),
      std::string(kTestNonGoogleLogId1, sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
      "signature3", base::Time::Now(),
      std::string(kTestNonGoogleLogId2, sizeof(kTestNonGoogleLogId2)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);

  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "mixed-scts.test", verify_result,
      net::OK);

  SetExtendedReportingEnabled(true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("mixed-scts.test", "/"));
  WaitForRequests(1);
  EXPECT_EQ(1u, requests_seen());

  auto report = GetLastSeenReport();
  EXPECT_EQ(1, report.certificate_report(0).included_sct_size());
}

IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, NoValidSCTsNoReport) {
  // Set up a mocked CertVerifyResult with only invalid SCTs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert = https_server()->GetCertificate().get();
  verify_result.is_issued_by_known_root = true;
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions1",
      "signature1", base::Time::Now(),
      std::string(kTestNonGoogleLogId1, sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_INVALID_TIMESTAMP, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions2",
      "signature2", base::Time::Now(),
      std::string(kTestNonGoogleLogId1, sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_EMBEDDED, "extensions3",
      "signature3", base::Time::Now(),
      std::string(kTestNonGoogleLogId1, sizeof(kTestNonGoogleLogId1)),
      net::ct::SCT_STATUS_INVALID_SIGNATURE, &verify_result.scts);

  mock_cert_verifier()->AddResultForCertAndHost(
      https_server()->GetCertificate().get(), "invalid-scts.test",
      verify_result, net::OK);

  SetExtendedReportingEnabled(true);
  ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("invalid-scts.test", "/"));
  EXPECT_EQ(0u, requests_seen());
  EXPECT_TRUE(FlushAndCheckZeroReports());
}

class SCTReportingServiceZeroSamplingRateBrowserTest
    : public SCTReportingServiceBrowserTest {
 public:
  SCTReportingServiceZeroSamplingRateBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSCTAuditing,
          {{features::kSCTAuditingSamplingRate.name, "0.0"}}}},
        {});
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }
  ~SCTReportingServiceZeroSamplingRateBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }

  SCTReportingServiceZeroSamplingRateBrowserTest(
      const SCTReportingServiceZeroSamplingRateBrowserTest&) = delete;
  const SCTReportingServiceZeroSamplingRateBrowserTest& operator=(
      const SCTReportingServiceZeroSamplingRateBrowserTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the embedder is not notified when the sampling rate is zero.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceZeroSamplingRateBrowserTest,
                       EmbedderNotNotified) {
  SetExtendedReportingEnabled(true);

  // Visit an HTTPS page.
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));

  // Check that no reports are observed.
  EXPECT_EQ(0u, requests_seen());
}
