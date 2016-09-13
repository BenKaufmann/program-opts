//
//  Copyright (c) Benjamin Kaufmann
//
//  This is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version. 
// 
//  This file is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see <http://www.gnu.org/licenses/>.
#include "catch.hpp"
#include <program_opts/value_store.h>
#include <program_opts/mapped_value.h>
#include <program_opts/typed_value.h>
#include <string.h>
#include <memory>
#include <map>
namespace ProgramOptions {
namespace Test {
namespace Po = ProgramOptions;
struct Counted {
	static int count;
	int parsed;
	Counted() : parsed(0) { ++count; }
	~Counted() { --count; }
	Counted(const Counted& o) : parsed(o.parsed) { ++count; }
	static bool parse(const std::string&, Counted& out) {
		++out.parsed;
		return true;
	}
};

int Counted::count = 0;
struct Notified {
	Notified() : notifications(0), reused(0), loc(0), ret(false) {}
	~Notified() { delete loc; }
	static bool notifyInt(Notified* this_, const std::string& name, const int* loc) {
		++this_->notifications;
		this_->opts.insert(std::map<std::string, int>::value_type(name, *loc));
		if (this_->loc == loc) {
			++this_->reused;
		}
		if (this_->ret) {
			this_->loc = loc;
		}
		return this_->ret;
	}
	static bool notifyUntyped(Notified* this_, const std::string& name, const std::string& value) {
		++this_->notifications;
		int temp;
		if (Po::string_cast<int>(value, temp)) {
			this_->opts[name] = temp;
			return true;
		}
		return false;
	}
	std::map<std::string, int> opts;
	int notifications, reused;
	const int* loc;
	bool ret;
};

TEST_CASE("Test value store", "[value]") {
	SECTION("default is empty") {
		Po::ValueStore x;
		REQUIRE(x.empty());
	}
	SECTION("values are copied into store") {
		Po::ValueStore x;
		Po::ValueStore y((Counted()));
		x = y;
		REQUIRE(Counted::count == 2);
	}
	SECTION("store supports swap") {
		Po::ValueStore x((Counted())), y((Counted()));
		Po::value_cast<Counted>(x).parsed = 0;
		Po::value_cast<Counted>(y).parsed = 10;
		x.swap(y);
		REQUIRE(Po::value_cast<Counted>(x).parsed == 10);
		REQUIRE(Po::value_cast<Counted>(y).parsed == 0);
		x.clear();
		REQUIRE(Po::unsafe_value_cast<Counted>(&x) == 0);
	}
	REQUIRE(Counted::count == 0);
}

TEST_CASE("Test flag", "[value]") {
	bool loud;
	SECTION("check properties") {
		std::auto_ptr<Po::Value> loudFlag(Po::flag(loud));
		REQUIRE(loudFlag->isImplicit() == true);
		REQUIRE(loudFlag->isFlag() == true);
		REQUIRE(strcmp(loudFlag->implicit(), "1") == 0);
	}
	SECTION("default parser stores true") {
		std::auto_ptr<Po::Value> loudFlag(Po::flag(loud));
		REQUIRE((loudFlag->parse("", "") && loud == true));
		loud = false;
		loudFlag->parse("", "on");
		REQUIRE(loud == true);
	}
	SECTION("alternative parser can store false") {
		std::auto_ptr<Po::Value> quietFlag(Po::flag(loud, Po::store_false));
		REQUIRE((quietFlag->parse("", "") && loud == false));
		quietFlag->parse("", "off");
		REQUIRE(loud == true);
	}
}

TEST_CASE("Test storeTo", "[value]") {
	int x; bool y;
	std::auto_ptr<Po::Value> v1(Po::storeTo(x));
	std::auto_ptr<Po::Value> v2(Po::flag(y));
	SECTION("store int") {
		REQUIRE(v1->parse("", "22"));
		REQUIRE(x == 22);
	}
	SECTION("fail on invalid type") {
		x = 99;
		REQUIRE(!v1->parse("", "ab"));
		REQUIRE(x == 99);
	}
	SECTION("init with state") {
		std::auto_ptr<Po::Value> v(Po::storeTo(x)->state(Po::Value::value_defaulted));
		REQUIRE(v->state() == Po::Value::value_defaulted);
		REQUIRE((v2->state() == Po::Value::value_unassigned && v2->isImplicit() && v2->isFlag()));
	}
	SECTION("parse as default") {
		REQUIRE(v2->parse("", "off", Po::Value::value_defaulted));
		REQUIRE(v2->state() == Po::Value::value_defaulted);
	}
	SECTION("parse bool as implicit") {
		REQUIRE(v2->parse("", ""));
		REQUIRE(y == true);
		v2->implicit("0");
		REQUIRE(v2->parse("", ""));
		REQUIRE(y == false);
	}
	SECTION("parse int as implicit") {
		v1->implicit(LIT_TO_STRING(102));
		REQUIRE(v1->isImplicit());
		REQUIRE((v1->parse("", "") && x == 102));
	}

	SECTION("test custom parser") {
		Counted c;
		std::auto_ptr<Po::Value> vc(Po::storeTo(c, &Counted::parse)->implicit(""));
		REQUIRE(vc->parse("", ""));
		REQUIRE(c.parsed == 1);
	}
}

TEST_CASE("Test custom value", "[value]") {
	Notified n;
	SECTION("with typed value creation") {
		std::auto_ptr<Po::Value> v(Po::notify<int>(&n, &Notified::notifyInt));
		v->parse("foo", "123");
		n.ret = true;
		v->parse("bar", "342");
		v->parse("jojo", "999");
		REQUIRE(n.notifications == 3);
		REQUIRE(n.reused == 1);
		REQUIRE(n.opts["foo"] == 123);
		REQUIRE(n.opts["bar"] == 342);
		REQUIRE(*n.loc == 999);
	}
	SECTION("with untyped value") {
		std::auto_ptr<Po::Value> v(Po::notify(&n, &Notified::notifyUntyped));
		REQUIRE(v->parse("foo", "123"));
		REQUIRE(v->parse("bar", "342"));
		REQUIRE(v->parse("jojo", "999"));
		REQUIRE(!v->parse("kaputt", "x12"));
		REQUIRE(n.reused == 0);
		REQUIRE(n.notifications == 4);
		REQUIRE(n.opts["foo"] == 123);
		REQUIRE(n.opts["bar"] == 342);
		REQUIRE(n.opts["jojo"] == 999);
		REQUIRE(n.opts.count("kaputt") == 0);
	}
}
TEST_CASE("Test mapped value", "[value]") {
	Po::ValueMap vm;
	std::auto_ptr<Po::Value> v1(Po::store<int>(vm));
	std::auto_ptr<Po::Value> v2(Po::store<double>(vm));
	std::auto_ptr<Po::Value> v3(Po::flag(vm));
	v1->parse("foo", "22");
	v2->parse("bar", "99.2");
	v3->parse("help", "false");
	REQUIRE(Po::value_cast<int>(vm["foo"]) == 22);
	REQUIRE(Po::value_cast<double>(vm["bar"]) == 99.2);
	REQUIRE(Po::value_cast<bool>(vm["help"]) == false);

	v1->parse("foo", "27");
	REQUIRE(Po::value_cast<int>(vm["foo"]) == 27);
}
struct Color { enum Value { RED = 2, GREEN = 10, BLUE = 20 }; };
struct Mode  { enum Value { DEF, IMP, EXP }; };
TEST_CASE("Test enum value", "[value]") {
	int x;
	Mode::Value y;

	std::auto_ptr<Po::Value> v1(Po::storeTo(x, Po::values<Color::Value>()
		("Red", Color::RED)
		("Green", Color::GREEN)
		("Blue", Color::BLUE)));

	std::auto_ptr<Po::Value> v2(Po::storeTo(y, Po::values<Mode::Value>()
		("Default", Mode::DEF)
		("Implicit", Mode::IMP)
		("Explicit", Mode::EXP)));

	REQUIRE((v1->parse("", "Red") && x == 2));
	REQUIRE((v1->parse("", "GREEN") && x == Color::GREEN));
	REQUIRE(!v1->parse("", "Blu"));

	REQUIRE((v2->parse("", "Implicit") && y == Mode::IMP));
}

}}
