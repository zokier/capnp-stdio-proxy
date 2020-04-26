#include <signal.h>
#include <kj/compat/http.h>
#include <kj/async-io.h>
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
	auto asyncIo = kj::setupAsyncIo();
	auto& timer = asyncIo.provider->getTimer();
	HelloService service;
	auto headerTable = kj::HttpHeaderTable::Builder().build();
	kj::HttpServer server(timer, *headerTable, service);
	auto listener = asyncIo.provider->getNetwork().parseAddress("127.0.0.1", 8080).then(
			[](kj::Own<kj::NetworkAddress> addr)
			{
				return addr->listen();
			})
	.wait(asyncIo.waitScope);
	server.listenHttp(*listener).wait(asyncIo.waitScope);
}
