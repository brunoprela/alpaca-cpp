#include "alpaca/data/client.hpp"

#include <simdjson/ondemand.h>
#include <simdjson/padded_string_view-inl.h>

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace alpaca::data {

namespace {

void ensure_success(int status, std::string_view context, const std::string &body) {
    if (status >= 200 && status < 300) {
        return;
    }
    std::ostringstream oss;
    oss << context << " failed with status " << status;
    if (!body.empty()) {
        oss << ": " << body;
    }
    throw std::runtime_error(oss.str());
}

// Forward declarations for helpers used below
std::string get_required_string(simdjson::ondemand::object &object, std::string_view key,
                                const char *context);
std::optional<std::string> get_optional_string(simdjson::ondemand::object &object,
                                               std::string_view key);
std::vector<std::string> get_string_array(simdjson::ondemand::object &object, std::string_view key);

std::string join_symbols(const std::vector<std::string> &symbols) {
    std::ostringstream oss;
    bool first = true;
    for (const auto &symbol : symbols) {
        if (symbol.empty()) {
            continue;
        }
        if (!first) {
            oss << ',';
        }
        first = false;
        oss << symbol;
    }
    return oss.str();
}

std::string build_stock_bars_query(const StockBarsRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("StockBarsRequest requires at least one symbol");
    }

    std::ostringstream oss;
    oss << "?symbols=" << join_symbols(request.symbols);
    oss << "&timeframe=" << request.timeframe.serialize();

    auto append = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        oss << '&' << key << '=' << value;
    };

    if (request.start) {
        append("start", *request.start);
    }
    if (request.end) {
        append("end", *request.end);
    }
    if (request.limit) {
        append("limit", std::to_string(*request.limit));
    }
    if (request.currency) {
        append("currency", std::string(common::to_string(*request.currency)));
    }
    if (request.sort) {
        append("sort", std::string(common::to_string(*request.sort)));
    }
    if (request.adjustment) {
        append("adjustment", std::string(to_string(*request.adjustment)));
    }
    if (request.feed) {
        append("feed", std::string(to_string(*request.feed)));
    }
    if (request.page_token) {
        append("page_token", *request.page_token);
    }
    if (request.asof) {
        append("asof", *request.asof);
    }

    return oss.str();
}

std::string build_stock_quotes_query(const StockQuotesRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("StockQuotesRequest requires at least one symbol");
    }

    std::ostringstream oss;
    oss << "?symbols=" << join_symbols(request.symbols);

    auto append = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        oss << '&' << key << '=' << value;
    };

    if (request.start) {
        append("start", *request.start);
    }
    if (request.end) {
        append("end", *request.end);
    }
    if (request.limit) {
        append("limit", std::to_string(*request.limit));
    }
    if (request.currency) {
        append("currency", std::string(common::to_string(*request.currency)));
    }
    if (request.sort) {
        append("sort", std::string(common::to_string(*request.sort)));
    }
    if (request.feed) {
        append("feed", std::string(to_string(*request.feed)));
    }
    if (request.page_token) {
        append("page_token", *request.page_token);
    }
    if (request.asof) {
        append("asof", *request.asof);
    }

    return oss.str();
}

std::string build_stock_latest_quotes_path(const StockLatestQuoteRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("StockLatestQuoteRequest requires at least one symbol");
    }

    std::ostringstream oss;
    oss << "/v2/stocks/quotes/latest?symbols=" << join_symbols(request.symbols);
    if (request.feed) {
        oss << "&feed=" << to_string(*request.feed);
    }
    if (request.currency) {
        oss << "&currency=" << common::to_string(*request.currency);
    }
    return oss.str();
}

std::string build_crypto_base_path(std::string_view resource, CryptoFeed feed) {
    std::ostringstream oss;
    oss << "/v1beta3/crypto/" << to_string(feed) << resource;
    return oss.str();
}

void append_common_param(std::ostringstream &oss, std::string_view key,
                         const std::optional<std::string> &value) {
    if (value && !value->empty()) {
        oss << '&' << key << '=' << *value;
    }
}

void append_common_param(std::ostringstream &oss, std::string_view key,
                         const std::optional<int> &value) {
    if (value) {
        oss << '&' << key << '=' << *value;
    }
}

std::string build_crypto_bars_path(const CryptoBarsRequest &request, CryptoFeed feed) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("CryptoBarsRequest requires at least one symbol");
    }

    auto base = build_crypto_base_path("/bars", feed);
    std::ostringstream oss;
    oss << base << "?symbols=" << join_symbols(request.symbols);
    oss << "&timeframe=" << request.timeframe.serialize();

    append_common_param(oss, "start", request.start);
    append_common_param(oss, "end", request.end);
    append_common_param(oss, "limit", request.limit);
    if (request.currency) {
        append_common_param(
            oss, "currency",
            std::optional<std::string>(std::string(common::to_string(*request.currency))));
    }
    if (request.sort) {
        append_common_param(
            oss, "sort", std::optional<std::string>(std::string(common::to_string(*request.sort))));
    }
    append_common_param(oss, "page_token", request.page_token);

    return oss.str();
}

std::string build_crypto_quotes_path(const CryptoQuoteRequest &request, CryptoFeed feed) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("CryptoQuoteRequest requires at least one symbol");
    }
    auto base = build_crypto_base_path("/quotes", feed);
    std::ostringstream oss;
    oss << base << "?symbols=" << join_symbols(request.symbols);
    append_common_param(oss, "start", request.start);
    append_common_param(oss, "end", request.end);
    append_common_param(oss, "limit", request.limit);
    if (request.currency) {
        append_common_param(
            oss, "currency",
            std::optional<std::string>(std::string(common::to_string(*request.currency))));
    }
    if (request.sort) {
        append_common_param(
            oss, "sort", std::optional<std::string>(std::string(common::to_string(*request.sort))));
    }
    append_common_param(oss, "page_token", request.page_token);
    return oss.str();
}

std::string build_crypto_trades_path(const CryptoTradesRequest &request, CryptoFeed feed) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("CryptoTradesRequest requires at least one symbol");
    }
    auto base = build_crypto_base_path("/trades", feed);
    std::ostringstream oss;
    oss << base << "?symbols=" << join_symbols(request.symbols);
    append_common_param(oss, "start", request.start);
    append_common_param(oss, "end", request.end);
    append_common_param(oss, "limit", request.limit);
    if (request.sort) {
        append_common_param(
            oss, "sort", std::optional<std::string>(std::string(common::to_string(*request.sort))));
    }
    append_common_param(oss, "page_token", request.page_token);
    return oss.str();
}

