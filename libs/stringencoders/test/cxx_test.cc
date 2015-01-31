/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */

#include <cstdlib>
#include <string>
using std::string;

#include <iostream>
using std::cerr;

#include "modp_b2.h"
#include "modp_b16.h"
#include "modp_b85.h"
#include "modp_b64.h"
#include "modp_b64w.h"
#include "modp_burl.h"
#include "modp_bjavascript.h"
#include "modp_ascii.h"

using namespace modp;

#define WHERE(A) A << "[" << __FILE__ << ":" << __LINE__ << "] "

static void test_b2()
{
    string orig("this is a test");
    string s(orig);
    b2_encode(s);
    b2_decode(s);
    if (s != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << s << "\n";
        exit(1);
    }

    s = "1";
    b2_decode(s);
    if (!s.empty()) {
        WHERE(cerr) << "Expected decode to be empty\n";
        exit(1);
    }
}

static void test_b2_const()
{
    const string orig("this is a test");
    const string s(b2_encode(orig));
    string result = b2_decode(s);
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }

    const string badstr("1");
    result = b2_decode(badstr);
    if (!result.empty()) {
        WHERE(cerr) << "Expected decode to be empty\n";
        exit(1);
    }
}

static void test_b16()
{
    string orig("this is a test");
    string expected("7468697320697320612074657374");
    string s(orig);
    b16_encode(s);
    if (s.size() != expected.size()) {
        WHERE(cerr) << "Bad Size.  Expected " << expected.size() << ", recieved " << s.size() << "\n";
        exit(1);
    }
    if (s != expected) {
        WHERE(cerr) << "Expected " << expected << ", recieved " << s << "\n";
        exit(1);
    }    
    b16_decode(s);
    if (s != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved '" << s << "'\n";
        exit(1);
    }

    // test bad input
    s = "1";
    b16_decode(s);
    if (!s.empty()) {
        WHERE(cerr) << "Expected decode to be empty\n";
        exit(1);
    }
}

static void test_b16_const()
{
    const string orig("this is a test");
    const string s(b16_encode(orig));
    string result = b16_decode(s);
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }

    // test bad input
    const string badstr("1");
    result = b16_decode(badstr);
    if (!result.empty()) {
        WHERE(cerr) << "Expected decode to be empty\n";
        exit(1);
    }
}

static void test_b64()
{
    string orig("this is a test");
    string s(orig);
    b64_encode(s);
    if (s == orig) {
        WHERE(cerr) << "something wrong\n";
        exit(1);
    }

    b64_decode(s);
    if (s != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << s << "\n";
        exit(1);
    }

    // test bad decode length
    s = "a";
    b64_decode(s);
    if (!s.empty()) {
        WHERE(cerr) << "Expected decode output to be empty\n";
        exit(1);
    }
}

static void test_b64w()
{
    string orig("this is a test");
    string s(orig);
    b64w_encode(s);
    if (s == orig) {
        WHERE(cerr) << "something wrong\n";
        exit(1);
    }

    b64w_decode(s);
    if (s != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << s << "\n";
        exit(1);
    }

    // test bad decode length
    s = "a";
    b64w_decode(s);
    if (!s.empty()) {
        WHERE(cerr) << "Expected decode output to be empty\n";
        exit(1);
    }
}

static void test_b64_const()
{
    const string orig("this is a test");
    const string s(b64_encode(orig));
    string result = b64_decode(s);
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }

    const string badstr("a");
    result = b64_decode(badstr);
    if (!result.empty()) {
        WHERE(cerr) << "Expected decode output to be empty\n";
        exit(1);
    }
}

static void test_b64w_const()
{
    const string orig("this is a test");
    const string s(b64w_encode(orig));
    string result = b64w_decode(s);
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }

    const string badstr("a");
    result = b64w_decode(badstr);
    if (!result.empty()) {
        WHERE(cerr) << "Expected decode output to be empty\n";
        exit(1);
    }
} 

static void test_b64_cstr()
{
    const char* orig = "this is a test";
    const string s(b64_encode(orig));
    string result = b64_decode(s);

    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }
    result = b64_decode(s.data(), s.size());
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }
    result = b64_decode(s.c_str());
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }
}

