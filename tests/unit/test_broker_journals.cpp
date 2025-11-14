#include "alpaca/broker/client.hpp"
#include "alpaca/core/mock_http_transport.hpp"

#include <cassert>
#include <iostream>

using namespace alpaca;

int main() {
    auto config = core::ClientConfig::WithPaperKeys("key", "secret");
    auto transport = std::make_shared<core::MockHttpTransport>();
    broker::BrokerClient client(config, transport);

    transport->enqueue_response(
        {200,
         {},
         R"({"id":"jnl_1","to_account":"acc_2","from_account":"acc_1","entry_type":"JNLC","status":"queued","net_amount":"250.5"})"});

    broker::CreateJournalRequest journal_request{
        .from_account = "acc_1",
        .to_account = "acc_2",
        .entry_type = broker::JournalEntryType::Cash,
        .amount = 250.5,
        .description = "Funding",
    };

    const auto journal = client.create_journal(journal_request);
    assert(journal.id == "jnl_1");
    assert(journal.net_amount && *journal.net_amount == "250.5");

    auto list_transport = std::make_shared<core::MockHttpTransport>();
    broker::BrokerClient list_client(config, list_transport);
    list_transport->enqueue_response(
        {200,
         {},
         R"([{"id":"jnl_2","to_account":"acc_target","from_account":"acc_source","entry_type":"JNLC","status":"executed"}])"});

    broker::GetJournalsRequest filter{
        .status = broker::JournalStatus::Executed,
        .entry_type = broker::JournalEntryType::Cash,
        .to_account = std::string("acc_target"),
    };

    const auto journals = list_client.list_journals(filter);
    assert(journals.size() == 1);
    assert(journals.front().status == broker::JournalStatus::Executed);

    const auto& journal_requests = list_transport->requests();
    if (journal_requests.empty()) {
        std::cerr << "Expected journal list request\n";
        return 1;
    }
    const auto& request = journal_requests.front();
    if (request.url.find("status=executed") == std::string::npos ||
        request.url.find("entry_type=JNLC") == std::string::npos ||
        request.url.find("to_account=acc_target") == std::string::npos) {
        std::cerr << "Unexpected journal query: " << request.url << '\n';
        return 1;
    }
    assert(request.url.find("status=executed") != std::string::npos);
    assert(request.url.find("entry_type=JNLC") != std::string::npos);
    assert(request.url.find("to_account=acc_target") != std::string::npos);

    std::cout << "Broker journal tests passed\n";
    return 0;
}