std::string build_crypto_latest_path(std::string_view endpoint,
                                     const std::vector<std::string> &symbols, CryptoFeed feed) {
    if (symbols.empty()) {
        throw std::invalid_argument("Crypto latest request requires at least one symbol");
    }
    auto base = build_crypto_base_path(endpoint, feed);
    std::ostringstream oss;
    oss << base << "?symbols=" << join_symbols(symbols);
    return oss.str();
}

std::string build_crypto_snapshots_path(const CryptoSnapshotRequest &request, CryptoFeed feed) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("CryptoSnapshotRequest requires at least one symbol");
    }
    auto base = build_crypto_base_path("/snapshots", feed);
    std::ostringstream oss;
    oss << base << "?symbols=" << join_symbols(request.symbols);
    return oss.str();
}

std::string build_option_bars_path(const OptionBarsRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("OptionBarsRequest requires at least one symbol");
    }
    std::ostringstream oss;
    oss << "/v1beta1/options/bars?symbols=" << join_symbols(request.symbols);
    oss << "&timeframe=" << request.timeframe.serialize();
    append_common_param(oss, "start", request.start);
    append_common_param(oss, "end", request.end);
    append_common_param(oss, "limit", request.limit);
    if (request.sort) {
        append_common_param(
            oss, "sort", std::optional<std::string>(std::string(common::to_string(*request.sort))));
    }
    append_common_param(oss, "page_token", request.page_token);
    return oss.str();
}

std::string build_option_trades_path(const OptionTradesRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("OptionTradesRequest requires at least one symbol");
    }
    std::ostringstream oss;
    oss << "/v1beta1/options/trades?symbols=" << join_symbols(request.symbols);
    append_common_param(oss, "start", request.start);
    append_common_param(oss, "end", request.end);
    append_common_param(oss, "limit", request.limit);
    if (request.sort) {
        append_common_param(
            oss, "sort", std::optional<std::string>(std::string(common::to_string(*request.sort))));
    }
    append_common_param(oss, "page_token", request.page_token);
    return oss.str();
}

std::string build_option_latest_path(std::string_view endpoint,
                                     const std::vector<std::string> &symbols,
                                     const std::optional<OptionsFeed> &feed) {
    if (symbols.empty()) {
        throw std::invalid_argument("Option latest request requires at least one symbol");
    }
    std::ostringstream oss;
    oss << "/v1beta1/options" << endpoint << "?symbols=" << join_symbols(symbols);
    if (feed) {
        oss << "&feed=" << to_string(*feed);
    }
    return oss.str();
}

std::string build_option_snapshots_path(const OptionSnapshotRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("OptionSnapshotRequest requires at least one symbol");
    }
    std::ostringstream oss;
    oss << "/v1beta1/options/snapshots?symbols=" << join_symbols(request.symbols);
    if (request.feed) {
        oss << "&feed=" << to_string(*request.feed);
    }
    return oss.str();
}

std::string build_option_chain_path(const OptionChainRequest &request) {
    if (request.underlying_symbol.empty()) {
        throw std::invalid_argument("OptionChainRequest requires an underlying symbol");
    }
    std::ostringstream oss;
    oss << "/v1beta1/options/snapshots/" << request.underlying_symbol << '?';
    bool first_param = true;
    auto append_param = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        if (!first_param) {
            oss << '&';
        }
        first_param = false;
        oss << key << '=' << value;
    };

    if (request.feed) {
        append_param("feed", std::string(to_string(*request.feed)));
    }
    if (request.type) {
        append_param("type", std::string(trading::to_string(*request.type)));
    }
    if (request.strike_price_gte) {
        append_param("strike_price_gte", std::to_string(*request.strike_price_gte));
    }
    if (request.strike_price_lte) {
        append_param("strike_price_lte", std::to_string(*request.strike_price_lte));
    }
    append_param("expiration_date", request.expiration_date.value_or(""));
    append_param("expiration_date_gte", request.expiration_date_gte.value_or(""));
    append_param("expiration_date_lte", request.expiration_date_lte.value_or(""));
    append_param("root_symbol", request.root_symbol.value_or(""));
    append_param("updated_since", request.updated_since.value_or(""));

    std::string url = oss.str();
    if (url.back() == '?') {
        url.pop_back();
    }
    return url;
}

std::string build_most_actives_path(const MostActivesRequest &request) {
    if (request.top <= 0) {
        throw std::invalid_argument("MostActivesRequest requires a positive top value");
    }
    std::ostringstream oss;
    oss << "/v1beta1/screener/stocks/most-actives?top=" << request.top;
    oss << "&by=" << to_string(request.by);
    return oss.str();
}

std::string build_market_movers_path(const MarketMoversRequest &request) {
    if (request.top <= 0) {
        throw std::invalid_argument("MarketMoversRequest requires a positive top value");
    }
    std::ostringstream oss;
    oss << "/v1beta1/screener/" << to_string(request.market_type) << "/movers?top=" << request.top;
    return oss.str();
}

MarketType parse_market_type_value(std::string_view value) {
    if (value == "crypto") {
        return MarketType::Crypto;
    }
    return MarketType::Stocks;
}

std::string build_news_path(const NewsRequest &request) {
    std::ostringstream oss;
    oss << "/v1beta1/news";
    char sep = '?';
    auto add = [&](std::string_view key, const std::string &value) {
        if (value.empty())
            return;
        oss << sep << key << '=' << value;
        sep = '&';
    };
    auto add_opt_str = [&](std::string_view key, const std::optional<std::string> &value) {
        if (value && !value->empty())
            add(std::string(key), *value);
    };
    auto add_opt_int = [&](std::string_view key, const std::optional<int> &value) {
        if (value)
            add(std::string(key), std::to_string(*value));
    };
    auto add_opt_bool = [&](std::string_view key, const std::optional<bool> &value) {
        if (value)
            add(std::string(key), *value ? "true" : "false");
    };
    add_opt_str("start", request.start);
    add_opt_str("end", request.end);
    add_opt_str("sort", request.sort);
    add_opt_str("symbols", request.symbols);
    add_opt_int("limit", request.limit);
    add_opt_bool("include_content", request.include_content);
    add_opt_bool("exclude_contentless", request.exclude_contentless);
    add_opt_str("page_token", request.page_token);
    return oss.str();
}

NewsImageSize parse_news_image_size(std::string_view v) {
    if (v == "thumb")
        return NewsImageSize::THUMB;
    if (v == "small")
        return NewsImageSize::SMALL;
    if (v == "large")
        return NewsImageSize::LARGE;
    return NewsImageSize::SMALL;
}

