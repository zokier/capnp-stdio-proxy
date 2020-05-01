#include "combinationstream.h"


CombinationStream::CombinationStream(kj::AsyncInputStream &input, kj::AsyncOutputStream &output)
	: input(input), output(output)
{
}

kj::Promise<void> CombinationStream::write(const void* buf, size_t size) {
	return output.write(buf, size);
}

kj::Promise<void> CombinationStream::write(kj::ArrayPtr<const kj::ArrayPtr<const unsigned char>> pieces) {
	return output.write(pieces);
}

kj::Promise<void> CombinationStream::whenWriteDisconnected() {
	return output.whenWriteDisconnected();
}

kj::Promise<size_t> CombinationStream::tryRead(void* buffer, size_t minBytes, size_t maxBytes) {
	return input.tryRead(buffer, minBytes, maxBytes);
}

void CombinationStream::shutdownWrite() {
}
