#include <capnp/ez-rpc.h>
#include <capnp/compat/http-over-capnp.capnp.h>
#include <kj/compat/http.h>
#include <kj/async-io.h>
#include <kj/memory.h>
#include <capnp/compat/http-over-capnp.h>

int main(int, const char**) {
	capnp::EzRpcClient client("127.0.0.1", 5923);

	capnp::HttpService::Client cap = client.getMain<capnp::HttpService>();

	kj::HttpHeaderTable::Builder headerTableBuilder;
	capnp::ByteStreamFactory streamFactory;
	capnp::HttpOverCapnpFactory hocFactory(streamFactory, headerTableBuilder);
	auto service = hocFactory.capnpToKj(cap);

	auto& timer = client.getIoProvider().getTimer();
	auto headerTable = kj::HttpHeaderTable::Builder().build();
	kj::HttpServer server(timer, *headerTable, *service);
	auto listener = client.getIoProvider().getNetwork().parseAddress("127.0.0.1", 8080).then(
			[](kj::Own<kj::NetworkAddress> addr)
			{
			return addr->listen();
			})
	.wait(client.getWaitScope());
	server.listenHttp(*listener).wait(client.getWaitScope());

	return 0;
}