std::string build_corporate_actions_path(const CorporateActionsRequest &request) {
    std::ostringstream oss;
    oss << "/v1/corporate-actions";
    char sep = '?';
    auto add = [&](std::string_view key, const std::string &value) {
        if (value.empty())
            return;
        oss << sep << key << '=' << value;
        sep = '&';
    };
    auto add_vec = [&](std::string_view key, const std::optional<std::vector<std::string>> &vec) {
        if (!vec || vec->empty())
            return;
        std::ostringstream tmp;
        for (size_t i = 0; i < vec->size(); ++i) {
            if (i)
                tmp << ',';
            tmp << (*vec)[i];
        }
        add(std::string(key), tmp.str());
    };
    auto add_opt_str = [&](std::string_view key, const std::optional<std::string> &value) {
        if (value && !value->empty())
            add(std::string(key), *value);
    };
    auto add_opt_int = [&](std::string_view key, const std::optional<int> &value) {
        if (value)
            add(std::string(key), std::to_string(*value));
    };
    add_vec("symbols", request.symbols);
    add_vec("cusips", request.cusips);
    if (request.types && !request.types->empty()) {
        std::ostringstream tmp;
        for (size_t i = 0; i < request.types->size(); ++i) {
            if (i)
                tmp << ',';
            tmp << to_string((*request.types)[i]);
        }
        add("types", tmp.str());
    }
    add_opt_str("start", request.start);
    add_opt_str("end", request.end);
    add_vec("ids", request.ids);
    add_opt_int("limit", request.limit);
    if (request.sort) {
        add("sort", std::string(common::to_string(*request.sort)));
    }
    return oss.str();
}

CorporateActionsResponse parse_corporate_actions_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    CorporateActionsResponse resp;
    auto root_res = doc.get_object();
    if (root_res.error()) {
        return resp;
    }
    auto root = root_res.value();

    for (auto field : root) {
        auto key_res = field.unescaped_key();
        if (key_res.error())
            continue;
        std::string key(key_res.value());
        if (key == "next_page_token") {
            std::string_view tok{};
            if (!field.value().get_string().get(tok)) {
                resp.next_page_token = std::string(tok);
            }
            continue;
        }
        auto arr_res = field.value().get_array();
        if (arr_res.error()) {
            continue;
        }
        CorporateActionsGroup group;
        group.type = key;
        for (auto entry : arr_res.value()) {
            auto obj_res = entry.get_object();
            if (obj_res.error())
                continue;
            auto obj = obj_res.value();
            CorporateActionItem item;
            for (auto kv : obj) {
                auto k_res = kv.unescaped_key();
                if (k_res.error())
                    continue;
                std::string k(k_res.value());
                std::string v_str;
                std::string_view sview{};
                if (!kv.value().get_string().get(sview)) {
                    v_str = std::string(sview);
                } else {
                    double d{};
                    int64_t i{};
                    bool b{};
                    if (!kv.value().get_double().get(d)) {
                        v_str = std::to_string(d);
                    } else if (!kv.value().get_int64().get(i)) {
                        v_str = std::to_string(i);
                    } else if (!kv.value().get_bool().get(b)) {
                        v_str = b ? "true" : "false";
                    } else {
                        v_str = "";
                    }
                }
                item.fields.emplace_back(std::move(k), std::move(v_str));
            }
            group.items.emplace_back(std::move(item));
        }
        resp.groups.emplace_back(std::move(group));
    }
    return resp;
}
NewsResponse parse_news_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    NewsResponse resp;
    auto root_obj_result = doc.get_object();
    if (root_obj_result.error()) {
        return resp;
    }
    auto root = root_obj_result.value();

    if (auto next_field = root.find_field_unordered("next_page_token"); !next_field.error()) {
        std::string_view token{};
        if (!next_field.get_string().get(token)) {
            resp.next_page_token = std::string(token);
        }
    }
    if (auto news_field = root.find_field_unordered("news"); !news_field.error()) {
        auto arr_res = news_field.get_array();
        if (!arr_res.error()) {
            for (auto item : arr_res.value()) {
                auto obj_res = item.get_object();
                if (obj_res.error())
                    continue;
                auto obj = obj_res.value();
                News n;
                // id can be int; accept as int64 then cast
                auto id_field = obj.find_field_unordered("id");
                if (!id_field.error()) {
                    int64_t idv{};
                    if (!id_field.get_int64().get(idv))
                        n.id = static_cast<int>(idv);
                }
                n.headline = get_required_string(obj, "headline", "news");
                n.source = get_required_string(obj, "source", "news");
                if (auto u = get_optional_string(obj, "url"))
                    n.url = *u;
                n.summary = get_required_string(obj, "summary", "news");
                if (auto sv = get_optional_string(obj, "created_at"))
                    n.created_at = *sv;
                if (auto sv = get_optional_string(obj, "updated_at"))
                    n.updated_at = *sv;
                n.symbols = get_string_array(obj, "symbols");
                if (auto a = get_optional_string(obj, "author"))
                    n.author = *a;
                if (auto c = get_optional_string(obj, "content"))
                    n.content = *c;
                if (auto imgs_field = obj.find_field_unordered("images"); !imgs_field.error()) {
                    auto imgs_arr = imgs_field.get_array();
                    if (!imgs_arr.error()) {
                        for (auto iv : imgs_arr.value()) {
                            auto iobj_res = iv.get_object();
                            if (iobj_res.error())
                                continue;
                            auto iobj = iobj_res.value();
                            NewsImage img;
                            if (auto sz = get_optional_string(iobj, "size")) {
                                img.size = parse_news_image_size(*sz);
                            }
                            img.url = get_required_string(iobj, "url", "news_image");
                            n.images.emplace_back(std::move(img));
                        }
                    }
                }
                resp.news.emplace_back(std::move(n));
            }
        }
    }
    return resp;
}
std::string build_stock_latest_trades_path(const StockLatestTradeRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("StockLatestTradeRequest requires at least one symbol");
    }

    std::ostringstream oss;
    oss << "/v2/stocks/trades/latest?symbols=" << join_symbols(request.symbols);
    if (request.feed) {
        oss << "&feed=" << to_string(*request.feed);
    }
    if (request.currency) {
        oss << "&currency=" << common::to_string(*request.currency);
    }
    return oss.str();
}

std::string build_stock_latest_bars_path(const StockLatestBarRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("StockLatestBarRequest requires at least one symbol");
    }

    std::ostringstream oss;
    oss << "/v2/stocks/bars/latest?symbols=" << join_symbols(request.symbols);
    if (request.feed) {
        oss << "&feed=" << to_string(*request.feed);
    }
    if (request.currency) {
        oss << "&currency=" << common::to_string(*request.currency);
    }
    return oss.str();
}

