// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "json_get_handler.h"
#include <mutex>
#include <memory>
#include <vector>

namespace vespalib {

/**
 * A repository of json get handlers that is also a json get
 * handler. The get function will dispatch the request to the
 * appropriate get handler in the repository. The bind function will
 * register a handler and return a token that can later be deleted to
 * unbind the handler. Each handler is registered with a path
 * prefix. If the requested path matches multiple handlers, the one
 * with the longest prefix will be selected. If multiple handlers are
 * tied for longest prefix, the most recently added handler will be
 * selected.
 **/
class JsonHandlerRepo : public JsonGetHandler
{
public:
    struct Token {
        using UP = std::unique_ptr<Token>;
        virtual ~Token() = default;
    };

private:
    struct Hook {
        size_t seq;
        vespalib::string path_prefix;
        const JsonGetHandler *handler;
        Hook(size_t seq_in,
             std::string_view prefix_in,
             const JsonGetHandler &handler_in) noexcept
            : seq(seq_in), path_prefix(prefix_in), handler(&handler_in) {}
        bool operator <(const Hook &rhs) const {
            if (path_prefix.size() == rhs.path_prefix.size()) {
                return (seq > rhs.seq);
            }
            return (path_prefix.size() > rhs.path_prefix.size());
        }
    };

    struct Resource {
        size_t seq;
        vespalib::string path;
        Resource(size_t seq_in, std::string_view path_in) noexcept
            : seq(seq_in), path(path_in) {}
    };

    struct State {
        using SP = std::shared_ptr<State>;
        std::mutex lock;
        size_t seq;
        std::vector<Hook> hooks;
        std::vector<Resource> root_resources;
        State() noexcept : lock(), seq(0), hooks(), root_resources() {}
        size_t bind(std::string_view path_prefix,
                    const JsonGetHandler &get_handler);
        size_t add_root_resource(std::string_view path);
        void unbind(size_t my_seq);
    };

    struct Unbinder : Token {
        State::SP state;
        size_t my_seq;
        Unbinder(State::SP state_in, size_t my_seq_in) noexcept
            : state(std::move(state_in)), my_seq(my_seq_in) {}
        ~Unbinder() override {
            state->unbind(my_seq);
        }
    };

    std::shared_ptr<State> _state;

public:
    JsonHandlerRepo();
    ~JsonHandlerRepo() override;
    Token::UP bind(std::string_view path_prefix, const JsonGetHandler &get_handler);
    Token::UP add_root_resource(std::string_view path);
    [[nodiscard]] std::vector<vespalib::string> get_root_resources() const;
    [[nodiscard]] Response get(const vespalib::string &host, const vespalib::string &path,
                               const std::map<vespalib::string,vespalib::string> &params,
                               const net::ConnectionAuthContext &auth_ctx) const override;
};

} // namespace vespalib
