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

void proxy_main(int childIn, int childOut, pid_t childPid) {
	auto asyncIo = kj::setupAsyncIo();

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
	server.listenHttp(*listener).wait(asyncIo.waitScope);
	// TODO above wait never returns so below code is never executed
	int childStatus = 0;
	waitpid(childPid, &childStatus, 0);
	printf("childStatus: %d\n", childStatus);
	server.drain().wait(asyncIo.waitScope);
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