std::string build_stock_snapshot_path(const StockSnapshotRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("StockSnapshotRequest requires at least one symbol");
    }

    std::ostringstream oss;
    oss << "/v2/stocks/snapshots?symbols=" << join_symbols(request.symbols);
    if (request.feed) {
        oss << "&feed=" << to_string(*request.feed);
    }
    if (request.currency) {
        oss << "&currency=" << common::to_string(*request.currency);
    }
    return oss.str();
}

std::string build_stock_latest_trades_reverse_path(const StockLatestTradeRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("StockLatestTradeRequest requires at least one symbol");
    }

    std::ostringstream oss;
    oss << "/v2/stocks/trades/latest/reverse?symbols=" << join_symbols(request.symbols);
    if (request.feed) {
        oss << "&feed=" << to_string(*request.feed);
    }
    if (request.currency) {
        oss << "&currency=" << common::to_string(*request.currency);
    }
    return oss.str();
}

std::string build_stock_trades_query(const StockTradesRequest &request) {
    if (request.symbols.empty()) {
        throw std::invalid_argument("StockTradesRequest requires at least one symbol");
    }

    std::ostringstream oss;
    oss << "?symbols=" << join_symbols(request.symbols);

    auto append = [&](std::string_view key, const std::string &value) {
        if (value.empty()) {
            return;
        }
        oss << '&' << key << '=' << value;
    };

    if (request.start) {
        append("start", *request.start);
    }
    if (request.end) {
        append("end", *request.end);
    }
    if (request.limit) {
        append("limit", std::to_string(*request.limit));
    }
    if (request.sort) {
        append("sort", std::string(common::to_string(*request.sort)));
    }
    if (request.page_token) {
        append("page_token", *request.page_token);
    }

    return oss.str();
}

double get_required_double(simdjson::ondemand::object &object, std::string_view key,
                           const char *context) {
    auto value = object.find_field_unordered(key);
    if (value.error()) {
        throw std::runtime_error(std::string("Missing required field '") + std::string(key) +
                                 "' in " + context);
    }
    double result{};
    auto error = value.get_double().get(result);
    if (error) {
        throw std::runtime_error(std::string("Failed to parse numeric field '") + std::string(key) +
                                 "'");
    }
    return result;
}

std::optional<double> get_optional_double(simdjson::ondemand::object &object,
                                          std::string_view key) {
    if (auto value = object.find_field_unordered(key); !value.error()) {
        double parsed{};
        if (!value.get_double().get(parsed)) {
            return parsed;
        }
    }
    return std::nullopt;
}

std::string get_required_string(simdjson::ondemand::object &object, std::string_view key,
                                const char *context) {
    auto value = object.find_field_unordered(key);
    if (value.error()) {
        throw std::runtime_error(std::string("Missing required field '") + std::string(key) +
                                 "' in " + context);
    }
    std::string_view view{};
    if (value.get_string().get(view)) {
        throw std::runtime_error(std::string("Failed to parse string field '") + std::string(key) +
                                 "' in " + context);
    }
    return std::string(view);
}

std::string get_timestamp(simdjson::ondemand::object &object) {
    auto value = object.find_field_unordered("t");
    if (value.error()) {
        return {};
    }
    std::string_view view{};
    if (value.get_string().get(view)) {
        return {};
    }
    return std::string(view);
}

std::optional<std::string> get_optional_string(simdjson::ondemand::object &object,
                                               std::string_view key) {
    auto value = object.find_field_unordered(key);
    if (value.error()) {
        return std::nullopt;
    }
    std::string_view view{};
    if (value.get_string().get(view)) {
        return std::nullopt;
    }
    return std::string(view);
}

std::vector<std::string> get_string_array(simdjson::ondemand::object &object,
                                          std::string_view key) {
    std::vector<std::string> items;
    auto value = object.find_field_unordered(key);
    if (value.error()) {
        return items;
    }
    auto array_result = value.get_array();
    if (array_result.error()) {
        return items;
    }
    for (auto element : array_result.value()) {
        std::string_view view{};
        if (!element.get_string().get(view)) {
            items.emplace_back(view);
        }
    }
    return items;
}

std::optional<bool> get_optional_bool(simdjson::ondemand::object &object, std::string_view key) {
    if (auto value = object.find_field_unordered(key); !value.error()) {
        bool parsed{};
        if (!value.get_bool().get(parsed)) {
            return parsed;
        }
    }
    return std::nullopt;
}

std::vector<OrderbookQuote> parse_orderbook_side(simdjson::ondemand::object &object,
                                                 std::string_view key) {
    std::vector<OrderbookQuote> quotes;
    auto value = object.find_field_unordered(key);
    if (value.error()) {
        return quotes;
    }
    auto array_result = value.get_array();
    if (array_result.error()) {
        return quotes;
    }
    for (auto entry : array_result.value()) {
        auto quote_obj_result = entry.get_object();
        if (quote_obj_result.error()) {
            continue;
        }
        auto quote_obj = quote_obj_result.value();
        OrderbookQuote quote;
        quote.price = get_required_double(quote_obj, "p", "orderbook_quote");
        quote.size = get_required_double(quote_obj, "s", "orderbook_quote");
        quotes.emplace_back(std::move(quote));
    }
    return quotes;
}

std::optional<OptionsGreeks> parse_options_greeks(simdjson::ondemand::object &object) {
    OptionsGreeks greeks;
    bool has_value = false;
    if (auto delta = get_optional_double(object, "delta"); delta) {
        greeks.delta = delta;
        has_value = true;
    }
    if (auto gamma = get_optional_double(object, "gamma"); gamma) {
        greeks.gamma = gamma;
        has_value = true;
    }
    if (auto rho = get_optional_double(object, "rho"); rho) {
        greeks.rho = rho;
        has_value = true;
    }
    if (auto theta = get_optional_double(object, "theta"); theta) {
        greeks.theta = theta;
        has_value = true;
    }
    if (auto vega = get_optional_double(object, "vega"); vega) {
        greeks.vega = vega;
        has_value = true;
    }
    if (!has_value) {
        return std::nullopt;
    }
    return greeks;
}

std::optional<Trade> parse_trade_object(simdjson::ondemand::object &object,
                                        const std::string &symbol, const char *context) {
    if (!object.count_fields().value_unsafe()) {
        return std::nullopt;
    }
    Trade trade;
    trade.symbol = symbol;
    trade.timestamp = get_timestamp(object);
    trade.price = get_required_double(object, "p", context);
    trade.size = get_required_double(object, "s", context);
    trade.exchange = get_optional_string(object, "x");
    trade.id = get_optional_string(object, "i");
    trade.conditions = get_string_array(object, "c");
    trade.tape = get_optional_string(object, "z");
    return trade;
}

