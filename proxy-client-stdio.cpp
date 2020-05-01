#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/memory.h>
#include <capnp/rpc.h>
#include <capnp/rpc-twoparty.h>
#include <capnp/compat/http-over-capnp.capnp.h>
#include <capnp/compat/http-over-capnp.h>

#include "combinationstream.h"

class LoggingErrorHandler : public kj::TaskSet::ErrorHandler {
public:
	virtual void taskFailed(kj::Exception&& exception) {
		printf("error\n");
	}
};

void proxy_main(int childIn, int childOut, pid_t childPid) {
	auto asyncIo = kj::setupAsyncIo();
	LoggingErrorHandler errorHandler;
	kj::TaskSet taskSet(errorHandler);

	auto pipeThread = asyncIo.provider->newPipeThread(
		[childPid](kj::AsyncIoProvider& provider, kj::AsyncIoStream& stream, kj::WaitScope& scope) {
			int childStatus = 0;
			waitpid(childPid, &childStatus, 0);
			char buf[sizeof(int)];
			memcpy(buf, &childStatus, sizeof(int));
			stream.write(buf, sizeof(int)).wait(scope);
		}
	);

	auto input = asyncIo.lowLevelProvider->wrapInputFd(childOut);
	auto output = asyncIo.lowLevelProvider->wrapOutputFd(childIn);
	CombinationStream stream(*input, *output);
	capnp::TwoPartyClient client(stream);

	capnp::HttpService::Client cap = client.bootstrap().castAs<capnp::HttpService::Client>();

	kj::HttpHeaderTable::Builder headerTableBuilder;
	capnp::ByteStreamFactory streamFactory;
	capnp::HttpOverCapnpFactory hocFactory(streamFactory, headerTableBuilder);
	auto service = hocFactory.capnpToKj(cap);

	auto& timer = asyncIo.provider->getTimer();
	auto headerTable = kj::HttpHeaderTable::Builder().build();
	kj::HttpServer server(timer, *headerTable, *service);
	auto listener = asyncIo.provider->getNetwork().parseAddress("127.0.0.1", 8080).then(
			[](kj::Own<kj::NetworkAddress> addr)
			{
			return addr->listen();
			})
	.wait(asyncIo.waitScope);

	taskSet.add(pipeThread.pipe->readAllBytes().then([&server,&taskSet](kj::Array<kj::byte> buf) -> kj::Promise<void> {
		if (buf.size() != sizeof(int)) {
			printf("unexpected buf len: %lu\n", buf.size());
			return kj::READY_NOW;
		}
		int childStatus = 0;
		//this memcpy maybe ub???
		memcpy(&childStatus, buf.begin(), sizeof(int));
		printf("childStatus: %d\n", childStatus);
		return server.drain();
	}).eagerlyEvaluate(nullptr));

	taskSet.add(server.listenHttp(*listener));

	taskSet.onEmpty().wait(asyncIo.waitScope);
	printf("taskset returned\n");
}

int main(int argc, char** argv) {
	if (argc < 2) {
		return -1;
	}
	int stdinFd[2];
	int stdoutFd[2];
	pipe(stdinFd);
	pipe(stdoutFd);
	pid_t childPid = fork();
	if (childPid == 0) {
		close(stdinFd[1]);
		close(stdoutFd[0]);
		dup2(stdinFd[0], 0);
		dup2(stdoutFd[1], 1);
		return execvp(argv[1], &argv[1]);
	} else {
		close(stdinFd[0]);
		close(stdoutFd[1]);
		proxy_main(stdinFd[1], stdoutFd[0], childPid);
	}
}
