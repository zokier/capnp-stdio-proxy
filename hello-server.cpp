#include <signal.h>
#include <kj/compat/http.h>
#include <kj/async-io.h>
#include <kj/memory.h>
#include <capnp/compat/http-over-capnp.h>
#include <capnp/ez-rpc.h>

const char hello_msg[] = "hello world\n";

class HelloService : public kj::HttpService {
public:
	HelloService(kj::Own<kj::PromiseFulfiller<void>> fulfiller) : fulfiller(kj::mv(fulfiller)) {}
	virtual kj::Promise<void> request(kj::HttpMethod, kj::StringPtr, const kj::HttpHeaders&, kj::AsyncInputStream&, kj::HttpService::Response&);
private:
	kj::Promise<void> get_hello(const kj::HttpHeaders&, kj::AsyncInputStream&, kj::HttpService::Response&);
	kj::Promise<void> get_exit(const kj::HttpHeaders&, kj::AsyncInputStream&, kj::HttpService::Response&);
	kj::Own<kj::PromiseFulfiller<void>> fulfiller;

};

kj::Promise<void> HelloService::get_exit(const kj::HttpHeaders&, kj::AsyncInputStream&, kj::HttpService::Response& response) {
	auto headerTable = kj::HttpHeaderTable::Builder().build();
	kj::HttpHeaders headers(*headerTable);
	return response.sendError(200, "OK", headers).then([&]() {
		this->fulfiller->fulfill();
	});
}

kj::Promise<void> HelloService::get_hello(const kj::HttpHeaders&, kj::AsyncInputStream&, kj::HttpService::Response& response) {
	auto headerTableBuilder = kj::HttpHeaderTable::Builder();
	auto contentType = headerTableBuilder.add("Content-type");
	auto headerTable = headerTableBuilder.build();
	kj::HttpHeaders headers(*headerTable);
	headers.set(contentType, "text/plain");
	auto os = response.send(200, "OK", headers);
	return os->write(hello_msg, sizeof(hello_msg)-1);
}

kj::Promise <void> HelloService::request(kj::HttpMethod method, kj::StringPtr url,
		const kj::HttpHeaders& headers, kj::AsyncInputStream& inputStream,
		kj::HttpService::Response& response) {
	if (url == "/hello"_kj) {
		return get_hello(headers, inputStream, response);
	} else if(url == "/exit"_kj) {
		return get_exit(headers, inputStream, response);
	} else {
		auto headerTable = kj::HttpHeaderTable::Builder().build();
		kj::HttpHeaders headers(*headerTable);
		return response.sendError(404, "Not found", headers);
	}
}

int main(int,char**) {
	auto promiseAndFulfiller = kj::newPromiseAndFulfiller<void>();
	kj::Own<HelloService> service = kj::heap<HelloService>(kj::mv(promiseAndFulfiller.fulfiller));
	kj::HttpHeaderTable::Builder headerTableBuilder;
	capnp::ByteStreamFactory streamFactory;
	capnp::HttpOverCapnpFactory hocFactory(streamFactory, headerTableBuilder);
	auto serviceClient = hocFactory.kjToCapnp(service.downcast<kj::HttpService>());

	capnp::EzRpcServer server(serviceClient, "127.0.0.1", 5923);
	auto& waitScope = server.getWaitScope();

	// Run forever, accepting connections and handling requests.
	promiseAndFulfiller.promise.wait(waitScope);
}