std::optional<Quote> parse_quote_object(simdjson::ondemand::object &object,
                                        const std::string &symbol, const char *context) {
    if (!object.count_fields().value_unsafe()) {
        return std::nullopt;
    }
    Quote quote;
    quote.symbol = symbol;
    quote.timestamp = get_timestamp(object);
    quote.bid_price = get_required_double(object, "bp", context);
    quote.bid_size = get_required_double(object, "bs", context);
    quote.bid_exchange = get_optional_string(object, "bx");
    quote.ask_price = get_required_double(object, "ap", context);
    quote.ask_size = get_required_double(object, "as", context);
    quote.ask_exchange = get_optional_string(object, "ax");
    quote.conditions = get_string_array(object, "c");
    quote.tape = get_optional_string(object, "z");
    return quote;
}

std::optional<Bar> parse_bar_object(simdjson::ondemand::object &object, const std::string &symbol,
                                    const char *context) {
    if (!object.count_fields().value_unsafe()) {
        return std::nullopt;
    }
    Bar bar;
    bar.symbol = symbol;
    bar.timestamp = get_timestamp(object);
    bar.open = get_required_double(object, "o", context);
    bar.high = get_required_double(object, "h", context);
    bar.low = get_required_double(object, "l", context);
    bar.close = get_required_double(object, "c", context);
    bar.volume = get_required_double(object, "v", context);
    bar.trade_count = get_optional_double(object, "n");
    bar.vwap = get_optional_double(object, "vw");
    return bar;
}

StockBarsResponse parse_stock_bars_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    StockBarsResponse response;

    if (auto bars_field = doc.find_field("bars"); !bars_field.error()) {
        auto bars_object = bars_field.get_object();
        for (auto symbol_field : bars_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto bar_array_result = symbol_field.value().get_array();
            if (bar_array_result.error()) {
                continue;
            }
            auto bar_array = bar_array_result.value();
            for (auto bar_value : bar_array) {
                auto object_result = bar_value.get_object();
                if (object_result.error()) {
                    continue;
                }
                auto object = object_result.value();
                Bar bar;
                bar.symbol = symbol;
                bar.timestamp = get_timestamp(object);
                bar.open = get_required_double(object, "o", "bar");
                bar.high = get_required_double(object, "h", "bar");
                bar.low = get_required_double(object, "l", "bar");
                bar.close = get_required_double(object, "c", "bar");
                bar.volume = get_required_double(object, "v", "bar");
                bar.trade_count = get_optional_double(object, "n");
                bar.vwap = get_optional_double(object, "vw");
                response.bars.emplace_back(std::move(bar));
            }
        }
    }

    if (auto next_token = doc.find_field("next_page_token"); !next_token.error()) {
        std::string_view token_view{};
        if (!next_token.get_string().get(token_view)) {
            response.next_page_token = std::string(token_view);
        }
    }

    return response;
}

StockQuotesResponse parse_stock_quotes_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    StockQuotesResponse response;

    if (auto quotes_field = doc.find_field("quotes"); !quotes_field.error()) {
        auto quotes_object = quotes_field.get_object();
        for (auto symbol_field : quotes_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto quote_array_result = symbol_field.value().get_array();
            if (quote_array_result.error()) {
                continue;
            }
            auto quote_array = quote_array_result.value();
            for (auto quote_value : quote_array) {
                auto object_result = quote_value.get_object();
                if (object_result.error()) {
                    continue;
                }
                auto object = object_result.value();
                Quote quote;
                quote.symbol = symbol;
                quote.timestamp = get_timestamp(object);
                quote.bid_price = get_required_double(object, "bp", "quote");
                quote.bid_size = get_required_double(object, "bs", "quote");
                quote.bid_exchange = get_optional_string(object, "bx");
                quote.ask_price = get_required_double(object, "ap", "quote");
                quote.ask_size = get_required_double(object, "as", "quote");
                quote.ask_exchange = get_optional_string(object, "ax");
                quote.conditions = get_string_array(object, "c");
                quote.tape = get_optional_string(object, "z");
                response.quotes.emplace_back(std::move(quote));
            }
        }
    }

    if (auto next_token = doc.find_field("next_page_token"); !next_token.error()) {
        std::string_view token_view{};
        if (!next_token.get_string().get(token_view)) {
            response.next_page_token = std::string(token_view);
        }
    }

    return response;
}

StockLatestQuoteResponse parse_stock_latest_quotes_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    StockLatestQuoteResponse response;

    if (auto quotes_field = doc.find_field("quotes"); !quotes_field.error()) {
        auto quotes_object = quotes_field.get_object();
        for (auto symbol_field : quotes_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto quote_obj_result = symbol_field.value().get_object();
            if (quote_obj_result.error()) {
                continue;
            }
            auto object = quote_obj_result.value();
            Quote quote;
            quote.symbol = symbol;
            quote.timestamp = get_timestamp(object);
            quote.bid_price = get_required_double(object, "bp", "latest_quote");
            quote.bid_size = get_required_double(object, "bs", "latest_quote");
            quote.bid_exchange = get_optional_string(object, "bx");
            quote.ask_price = get_required_double(object, "ap", "latest_quote");
            quote.ask_size = get_required_double(object, "as", "latest_quote");
            quote.ask_exchange = get_optional_string(object, "ax");
            quote.conditions = get_string_array(object, "c");
            quote.tape = get_optional_string(object, "z");
            response.quotes.emplace_back(std::move(quote));
        }
    }

    return response;
}

StockTradesResponse parse_stock_trades_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    StockTradesResponse response;

    if (auto trades_field = doc.find_field("trades"); !trades_field.error()) {
        auto trades_object = trades_field.get_object();
        for (auto symbol_field : trades_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto trade_array_result = symbol_field.value().get_array();
            if (trade_array_result.error()) {
                continue;
            }
            auto trade_array = trade_array_result.value();
            for (auto trade_value : trade_array) {
                auto object_result = trade_value.get_object();
                if (object_result.error()) {
                    continue;
                }
                auto object = object_result.value();
                Trade trade;
                trade.symbol = symbol;
                trade.timestamp = get_timestamp(object);
                trade.price = get_required_double(object, "p", "trade");
                trade.size = get_required_double(object, "s", "trade");
                trade.exchange = get_optional_string(object, "x");
                trade.id = get_optional_string(object, "i");
                trade.conditions = get_string_array(object, "c");
                trade.tape = get_optional_string(object, "z");
                response.trades.emplace_back(std::move(trade));
            }
        }
    }

    if (auto next_token = doc.find_field("next_page_token"); !next_token.error()) {
        std::string_view token_view{};
        if (!next_token.get_string().get(token_view)) {
            response.next_page_token = std::string(token_view);
        }
    }

    return response;
}

