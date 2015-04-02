#include "cocaine/detail/service/node.v2/actor.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/detail/chamber.hpp"
#include "cocaine/detail/engine.hpp"

using namespace cocaine;

using namespace blackhole;

class unix_actor_t::accept_action_t:
    public std::enable_shared_from_this<accept_action_t>
{
    unix_actor_t *const   parent;
    protocol_type::socket socket;

public:
    accept_action_t(unix_actor_t *const parent):
        parent(parent),
        socket(*parent->m_asio)
    {}

    void
    operator()() {
        parent->m_acceptor->async_accept(socket, std::bind(&accept_action_t::finalize,
            shared_from_this(),
            std::placeholders::_1
        ));
    }

private:
    void
    finalize(const std::error_code& ec) {
        // Prepare the internal socket object for consequential operations by moving its contents to a
        // heap-allocated object, which in turn might be attached to an engine.
        auto ptr = std::make_shared<protocol_type::socket>(std::move(socket));

        switch(ec.value()) {
          case 0:
            COCAINE_LOG_DEBUG(parent->m_log, "accepted remote client on fd %d", ptr->native_handle());
            parent->m_context.engine().attach(ptr, parent->m_prototype);
            break;

          case asio::error::operation_aborted:
            return;

          default:
            COCAINE_LOG_ERROR(parent->m_log, "dropped remote client: [%d] %s", ec.value(), ec.message());
        }

        // TODO: Find out if it's always a good idea to continue accepting connections no matter what.
        // For example, destroying a socket from outside this thread will trigger weird stuff on Linux.
        operator()();
    }
};

unix_actor_t::unix_actor_t(cocaine::context_t& context,
                           asio::local::stream_protocol::endpoint endpoint,
                           const std::shared_ptr<asio::io_service>& asio,
                           std::unique_ptr<cocaine::io::basic_dispatch_t> prototype) :
    m_context(context),
    endpoint(std::move(endpoint)),
    m_log(context.log("core::io", { attribute::make("app", prototype->name()) })),
    m_asio(asio),
    m_prototype(std::move(prototype))
{}

unix_actor_t::~unix_actor_t() {}

void
unix_actor_t::run() {
    BOOST_ASSERT(!m_chamber);

    try {
        m_acceptor = std::make_unique<protocol_type::acceptor>(*m_asio, this->endpoint);
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to bind local endpoint for service: [%d] %s",
            e.code().value(), e.code().message());
        throw;
    }

    std::error_code ec;
    const auto endpoint = m_acceptor->local_endpoint(ec);

    COCAINE_LOG_INFO(m_log, "exposing service on local endpoint %s", endpoint);

    m_asio->post(std::bind(&accept_action_t::operator(),
        std::make_shared<accept_action_t>(this)
    ));

    // The post() above won't be executed until this thread is started.
    m_chamber = std::make_unique<io::chamber_t>(m_prototype->name(), m_asio);
}

void
unix_actor_t::terminate() {
    BOOST_ASSERT(m_chamber);

    // Do not wait for the service to finish all its stuff (like timers, etc). Graceful termination
    // happens only in engine chambers, because that's where client connections are being handled.
    m_asio->stop();

    std::error_code ec;
    const auto endpoint = m_acceptor->local_endpoint(ec);

    COCAINE_LOG_INFO(m_log, "removing service from local endpoint %s", endpoint);

    // Does not block, unlike the one in execution_unit_t's destructors.
    m_chamber  = nullptr;
    m_acceptor = nullptr;

    // Be ready to restart the actor.
    m_asio->reset();
}
