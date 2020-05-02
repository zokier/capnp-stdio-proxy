#include <kj/async-io.h>
#include <capnp/ez-rpc.h>
#include <capnp/compat/http-over-capnp.capnp.h>
#include <capnp/compat/byte-stream.h>
#include <iostream>
#include <stdio.h>

class ClientRequestContextImpl : public capnp::HttpService::ClientRequestContext::Server {
public:
	ClientRequestContextImpl(kj::Own<kj::AsyncOutputStream> kjStream);

	virtual ::kj::Promise<void> startResponse(StartResponseContext context);
private:
	capnp::ByteStream::Client byteStream;
};

ClientRequestContextImpl::ClientRequestContextImpl(kj::Own<kj::AsyncOutputStream> kjStream) 
	: byteStream(capnp::ByteStreamFactory().kjToCapnp(kj::mv(kjStream)))
{
}

::kj::Promise<void> ClientRequestContextImpl::startResponse(StartResponseContext context) {
	auto response = context.getParams().getResponse();
	std::cout << response.getStatusCode() << std::endl;
	context.getResults().setBody(this->byteStream);
	return kj::READY_NOW;
}

void printStream(kj::AsyncIoProvider& provider, kj::AsyncIoStream& stream, kj::WaitScope& scope) {
	stream.readAllText().then([] (kj::String text) {
		puts(text.cStr());
	}).wait(scope);
}

int main(int, const char**) {
	// Set up the EzRpcClient, connecting to the server on port
	// 5923 unless a different port was specified by the user.
	capnp::EzRpcClient client("127.0.0.1", 5923);
	auto& waitScope = client.getWaitScope();

	// Request the bootstrap capability from the server.
	capnp::HttpService::Client cap = client.getMain<capnp::HttpService>();

	// Make a call to the capability.
	auto request = cap.startRequestRequest();
	request.getRequest().setMethod(capnp::HttpMethod::GET);
	request.getRequest().getBodySize().setFixed(0);
	auto pipeThread = client.getIoProvider().newPipeThread(printStream);
	auto myCtx = kj::heap<ClientRequestContextImpl>(kj::mv(pipeThread.pipe));
	request.setContext(capnp::HttpService::ClientRequestContext::Client(myCtx.downcast<capnp::HttpService::ClientRequestContext::Server>()));
	auto promise = request.send();

	// Wait for the result.  This is the only line that blocks.
	auto response = promise.wait(waitScope);

	// All done.
	return 0;
}