StockLatestTradeResponse parse_stock_latest_trades_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    StockLatestTradeResponse response;

    if (auto trades_field = doc.find_field("trades"); !trades_field.error()) {
        auto trades_object = trades_field.get_object();
        for (auto symbol_field : trades_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto trade_obj_result = symbol_field.value().get_object();
            if (trade_obj_result.error()) {
                continue;
            }
            auto object = trade_obj_result.value();
            Trade trade;
            trade.symbol = symbol;
            trade.timestamp = get_timestamp(object);
            trade.price = get_required_double(object, "p", "latest_trade");
            trade.size = get_required_double(object, "s", "latest_trade");
            trade.exchange = get_optional_string(object, "x");
            trade.id = get_optional_string(object, "i");
            trade.conditions = get_string_array(object, "c");
            trade.tape = get_optional_string(object, "z");
            response.trades.emplace_back(std::move(trade));
        }
    }

    return response;
}

StockLatestBarResponse parse_stock_latest_bars_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    StockLatestBarResponse response;

    if (auto bars_field = doc.find_field("bars"); !bars_field.error()) {
        auto bars_object = bars_field.get_object();
        for (auto symbol_field : bars_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto bar_obj_result = symbol_field.value().get_object();
            if (bar_obj_result.error()) {
                continue;
            }
            auto object = bar_obj_result.value();
            Bar bar;
            bar.symbol = symbol;
            bar.timestamp = get_timestamp(object);
            bar.open = get_required_double(object, "o", "latest_bar");
            bar.high = get_required_double(object, "h", "latest_bar");
            bar.low = get_required_double(object, "l", "latest_bar");
            bar.close = get_required_double(object, "c", "latest_bar");
            bar.volume = get_required_double(object, "v", "latest_bar");
            bar.trade_count = get_optional_double(object, "n");
            bar.vwap = get_optional_double(object, "vw");
            response.bars.emplace_back(std::move(bar));
        }
    }

    return response;
}

StockSnapshotResponse parse_stock_snapshot_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    StockSnapshotResponse response;

    if (auto snapshots_field = doc.find_field("snapshots"); !snapshots_field.error()) {
        auto snap_object = snapshots_field.get_object();
        for (auto symbol_field : snap_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto snapshot_obj_result = symbol_field.value().get_object();
            if (snapshot_obj_result.error()) {
                continue;
            }
            auto snapshot_obj = snapshot_obj_result.value();
            Snapshot snapshot;
            snapshot.symbol = symbol;

            if (auto latest_trade_field = snapshot_obj.find_field_unordered("latestTrade");
                !latest_trade_field.error()) {
                if (auto trade_obj = latest_trade_field.get_object(); !trade_obj.error()) {
                    snapshot.latest_trade =
                        parse_trade_object(trade_obj.value(), symbol, "snapshot_latest_trade");
                }
            }

            if (auto latest_quote_field = snapshot_obj.find_field_unordered("latestQuote");
                !latest_quote_field.error()) {
                if (auto quote_obj = latest_quote_field.get_object(); !quote_obj.error()) {
                    snapshot.latest_quote =
                        parse_quote_object(quote_obj.value(), symbol, "snapshot_latest_quote");
                }
            }

            auto parse_bar_helper = [&](std::string_view field,
                                        std::optional<Bar> Snapshot::*target, const char *context) {
                if (auto bar_field = snapshot_obj.find_field_unordered(field); !bar_field.error()) {
                    if (auto bar_obj = bar_field.get_object(); !bar_obj.error()) {
                        snapshot.*target = parse_bar_object(bar_obj.value(), symbol, context);
                    }
                }
            };

            parse_bar_helper("minuteBar", &Snapshot::minute_bar, "snapshot_minute_bar");
            parse_bar_helper("dailyBar", &Snapshot::daily_bar, "snapshot_daily_bar");
            parse_bar_helper("prevDailyBar", &Snapshot::prev_daily_bar, "snapshot_prev_daily_bar");

            response.snapshots.emplace_back(std::move(snapshot));
        }
    }

    return response;
}

CryptoLatestOrderbookResponse parse_crypto_latest_orderbooks_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    CryptoLatestOrderbookResponse response;

    if (auto books_field = doc.find_field("orderbooks"); !books_field.error()) {
        auto books_object = books_field.get_object();
        for (auto symbol_field : books_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto book_obj_result = symbol_field.value().get_object();
            if (book_obj_result.error()) {
                continue;
            }
            auto book_obj = book_obj_result.value();
            Orderbook orderbook;
            orderbook.symbol = symbol;
            orderbook.timestamp = get_timestamp(book_obj);
            orderbook.bids = parse_orderbook_side(book_obj, "b");
            orderbook.asks = parse_orderbook_side(book_obj, "a");
            orderbook.reset = get_optional_bool(book_obj, "r").value_or(false);
            response.orderbooks.emplace_back(std::move(orderbook));
        }
    }

    return response;
}

OptionsSnapshotResponse parse_options_snapshot_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    OptionsSnapshotResponse response;

    if (auto snapshots_field = doc.find_field("snapshots"); !snapshots_field.error()) {
        auto snap_object = snapshots_field.get_object();
        for (auto symbol_field : snap_object) {
            auto symbol_key = symbol_field.unescaped_key();
            if (symbol_key.error()) {
                continue;
            }
            std::string symbol(symbol_key.value());
            auto snapshot_obj_result = symbol_field.value().get_object();
            if (snapshot_obj_result.error()) {
                continue;
            }
            auto snapshot_obj = snapshot_obj_result.value();
            OptionsSnapshot snapshot;
            snapshot.symbol = symbol;

            if (auto latest_trade_field = snapshot_obj.find_field_unordered("latestTrade");
                !latest_trade_field.error()) {
                if (auto trade_obj = latest_trade_field.get_object(); !trade_obj.error()) {
                    snapshot.latest_trade =
                        parse_trade_object(trade_obj.value(), symbol, "option_snapshot_trade");
                }
            }

            if (auto latest_quote_field = snapshot_obj.find_field_unordered("latestQuote");
                !latest_quote_field.error()) {
                if (auto quote_obj = latest_quote_field.get_object(); !quote_obj.error()) {
                    snapshot.latest_quote =
                        parse_quote_object(quote_obj.value(), symbol, "option_snapshot_quote");
                }
            }

            snapshot.implied_volatility = get_optional_double(snapshot_obj, "impliedVolatility");

            if (auto greeks_field = snapshot_obj.find_field_unordered("greeks");
                !greeks_field.error()) {
                if (auto greeks_obj = greeks_field.get_object(); !greeks_obj.error()) {
                    snapshot.greeks = parse_options_greeks(greeks_obj.value());
                }
            }

            response.snapshots.emplace_back(std::move(snapshot));
        }
    }

    return response;
}

