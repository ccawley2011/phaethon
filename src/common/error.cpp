/* Phaethon - A FLOSS resource explorer for BioWare's Aurora engine games
 *
 * Phaethon is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * Phaethon is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * Phaethon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Phaethon. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  Basic exceptions to throw.
 */

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "src/common/error.h"
#include "src/common/util.h"

namespace Common {

StackException::StackException(const char *s, ...) {
	char buf[STRINGBUFLEN];
	va_list va;

	va_start(va, s);
	vsnprintf(buf, STRINGBUFLEN, s, va);
	va_end(va);

	_stack.push(buf);
}

StackException::StackException(const std::exception &e) {
	add(e);
}

void StackException::add(const char *s, ...) {
	char buf[STRINGBUFLEN];
	va_list va;

	va_start(va, s);
	vsnprintf(buf, STRINGBUFLEN, s, va);
	va_end(va);

	_stack.push(buf);
}

void StackException::add(const StackException &e) {
	Stack orig = e._stack;

	Stack reverse;
	while (!orig.empty()) {
		reverse.push(orig.top());
		orig.pop();
	}

	while (!reverse.empty()) {
		_stack.push(reverse.top());
		reverse.pop();
	}
}

void StackException::add(const std::exception &e) {
	add("%s", e.what());
}

const char *StackException::what() const throw() {
	if (_stack.empty())
		return "";

	return _stack.top().c_str();
}

bool StackException::empty() const {
	return _stack.empty();
}

StackException::Stack &StackException::getStack() {
	return _stack;
}


const Exception kOpenError("Can't open file");
const Exception kReadError("Read error");
const Exception kSeekError("Seek error");
const Exception kWriteError("Write error");


void printException(Exception &e, const UString &prefix) {
	try {
		Exception::Stack &stack = e.getStack();

		if (stack.empty()) {
			status("FATAL ERROR");
			return;
		}

		status("%s%s", prefix.c_str(), stack.top().c_str());

		stack.pop();

		while (!stack.empty()) {
			status("    Because: %s", stack.top().c_str());
			stack.pop();
		}
	} catch (...) {
		status("FATAL ERROR: Exception while printing exception stack");
		std::exit(1);
	}
}

} // End of namespace Common
