#include <kj/compat/http.h>
#include <kj/async-io.h>
#include <kj/memory.h>
#include <capnp/compat/http-over-capnp.h>
#include <capnp/rpc.h>
#include <capnp/rpc-twoparty.h>

#include "combinationstream.h"

char msg[] = "hello world\n";

class HelloService : public kj::HttpService {
	virtual kj::Promise<void> request(kj::HttpMethod, kj::StringPtr, const kj::HttpHeaders&, kj::AsyncInputStream&, kj::HttpService::Response&);
};

kj::Promise<void> HelloService::request(kj::HttpMethod, kj::StringPtr, const kj::HttpHeaders&, kj::AsyncInputStream&, kj::HttpService::Response& response) {
	auto headerTableBuilder = kj::HttpHeaderTable::Builder();
	auto contentType = headerTableBuilder.add("Content-type");
	auto headerTable = headerTableBuilder.build();
	kj::HttpHeaders headers(*headerTable);
	headers.set(contentType, "text/plain");
	auto os = response.send(200, "OK", headers);
	return os->write(msg, sizeof(msg)-1);
}

int main(int,char**) {
	kj::Own<HelloService> service = kj::heap<HelloService>();
	kj::HttpHeaderTable::Builder headerTableBuilder;
	capnp::ByteStreamFactory streamFactory;
	capnp::HttpOverCapnpFactory hocFactory(streamFactory, headerTableBuilder);
	auto serviceClient = hocFactory.kjToCapnp(service.downcast<kj::HttpService>());

	auto asyncIo = kj::setupAsyncIo();

	auto input = asyncIo.lowLevelProvider->wrapInputFd(0);
	auto output = asyncIo.lowLevelProvider->wrapOutputFd(1);
	CombinationStream stream(*input, *output);

	capnp::TwoPartyVatNetwork network(stream, capnp::rpc::twoparty::Side::SERVER);
	auto server = makeRpcServer(network, serviceClient);

	// Run forever, accepting connections and handling requests.
	kj::NEVER_DONE.wait(asyncIo.waitScope);
}