MostActives parse_most_actives_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    MostActives response;
    auto root_result = doc.get_object();
    if (root_result.error()) {
        return response;
    }
    auto root = root_result.value();

    if (auto actives_field = root.find_field_unordered("most_actives"); !actives_field.error()) {
        auto array_result = actives_field.get_array();
        if (!array_result.error()) {
            for (auto entry : array_result.value()) {
                auto object_result = entry.get_object();
                if (object_result.error()) {
                    continue;
                }
                auto object = object_result.value();
                ActiveStock stock;
                stock.symbol = get_required_string(object, "symbol", "most_actives");
                stock.volume = get_required_double(object, "volume", "most_actives");
                stock.trade_count = get_required_double(object, "trade_count", "most_actives");
                response.most_actives.emplace_back(std::move(stock));
            }
        }
    }

    if (auto last_updated_field = root.find_field_unordered("last_updated");
        !last_updated_field.error()) {
        std::string_view value{};
        if (!last_updated_field.get_string().get(value)) {
            response.last_updated = std::string(value);
        }
    }

    return response;
}

Movers parse_market_movers_response(std::string_view payload) {
    simdjson::ondemand::parser parser;
    std::string storage(payload);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), payload.size(), storage.size());

    Movers response;
    auto root_result = doc.get_object();
    if (root_result.error()) {
        return response;
    }
    auto root = root_result.value();

    auto parse_movers_array = [&](std::string_view field, std::vector<Mover> &target,
                                  const char *context) {
        if (auto movers_field = root.find_field_unordered(field); !movers_field.error()) {
            auto array_result = movers_field.get_array();
            if (array_result.error()) {
                return;
            }
            for (auto entry : array_result.value()) {
                auto object_result = entry.get_object();
                if (object_result.error()) {
                    continue;
                }
                auto object = object_result.value();
                Mover mover;
                mover.symbol = get_required_string(object, "symbol", context);
                mover.percent_change = get_required_double(object, "percent_change", context);
                mover.change = get_required_double(object, "change", context);
                mover.price = get_required_double(object, "price", context);
                target.emplace_back(std::move(mover));
            }
        }
    };

    parse_movers_array("gainers", response.gainers, "market_mover_gainer");
    parse_movers_array("losers", response.losers, "market_mover_loser");

    if (auto market_field = root.find_field_unordered("market_type"); !market_field.error()) {
        std::string_view view{};
        if (!market_field.get_string().get(view)) {
            response.market_type = parse_market_type_value(view);
        }
    }

    if (auto last_updated_field = root.find_field_unordered("last_updated");
        !last_updated_field.error()) {
        std::string_view value{};
        if (!last_updated_field.get_string().get(value)) {
            response.last_updated = std::string(value);
        }
    }

    return response;
}

} // namespace

DataClient::DataClient(core::ClientConfig config, std::shared_ptr<core::IHttpTransport> transport)
    : config_(std::move(config)), transport_(std::move(transport)) {
    if (!transport_) {
        throw std::invalid_argument("DataClient requires a valid IHttpTransport");
    }
}

StockBarsResponse DataClient::get_stock_bars(const StockBarsRequest &request) const {
    auto query = build_stock_bars_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/stocks/bars" + query);
    ensure_success(response.status_code, "get_stock_bars", response.body);
    return parse_stock_bars_response(response.body);
}

StockQuotesResponse DataClient::get_stock_quotes(const StockQuotesRequest &request) const {
    auto query = build_stock_quotes_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/stocks/quotes" + query);
    ensure_success(response.status_code, "get_stock_quotes", response.body);
    return parse_stock_quotes_response(response.body);
}

StockLatestQuoteResponse
DataClient::get_stock_latest_quotes(const StockLatestQuoteRequest &request) const {
    auto path = build_stock_latest_quotes_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_stock_latest_quotes", response.body);
    return parse_stock_latest_quotes_response(response.body);
}

StockTradesResponse DataClient::get_stock_trades(const StockTradesRequest &request) const {
    auto query = build_stock_trades_query(request);
    auto response = send_request(core::HttpMethod::Get, "/v2/stocks/trades" + query);
    ensure_success(response.status_code, "get_stock_trades", response.body);
    return parse_stock_trades_response(response.body);
}

StockLatestTradeResponse
DataClient::get_stock_latest_trades(const StockLatestTradeRequest &request) const {
    auto path = build_stock_latest_trades_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_stock_latest_trades", response.body);
    return parse_stock_latest_trades_response(response.body);
}

StockLatestBarResponse
DataClient::get_stock_latest_bars(const StockLatestBarRequest &request) const {
    auto path = build_stock_latest_bars_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_stock_latest_bars", response.body);
    return parse_stock_latest_bars_response(response.body);
}

StockSnapshotResponse DataClient::get_stock_snapshots(const StockSnapshotRequest &request) const {
    auto path = build_stock_snapshot_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_stock_snapshots", response.body);
    return parse_stock_snapshot_response(response.body);
}

StockLatestTradeResponse
DataClient::get_stock_latest_trades_reverse(const StockLatestTradeRequest &request) const {
    auto path = build_stock_latest_trades_reverse_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_stock_latest_trades_reverse", response.body);
    return parse_stock_latest_trades_response(response.body);
}