static void test_b64w_cstr()
{
    const char* orig = "this is a test";
    const string s(b64w_encode(orig));
    string result = b64w_decode(s);

    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }
    result = b64w_decode(s.data(), s.size());
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }
    result = b64w_decode(s.c_str());
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }
}
static void test_b85()
{
    // must be multiple of 4
    string orig("this is a test!!");
    string s(orig);
    b85_encode(s);
    if (s == orig) {
        WHERE(cerr) << "something wrong\n";
        exit(1);
    }
    b85_decode(s);
    if (s != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << s << "\n";
        exit(1);
    }

    // test non-multiple for decode
    string badstr;

    badstr = "abcd";
    b85_decode(badstr);
    if (!badstr.empty()) {
        WHERE(cerr) << "Expected decode output to be empty\n";
        exit(1);
    }

    badstr = "abcdef";
    b85_encode(badstr);
    if (!badstr.empty()) {
        WHERE(cerr) << "Expected encode output to be empty\n";
        exit(1);
    }
}

static void test_b85_const()
{
    const string orig("this is a test!!"); // must be multiple of 4
    const string s(b85_encode(orig));
    string result = b85_decode(s);
    if (result != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }

    const string badstr("abcdef");
    result = b85_encode(badstr);
    if (!result.empty()) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << result << "\n";
        exit(1);
    }
}

static void test_url()
{
    string orig("this is a test");
    string expected("this+is+a+test");
    string s(orig);
    url_encode(s);
    if (s != expected) {
        WHERE(cerr) << "Expected " << expected << ", recieved " << s << "\n";
        exit(1);
    }

    url_decode(s);
    if (s != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << s << "\n";
        exit(1);
    }

    s = "bad\n";
    url_decode(s);
    if (s.empty()) {
        WHERE(cerr) << "Expected empty decode\n";
        exit(1);
    }

}

static void test_url_const()
{
    const string orig("this is a test");
    const string expected("this+is+a+test");
    const string s1(url_encode(orig));
    if (s1 != expected) {
        WHERE(cerr) << "Expected " << expected << ", recieved " << s1 << "\n";
        exit(1);
    }

    const string s2(url_decode(s1));
    if (s2 != orig) {
        WHERE(cerr) << "Expected " << orig << ", recieved " << s2 << "\n";
        exit(1);
    }

    const string s3("bad\n");
    const string r3(url_decode(s3));
    if (r3.empty()) {
        WHERE(cerr) << "Expected empty decode\n";
        exit(1);
    }
}

static void test_url_cstr()
{
    const char* data = "this+is+a%20test";
    const string expected("this is a test");

    const string s1(url_decode(data));
    if (s1 != expected) {
        WHERE(cerr) << "Expected " << expected << ", recieved " << s1 << "\n";
        exit(1);
    }

    const string s2(url_decode(data, strlen(data)));
    if (s2 != expected) {
        WHERE(cerr) << "Expected " << expected << ", recieved " << s2 << "\n";
        exit(1);
    }
}

static void test_javascript()
{
    string orig("this \"is\' a test\n");
    string expected("this \\\"is\\' a test\\n");
    javascript_encode(orig);
    if (orig != expected) {
        WHERE(cerr) << "Expected: " << expected << "Received: " << orig << "\n";
        exit(1);
    }
}
static void test_javascript_const()
{
    const string orig("this \"is\' a test\n");
    const string expected("this \\\"is\\' a test\\n");
    string result = javascript_encode(orig);
    if (result != expected) {
        WHERE(cerr) << "Expected: " << expected << "Received: " << result << "\n";
        exit(1);
    }
}

static void test_ascii_copy()
{
    string orig;

    orig = "abcd123";
    toupper(orig);
    if (orig != "ABCD123") {
        WHERE(cerr) << "to upper copy failed: " << orig << " (size=" << orig.size() << ")\n";
        exit(1);
    }

    orig = "ABCD123";
    tolower(orig);
    if (orig != "abcd123") {
        WHERE(cerr) << "to lower copy failed: " << orig << " (size=" << orig.size() << ")\n";
        exit(1);
    }
}

int main()
{
    test_b2();
    test_b2_const();
    test_b16();
    test_b16_const();
    test_b64();
    test_b64_const();
    test_b64_cstr();
    test_b64w();
    test_b64w_const();
    test_b64w_cstr();
    test_b85();
    test_b85_const();
    test_url();
    test_url_const();
    test_url_cstr();
    test_javascript();
    test_javascript_const();
    test_ascii_copy();

    return 0;
}
