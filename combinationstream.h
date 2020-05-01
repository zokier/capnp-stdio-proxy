#pragma once

#include <kj/async-io.h>

class CombinationStream : public kj::AsyncIoStream {
public:
	CombinationStream(kj::AsyncInputStream &input, kj::AsyncOutputStream &output);
	virtual kj::Promise<void> write(const void* buf, size_t size);
	virtual kj::Promise<void> write(kj::ArrayPtr<const kj::ArrayPtr<const unsigned char>> pieces);
	virtual kj::Promise<void> whenWriteDisconnected();
	virtual kj::Promise<size_t> tryRead(void* buffer, size_t minBytes, size_t maxBytes);
	virtual void shutdownWrite();
private:
	kj::AsyncInputStream &input;
	kj::AsyncOutputStream &output;
};