StockBarsResponse DataClient::get_crypto_bars(const CryptoBarsRequest &request,
                                              CryptoFeed feed) const {
    auto path = build_crypto_bars_path(request, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_bars", response.body);
    return parse_stock_bars_response(response.body);
}

StockQuotesResponse DataClient::get_crypto_quotes(const CryptoQuoteRequest &request,
                                                  CryptoFeed feed) const {
    auto path = build_crypto_quotes_path(request, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_quotes", response.body);
    return parse_stock_quotes_response(response.body);
}

StockTradesResponse DataClient::get_crypto_trades(const CryptoTradesRequest &request,
                                                  CryptoFeed feed) const {
    auto path = build_crypto_trades_path(request, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_trades", response.body);
    return parse_stock_trades_response(response.body);
}

StockLatestTradeResponse
DataClient::get_crypto_latest_trades(const CryptoLatestTradeRequest &request,
                                     CryptoFeed feed) const {
    auto path = build_crypto_latest_path("/latest/trades", request.symbols, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_latest_trades", response.body);
    return parse_stock_latest_trades_response(response.body);
}

StockLatestTradeResponse
DataClient::get_crypto_latest_trades_reverse(const CryptoLatestTradeRequest &request,
                                             CryptoFeed feed) const {
    auto path = build_crypto_latest_path("/latest/trades/reverse", request.symbols, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_latest_trades_reverse", response.body);
    return parse_stock_latest_trades_response(response.body);
}

StockLatestQuoteResponse
DataClient::get_crypto_latest_quotes(const CryptoLatestQuoteRequest &request,
                                     CryptoFeed feed) const {
    auto path = build_crypto_latest_path("/latest/quotes", request.symbols, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_latest_quotes", response.body);
    return parse_stock_latest_quotes_response(response.body);
}

StockLatestBarResponse DataClient::get_crypto_latest_bars(const CryptoLatestBarRequest &request,
                                                          CryptoFeed feed) const {
    auto path = build_crypto_latest_path("/latest/bars", request.symbols, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_latest_bars", response.body);
    return parse_stock_latest_bars_response(response.body);
}

CryptoLatestOrderbookResponse
DataClient::get_crypto_latest_orderbooks(const CryptoLatestOrderbookRequest &request,
                                         CryptoFeed feed) const {
    auto path = build_crypto_latest_path("/latest/orderbooks", request.symbols, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_latest_orderbooks", response.body);
    return parse_crypto_latest_orderbooks_response(response.body);
}

StockSnapshotResponse DataClient::get_crypto_snapshots(const CryptoSnapshotRequest &request,
                                                       CryptoFeed feed) const {
    auto path = build_crypto_snapshots_path(request, feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_crypto_snapshots", response.body);
    return parse_stock_snapshot_response(response.body);
}

StockBarsResponse DataClient::get_option_bars(const OptionBarsRequest &request) const {
    auto path = build_option_bars_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_option_bars", response.body);
    return parse_stock_bars_response(response.body);
}

StockTradesResponse DataClient::get_option_trades(const OptionTradesRequest &request) const {
    auto path = build_option_trades_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_option_trades", response.body);
    return parse_stock_trades_response(response.body);
}

StockLatestTradeResponse
DataClient::get_option_latest_trades(const OptionLatestTradeRequest &request) const {
    auto path = build_option_latest_path("/trades/latest", request.symbols, request.feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_option_latest_trades", response.body);
    return parse_stock_latest_trades_response(response.body);
}

StockLatestQuoteResponse
DataClient::get_option_latest_quotes(const OptionLatestQuoteRequest &request) const {
    auto path = build_option_latest_path("/quotes/latest", request.symbols, request.feed);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_option_latest_quotes", response.body);
    return parse_stock_latest_quotes_response(response.body);
}

OptionsSnapshotResponse
DataClient::get_option_snapshots(const OptionSnapshotRequest &request) const {
    auto path = build_option_snapshots_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_option_snapshots", response.body);
    return parse_options_snapshot_response(response.body);
}

OptionsSnapshotResponse DataClient::get_option_chain(const OptionChainRequest &request) const {
    auto path = build_option_chain_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_option_chain", response.body);
    return parse_options_snapshot_response(response.body);
}

std::unordered_map<std::string, std::string> DataClient::get_option_exchange_codes() const {
    auto response = send_request(core::HttpMethod::Get, "/v1beta1/options/meta/exchanges");
    ensure_success(response.status_code, "get_option_exchange_codes", response.body);

    std::unordered_map<std::string, std::string> result;
    simdjson::ondemand::parser parser;
    std::string storage(response.body);
    storage.append(simdjson::SIMDJSON_PADDING, '\0');
    auto doc = parser.iterate(storage.data(), response.body.size(), storage.size());
    auto obj_res = doc.get_object();
    if (obj_res.error()) {
        return result;
    }
    for (auto kv : obj_res.value()) {
        auto k_res = kv.unescaped_key();
        if (k_res.error())
            continue;
        std::string key(k_res.value());
        std::string_view val{};
        if (!kv.value().get_string().get(val)) {
            result.emplace(std::move(key), std::string(val));
        }
    }
    return result;
}

std::string DataClient::get_option_exchange_codes_raw() const {
    auto response = send_request(core::HttpMethod::Get, "/v1beta1/options/meta/exchanges");
    ensure_success(response.status_code, "get_option_exchange_codes_raw", response.body);
    return response.body;
}

MostActives DataClient::get_most_actives(const MostActivesRequest &request) const {
    auto path = build_most_actives_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_most_actives", response.body);
    return parse_most_actives_response(response.body);
}

std::string DataClient::get_most_actives_raw(const MostActivesRequest &request) const {
    auto path = build_most_actives_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_most_actives_raw", response.body);
    return response.body;
}

Movers DataClient::get_market_movers(const MarketMoversRequest &request) const {
    auto path = build_market_movers_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_market_movers", response.body);
    return parse_market_movers_response(response.body);
}

std::string DataClient::get_market_movers_raw(const MarketMoversRequest &request) const {
    auto path = build_market_movers_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_market_movers_raw", response.body);
    return response.body;
}

NewsResponse DataClient::get_news(const NewsRequest &request) const {
    auto path = build_news_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_news", response.body);
    return parse_news_response(response.body);
}

std::string DataClient::get_news_raw(const NewsRequest &request) const {
    auto path = build_news_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_news_raw", response.body);
    return response.body;
}

CorporateActionsResponse
DataClient::get_corporate_actions(const CorporateActionsRequest &request) const {
    auto path = build_corporate_actions_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_corporate_actions", response.body);
    return parse_corporate_actions_response(response.body);
}

std::string DataClient::get_corporate_actions_raw(const CorporateActionsRequest &request) const {
    auto path = build_corporate_actions_path(request);
    auto response = send_request(core::HttpMethod::Get, path);
    ensure_success(response.status_code, "get_corporate_actions_raw", response.body);
    return response.body;
}
core::HttpResponse DataClient::send_request(core::HttpMethod method, std::string_view path) const {
    core::HttpRequest request;
    request.method = method;
    request.url = config_.environment().market_data_url + std::string(path);
    request.headers["Accept"] = "application/json";

    if (auto token = config_.oauth_token()) {
        request.headers["Authorization"] = "Bearer " + *token;
    } else {
        if (!config_.api_key().empty()) {
            request.headers["APCA-API-KEY-ID"] = std::string(config_.api_key());
        }
        if (!config_.api_secret().empty()) {
            request.headers["APCA-API-SECRET-KEY"] = std::string(config_.api_secret());
        }
    }

    return transport_->send(request);
}

} // namespace alpaca::data
